/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 *
 * -=- Robust Distributed System Nucleus (rDSN) -=-
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Description:
 *     replication learning process
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#include "replica.h"
#include "mutation.h"
#include "mutation_log.h"
#include "replica_stub.h"
#include "replica/duplication/replica_duplicator_manager.h"

#include <dsn/utility/filesystem.h>
#include <dsn/dist/replication/replication_app_base.h>
#include <dsn/dist/fmt_logging.h>

namespace dsn {
namespace replication {

void replica::init_cluster_learn(uint64_t signature,
                                 configuration_update_request &proposal) // todo 可能不需要这个参数
{
    _checker.only_one_thread_access(); // todo 依然必须在replication线程池，并保持单线程

    if (!check_cluster_learner_state(proposal)) {
        derror_replica(""); // todo
        return;
    }

    _duplicating = proposal.info.duplicating;
    _duplication_remote_node = proposal.duplication_config.primary;

    derror_replica("process add cluster learner, remote = {}, "
                   "last_committed_decree = {} vs {}, duplicating = {}",
                   proposal.duplication_config.primary.to_string(),
                   proposal.duplication_config.last_committed_decree,
                   last_committed_decree(),
                   proposal.info.duplicating);

    start_cluster_learn(signature);
}

bool replica::check_cluster_learner_state(configuration_update_request &proposal)
{
    if (_app_duplication_status != app_duplication_status::ClusterLearning) {
        derror_replica("app_duplication_status must be at {}",
                       enum_to_string(app_duplication_status::ClusterLearning));
        return false;
    }

    if (!proposal.info.duplicating) {
        derror_replica("app must be at duplicating when start cluster learning");
        return false;
    }

    if (status() != partition_status::PS_PRIMARY) { // todo 需要支持Primary done
        derror_replica("state is not potential secondary or duplicating but "
                       "replica_learn={}，cluster_learn={}, skip "
                       "learning with signature",
                       enum_to_string(status()),
                       cluster_learn_status());
        return false;
    }

    if (_learner_states.learning_round_is_running) {
        derror_replica("previous learning is still running, skip this learnning");
        return false;
    }

    return true;
}

void replica::start_cluster_learn(uint64_t signature)
{
    if (_learner_states.learning_status >= learner_status::LearningWithPrepare) {
        derror_replica("app duplication is {}, step to next stage {} for learning status is {}",
                       cluster_learn_status(),
                       enum_to_string(app_duplication_status::ClusterLearningSucceeded),
                       enum_to_string(_learner_states.learning_status));
        _app_duplication_status = app_duplication_status::ClusterLearningSucceeded;
        return;
    }

    if (signature == invalid_signature) {
        if (!_learner_states.cleanup(false)) {
            derror_replica("previous learning with signature {} and can't stop",
                           _learner_states.learning_version);
            return;
        }
        init_learner_status(_learner_states.learning_version++,
                            _duplication_remote_node,
                            RPC_LEARN, // todo
                            std::bind(&replica::start_learn,// NOLINT
                                      this,
                                      std::placeholders::_1,
                                      std::placeholders::_2));
    } else {
        handle_current_learner_status(_duplication_remote_node,
                                      RPC_LEARN, // todo
                                      std::bind(&replica::start_learn,// NOLINT
                                                this,
                                                std::placeholders::_1,
                                                std::placeholders::_2));
    }
}

void replica::init_learner_status(
    uint64_t signature,
    const dsn::rpc_address &target,
    dsn::task_code code,
    const std::function<void(const rpc_address &, dsn::task_code)> &start_learn)
{
    _learner_states.learning_version = signature;
    _learner_states.learning_start_ts_ns = dsn_now_ns();
    _learner_states.learning_status = learner_status::LearningWithoutPrepare;
    _prepare_list->truncate(_app->last_committed_decree());

    start_learn(target, code);
}

void replica::handle_current_learner_status(
    const dsn::rpc_address &target,
    dsn::task_code code,
    const std::function<void(const rpc_address &, dsn::task_code)> &start_learn)
{
    switch (_learner_states.learning_status) {
    case learner_status::LearningWithoutPrepare:
    case learner_status::LearningFailed:
        break;
    case learner_status::LearningWithPrepare:
        if (!catch_up_with_latest_decree()) {
            derror_replica("can't have catch up with latest decree and wait next round");
            return;
        }
    case learner_status::LearningSucceeded: {
        check_state_completeness(); // todo 等待重构
        notify_learn_completion();  // todo 等待重构
        return;
    }
    default:
        dassert_replica(false,
                        "invalid learner_status, status = %s",
                        enum_to_string(_learner_states.learning_status));
    }

    start_learn(target, code);
}

bool replica::catch_up_with_latest_decree()
{
    // check missing state due to _app->flush to checkpoint the learned state
    auto ac = _app->last_committed_decree();
    auto pc = _prepare_list->last_committed_decree();
    if (ac > pc) {
        // missing commits
        // missed ones are covered by prepare list
        if (_prepare_list->count() > 0 && ac + 1 >= _prepare_list->min_decree()) {
            for (auto d = ac + 1; d <= pc; d++) {
                auto mu = _prepare_list->get_mutation_by_decree(d);
                dassert(nullptr != mu, "mutation must not be nullptr, decree = %" PRId64 "", d);
                auto err = _app->apply_mutation(mu);
                if (ERR_OK != err) {
                    handle_learning_error(err, true);
                    return false;
                }
            }
        }
        // missed ones need to be loaded via private logs
        else {
            _learner_states.learning_round_is_running = true;
            _learner_states.catchup_with_private_log_task = tasking::create_task(
                LPC_CATCHUP_WITH_PRIVATE_LOGS,
                &_tracker,
                [this]() {
                    this->catch_up_with_private_logs(
                        partition_status::PS_POTENTIAL_SECONDARY); // todo 等待重构
                },
                get_gpid().thread_hash());
            _learner_states.catchup_with_private_log_task->enqueue();
            return false; // incomplete
        }
    }
    // convert to success if app state and prepare list is connected
    _learner_states.learning_status = learner_status::LearningSucceeded;
    return true;
}

void replica::start_learn(const dsn::rpc_address &target, dsn::task_code code)
{
    learn_request request;
    request.pid = get_gpid();
    request.__set_max_gced_decree(get_max_gced_decree_for_learn());
    request.__set_duplicating(_duplicating);
    request.last_committed_decree_in_app = _app->last_committed_decree();
    request.last_committed_decree_in_prepare_list = _prepare_list->last_committed_decree();
    request.learner = _stub->_primary_address;
    request.signature = _learner_states.learning_version;
    _app->prepare_get_checkpoint(request.app_specific_learn_request);

    derror("%s: init_learn[%016" PRIx64 "]: learnee = %s, learn_duration = %" PRIu64
           " ms, max_gced_decree = %" PRId64 ", local_committed_decree = %" PRId64 ", "
           "app_committed_decree = %" PRId64 ", app_durable_decree = %" PRId64
           ", current_learning_status = %s, total_copy_file_count = %" PRIu64
           ", total_copy_file_size = %" PRIu64 ", total_copy_buffer_size = %" PRIu64,
           name(),
           request.signature,
           _config.primary.to_string(),
           _learner_states.duration_ms(),
           request.max_gced_decree,
           last_committed_decree(),
           _app->last_committed_decree(),
           _app->last_durable_decree(),
           enum_to_string(_learner_states.learning_status),
           _learner_states.learning_copy_file_count,
           _learner_states.learning_copy_file_size,
           _learner_states.learning_copy_buffer_size);

    dsn::message_ex *msg = dsn::message_ex::create_request(code, 0, get_gpid().thread_hash());
    dsn::marshall(msg, request);
    _learner_states.learning_task =
        rpc::call(target, // TODO 这里根据是集群间Learn还是集群内Learn传递相应地址 done
                  msg,
                  &_tracker,
                  [ this, req_cap = request ](error_code err, learn_response && resp) mutable {
                      on_learn_reply(err, std::move(req_cap), std::move(resp));
                  });
}

// learner
void replica::on_add_cluster_learner(configuration_update_request &proposal)
{
    if (_app_duplication_status == app_duplication_status::DuplicationIdle ||
        _app_duplication_status == app_duplication_status::ReplicaLearningSucceeded) {
        derror_replica("{} duplication status is {}, start new cluster learn work",
                       proposal.info.app_name,
                       cluster_learn_status());
        _app_duplication_status = app_duplication_status::ClusterLearning;
        init_cluster_learn(invalid_signature, proposal);
    } else if (_app_duplication_status == app_duplication_status::ClusterLearning ||
               _app_duplication_status == app_duplication_status::ReplicaLearning) {
        derror_replica("{} duplication status is {}, skip the proposal",
                       proposal.info.app_name,
                       cluster_learn_status());
        return;
    } else if (_app_duplication_status == app_duplication_status::ClusterLearningSucceeded) {
        derror_replica("{} duplication status is {}, step to next stage {}",
                       proposal.info.app_name,
                       cluster_learn_status(),
                       enum_to_string(app_duplication_status::ReplicaLearning));
        _app_duplication_status = app_duplication_status::ReplicaLearning;
        add_potential_secondary(proposal); // todo:
        // 当Primary挂掉后，当前只存在两个PotentialSecondary，
        // meta此时如何抉择？
    }
}

// learner
void replica::add_duplication_learner(const rpc_address &learner, uint64_t signature)
{
    auto it = _primary_states.learners.find(learner);
    if (it == _primary_states.learners.end()) {
        remote_learner_state state;
        state.prepare_start_decree = invalid_decree;
        state.timeout_task = nullptr; // TODO: add timer for learner task

        state.signature = signature; // TODO 这里的校验可能会有一些问题
        _primary_states.learners[learner] = state;
        _primary_states.statuses[learner] = partition_status::PS_PRIMARY;
    }
}

/****************************** Learnee *************************/
void replica::on_cluster_learn(dsn::message_ex *msg, const learn_request &request)
{
    _checker.only_one_thread_access();

    learn_response response;
    _primary_states.get_replica_config(partition_status::PS_POTENTIAL_SECONDARY, response.config);

    response.err = check_learnee_status(request);
    if (response.err != ERR_OK) {
        reply(msg, response);
        return;
    }

    int64_t local_committed_decree = last_committed_decree();
    const decree learn_start_decree = get_learn_start_decree(request);
    response.state.__set_learn_start_decree(learn_start_decree);
    bool delayed_replay_prepare_list = false;

    ddebug("%s: on_learn[%016" PRIx64 "]: learner = %s, remote_committed_decree = %" PRId64 ", "
           "remote_app_committed_decree = %" PRId64 ", local_committed_decree = %" PRId64 ", "
           "app_committed_decree = %" PRId64 ", app_durable_decree = %" PRId64 ", "
           "prepare_min_decree = %" PRId64
           ", prepare_list_count = %d, learn_start_decree = %" PRId64,
           name(),
           request.signature,
           request.learner.to_string(),
           request.last_committed_decree_in_prepare_list,
           request.last_committed_decree_in_app,
           local_committed_decree,
           _app->last_committed_decree(),
           _app->last_durable_decree(),
           _prepare_list->min_decree(),
           _prepare_list->count(),
           learn_start_decree);

    response.address = _stub->_primary_address;
    response.prepare_start_decree = invalid_decree;
    response.last_committed_decree = local_committed_decree;
    response.err = ERR_OK;

    // learn delta state or checkpoint
    bool should_learn_cache = prepare_cached_learn_state(request,
                                                         learn_start_decree,
                                                         local_committed_decree,
                                                         learner_state,
                                                         response,
                                                         delayed_replay_prepare_list);
    if (!should_learn_cache) {
        if (learn_start_decree > _app->last_durable_decree()) {
            ddebug("%s: on_learn[%016" PRIx64 "]: learner = %s, choose to learn private logs, "
                   "because learn_start_decree(%" PRId64 ") > _app->last_durable_decree(%" PRId64
                   ")",
                   name(),
                   request.signature,
                   request.learner.to_string(),
                   learn_start_decree,
                   _app->last_durable_decree());
            _private_log->get_learn_state(get_gpid(), learn_start_decree, response.state);
            response.type = learn_type::LT_LOG;
        } else if (_private_log->get_learn_state(get_gpid(), learn_start_decree, response.state)) {
            ddebug("%s: on_learn[%016" PRIx64 "]: learner = %s, choose to learn private logs, "
                   "because mutation_log::get_learn_state() returns true",
                   name(),
                   request.signature,
                   request.learner.to_string());
            response.type = learn_type::LT_LOG;
        } else if (learn_start_decree < request.last_committed_decree_in_app + 1) {
            ddebug("%s: on_learn[%016" PRIx64 "]: learner = %s, choose to learn private logs, "
                   "because learn_start_decree steps back for duplication",
                   name(),
                   request.signature,
                   request.learner.to_string());
            response.type = learn_type::LT_LOG;
        } else {
            ddebug("%s: on_learn[%016" PRIx64 "]: learner = %s, choose to learn app, "
                   "beacuse learn_start_decree(%" PRId64 ") <= _app->last_durable_decree(%" PRId64
                   "), "
                   "and mutation_log::get_learn_state() returns false",
                   name(),
                   request.signature,
                   request.learner.to_string(),
                   learn_start_decree,
                   _app->last_durable_decree());
            response.type = learn_type::LT_APP;
            response.state = learn_state();
        }

        if (response.type == learn_type::LT_LOG) {
            response.base_local_dir = _private_log->dir();
            if (response.state.files.size() > 0) {
                auto &last_file = response.state.files.back();
                if (last_file == learner_state.last_learn_log_file) {
                    ddebug(
                        "%s: on_learn[%016" PRIx64
                        "]: learner = %s, learn the same file %s repeatedly, hint to switch file",
                        name(),
                        request.signature,
                        request.learner.to_string(),
                        last_file.c_str());
                    _private_log->hint_switch_file();
                } else {
                    learner_state.last_learn_log_file = last_file;
                }
            }
            // it is safe to commit to last_committed_decree() now
            response.state.to_decree_included = last_committed_decree();
            ddebug("%s: on_learn[%016" PRIx64 "]: learner = %s, learn private logs succeed, "
                   "learned_meta_size = %u, learned_file_count = %u, "
                   "to_decree_included = %" PRId64,
                   name(),
                   request.signature,
                   request.learner.to_string(),
                   response.state.meta.length(),
                   static_cast<uint32_t>(response.state.files.size()),
                   response.state.to_decree_included);
        } else {
            ::dsn::error_code err = _app->get_checkpoint(
                learn_start_decree, request.app_specific_learn_request, response.state);

            if (err != ERR_OK) {
                response.err = ERR_GET_LEARN_STATE_FAILED;
                derror("%s: on_learn[%016" PRIx64
                       "]: learner = %s, get app checkpoint failed, error = %s",
                       name(),
                       request.signature,
                       request.learner.to_string(),
                       err.to_string());
            } else {
                response.base_local_dir = _app->data_dir();
                response.__set_replica_disk_tag(get_replica_disk_tag());
                ddebug(
                    "%s: on_learn[%016" PRIx64 "]: learner = %s, get app learn state succeed, "
                    "learned_meta_size = %u, learned_file_count = %u, learned_to_decree = %" PRId64,
                    name(),
                    request.signature,
                    request.learner.to_string(),
                    response.state.meta.length(),
                    static_cast<uint32_t>(response.state.files.size()),
                    response.state.to_decree_included);
            }
        }
    }

    for (auto &file : response.state.files) {
        file = file.substr(response.base_local_dir.length() + 1);
    }

    reply(msg, response);

    // the replayed prepare msg needs to be AFTER the learning response msg
    if (delayed_replay_prepare_list) {
        replay_prepare_list();
    }
}

error_code replica::check_learnee_status(const learn_request &request) {
    if (partition_status::PS_PRIMARY != status()) {
        return (partition_status::PS_INACTIVE == status() && _inactive_is_transient)
                       ? ERR_INACTIVE_STATE
                       : ERR_INVALID_STATE;
    }

    // TODO: learner machine has been down for a long time, and DDD MUST happened before
    // which leads to state lost. Now the lost state is back, what shall we do?
    if (request.last_committed_decree_in_app > last_prepared_decree()) {
        derror_replica("on_learn[{}]: learner = {}, learner state is newer than learnee, "
                       "learner_app_committed_decree = {}, local_committed_decree ={}, learn from "
                       "scratch",
                       request.signature,
                       request.learner.to_string(),
                       request.last_committed_decree_in_app,
                       last_committed_decree());
        *(decree *)&request.last_committed_decree_in_app = 0;
        return ERR_OK;
    }
        // mutations are previously committed already on learner (old primary)
        // this happens when the new primary does not commit the previously prepared mutations
        // yet, which it should do, so let's help it now.
    else if (request.last_committed_decree_in_app > last_committed_decree()) {
        derror_replica("on_learn[{}]: learner = {}, learner's last_committed_decree_in_app is "
                       "newer than learnee, learner_app_committed_decree = {}, "
                       "local_committed_decree = {}, commit local soft",
                       request.signature,
                       request.learner.to_string(),
                       request.last_committed_decree_in_app,
                       last_committed_decree());
        // we shouldn't commit mutations hard coz these mutations may preparing on another learner
        _prepare_list->commit(request.last_committed_decree_in_app, COMMIT_TO_DECREE_SOFT);

        if (request.last_committed_decree_in_app > last_committed_decree()) {
            derror("%s: on_learn[%016" PRIx64 "]: try to commit primary to %" PRId64
                       ", still less than learner(%s)'s committed decree(%" PRId64
                       "), wait mutations to be commitable",
                   name(),
                   request.signature,
                   last_committed_decree(),
                   request.learner.to_string(),
                   request.last_committed_decree_in_app);
            return ERR_INCONSISTENT_STATE;
        }
    }
    dassert(request.last_committed_decree_in_app <= last_committed_decree(),
            "%" PRId64 " VS %" PRId64 "",
            request.last_committed_decree_in_app,
            last_committed_decree());
}

} // namespace replication
} // namespace dsn
