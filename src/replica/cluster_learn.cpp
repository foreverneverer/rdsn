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

void replica::init_cluster_learn(uint64_t signature)
{
    _checker.only_one_thread_access();

    _stub->_counter_replicas_learning_recent_start_count->increment();
    _potential_secondary_states.learning_version = signature;
    _potential_secondary_states.learning_start_ts_ns = dsn_now_ns();
    _potential_secondary_states.learning_status = learner_status::LearningWithoutPrepare;

    _stub->_counter_replicas_learning_recent_round_start_count->increment();
    _potential_secondary_states.learning_round_is_running = true;

    learn_request request;
    request.pid = get_gpid();
    request.__set_max_gced_decree(get_max_gced_decree_for_learn());
    request.last_committed_decree_in_app = _app->last_committed_decree();
    request.last_committed_decree_in_prepare_list = _prepare_list->last_committed_decree();
    request.learner = _stub->_primary_address; // TODO 改成master的primary地址
    request.signature = _potential_secondary_states.learning_version;
    _app->prepare_get_checkpoint(request.app_specific_learn_request);


    dsn::message_ex *msg =
        dsn::message_ex::create_request(RPC_CLUSTER_LEARN, 0, get_gpid().thread_hash());
    dsn::marshall(msg, request);
    _potential_secondary_states.learning_task = rpc::call(
        _config.primary,
        msg,
        &_tracker,
        [ this, req_cap = std::move(request) ](error_code err, learn_response && resp) mutable {
            on_cluster_learn_reply(err, std::move(req_cap), std::move(resp));
        });
}

// ThreadPool: THREAD_POOL_CLUSTER_LEARN
decree replica::get_max_gced_decree_for_learn() const // on learner
{
    decree max_gced_decree_for_learn;

    decree plog_max_gced_decree = max_gced_decree_no_lock();
    decree first_learn_start = _potential_secondary_states.first_learn_start_decree;
    if (first_learn_start == invalid_decree) {
        // this is the first round of learn
        max_gced_decree_for_learn = plog_max_gced_decree;
    } else {
        if (plog_max_gced_decree < 0) {
            // The previously learned logs may still reside in learn_dir, and
            // the actual plog dir is empty. In this condition the logs in learn_dir
            // are taken as not-GCed.
            max_gced_decree_for_learn = first_learn_start - 1;
        } else {
            // The actual plog dir is not empty. Use the minimum.
            max_gced_decree_for_learn = std::min(plog_max_gced_decree, first_learn_start - 1);
        }
    }

    return max_gced_decree_for_learn;
}

/*virtual*/ decree replica::max_gced_decree_no_lock() const
{
    return _private_log->max_gced_decree_no_lock(get_gpid());
}

// ThreadPool: THREAD_POOL_CLUSTER_LEARN
decree replica::get_learn_start_decree(const learn_request &request) // on primary
{
    decree local_committed_decree = last_committed_decree();
    dcheck_le_replica(request.last_committed_decree_in_app, local_committed_decree);

    decree learn_start_decree_no_dup = request.last_committed_decree_in_app + 1;
    if (!is_duplicating()) {
        // fast path for no duplication case: only learn those that the learner is not having.
        return learn_start_decree_no_dup;
    }

    decree min_confirmed_decree = _duplication_mgr->min_confirmed_decree();

    // Learner should include the mutations not confirmed by meta server
    // so as to prevent data loss during duplication. For example, when
    // the confirmed+1 decree has been missing from plog, the learner
    // needs to learn back from it.
    //
    //                confirmed but missing
    //                    |
    // learner's plog: ======[--------------]
    //                       |              |
    //                      gced           committed
    //
    // In the above case, primary should return logs started from confirmed+1.

    decree learn_start_decree_for_dup = learn_start_decree_no_dup;
    if (min_confirmed_decree >= 0) {
        learn_start_decree_for_dup = min_confirmed_decree + 1;
    } else {
        // if the confirmed_decree is unsure, copy all the logs
        // TODO(wutao1): can we reduce the copy size?
        decree local_gced = max_gced_decree_no_lock();
        if (local_gced == invalid_decree) {
            // abnormal case
            dwarn_replica("no plog to be learned for duplication, continue as normal");
        } else {
            learn_start_decree_for_dup = local_gced + 1;
        }
    }

    decree learn_start_decree = learn_start_decree_no_dup;
    if (learn_start_decree_for_dup <= request.max_gced_decree ||
        request.max_gced_decree == invalid_decree) {
        // `request.max_gced_decree == invalid_decree` indicates the learner has no log,
        // see replica::get_max_gced_decree_for_learn for details.
        if (learn_start_decree_for_dup < learn_start_decree_no_dup) {
            learn_start_decree = learn_start_decree_for_dup;
            ddebug_replica("learn_start_decree steps back to {} to ensure learner having enough "
                           "logs for duplication [confirmed_decree={}, learner_gced_decree={}]",
                           learn_start_decree,
                           min_confirmed_decree,
                           request.max_gced_decree);
        }
    }
    dcheck_le_replica(learn_start_decree, local_committed_decree + 1);
    dcheck_gt_replica(learn_start_decree, 0); // learn_start_decree can never be invalid_decree
    return learn_start_decree;
}

void replica::on_cluster_learn(dsn::message_ex *msg, const learn_request &request)
{
    _checker.only_one_thread_access();

    learn_response response;
    auto it = _primary_states.learners.find(request.learner); // todo: 需要改造以存储remote learner
    if (it == _primary_states.learners.end()) {
        response.config.status = partition_status::PS_INACTIVE;
        response.err = ERR_OBJECT_NOT_FOUND;
        reply(msg, response);
        return;
    }

    remote_learner_state &learner_state = it->second;
    if (learner_state.signature != request.signature) {
        response.config.learner_signature = learner_state.signature;
        response.err = ERR_WRONG_CHECKSUM; // means invalid signature
        reply(msg, response);
        return;
    }

    // prepare learn_start_decree
    decree local_committed_decree = last_committed_decree();
    dassert(request.last_committed_decree_in_app <= local_committed_decree,
            "%" PRId64 " VS %" PRId64 "",
            request.last_committed_decree_in_app,
            local_committed_decree);

    const decree learn_start_decree = get_learn_start_decree(request);
    response.state.__set_learn_start_decree(learn_start_decree);
    bool delayed_replay_prepare_list = false;

    response.address = _stub->_primary_address; // 地址需要变更
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
            _private_log->get_learn_state(get_gpid(), learn_start_decree, response.state);
            response.type = learn_type::LT_LOG;
        } else if (_private_log->get_learn_state(get_gpid(), learn_start_decree, response.state)) {
            response.type = learn_type::LT_LOG;
        } else if (learn_start_decree < request.last_committed_decree_in_app + 1) {
            response.type = learn_type::LT_LOG;
        } else {
            response.type = learn_type::LT_APP;
            response.state = learn_state();
        }

        if (response.type == learn_type::LT_LOG) {
            response.base_local_dir = _private_log->dir();
            response.state.to_decree_included = last_committed_decree();
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
            }
        }
    } else {
        derror_replica("start cache, so break");
        return;
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

void replica::on_cluster_learn_reply(error_code err, learn_request &&req, learn_response &&resp)
{
    _checker.only_one_thread_access();
    dassert_replica(err == ERR_OK, "rpc failed");
    dassert_replica(resp.err == ERR_OK, "result failed");
    _potential_secondary_states.learning_copy_buffer_size += resp.state.meta.length();
    if (resp.type == learn_type::LT_APP) {
        if (++_stub->_learn_app_concurrent_count > _options->learn_app_max_concurrent_count) {
            --_stub->_learn_app_concurrent_count;
            _potential_secondary_states.learning_round_is_running = false;
            return;
        } else {
            _potential_secondary_states.learn_app_concurrent_count_increased = true;
        }
    }


    if (resp.prepare_start_decree != invalid_decree) {
        derror_replica("start copy cache and end the cluster learn");
        return;
    } else if (resp.state.files.size() > 0) {
        auto learn_dir = _app->learn_dir();
        utils::filesystem::remove_path(learn_dir);
        utils::filesystem::create_directory(learn_dir);

        if (!dsn::utils::filesystem::directory_exists(learn_dir)) {
            dassert_replica(false, "directory_exists");
        }

        bool high_priority = resp.type != learn_type::LT_APP;
        _potential_secondary_states.learn_remote_files_task = _stub->_nfs->copy_remote_files(
            resp.config.primary,
            resp.replica_disk_tag,
            resp.base_local_dir,
            resp.state.files,
            get_replica_disk_tag(),
            learn_dir,
            true, // overwrite
            high_priority,
            LPC_REPLICATION_COPY_REMOTE_FILES,
            &_tracker,
            [
              this,
              copy_start = _potential_secondary_states.duration_ms(),
              req_cap = std::move(req),
              resp_copy = resp
            ](error_code err, size_t sz) mutable {
                on_copy_remote_cluster_state_completed(
                    err, sz, copy_start, std::move(req_cap), std::move(resp_copy));
            });
    } else {
       dassert_replica(resp.state.files.size(), "must lager than 0");
    }
}

bool replica::prepare_cached_learn_state(const learn_request &request,
                                         decree learn_start_decree,
                                         decree local_committed_decree,
                                         /*out*/ remote_learner_state &learner_state,
                                         /*out*/ learn_response &response,
                                         /*out*/ bool &delayed_replay_prepare_list)
{
    // set prepare_start_decree when to-be-learn state is covered by prepare list,
    // note min_decree can be NOT present in prepare list when list.count == 0
    if (learn_start_decree > _prepare_list->min_decree() ||
        (learn_start_decree == _prepare_list->min_decree() && _prepare_list->count() > 0)) {
        if (learner_state.prepare_start_decree == invalid_decree) {
            // start from (last_committed_decree + 1)
            learner_state.prepare_start_decree = local_committed_decree + 1;

            cleanup_preparing_mutations(false);

            // the replayed prepare msg needs to be AFTER the learning response msg
            // to reduce probability that preparing messages arrive remote early than
            // learning response msg.
            delayed_replay_prepare_list = true;

            ddebug("%s: on_learn[%016" PRIx64
                   "]: learner = %s, set prepare_start_decree = %" PRId64,
                   name(),
                   request.signature,
                   request.learner.to_string(),
                   local_committed_decree + 1);
        }

        response.prepare_start_decree = learner_state.prepare_start_decree;
    } else {
        learner_state.prepare_start_decree = invalid_decree;
    }

    // only learn mutation cache in range of [learn_start_decree, prepare_start_decree),
    // in this case, the state on the PS should be contiguous (+ to-be-sent prepare list)
    if (response.prepare_start_decree != invalid_decree) {
        binary_writer writer;
        int count = 0;
        for (decree d = learn_start_decree; d < response.prepare_start_decree; d++) {
            auto mu = _prepare_list->get_mutation_by_decree(d);
            dassert(mu != nullptr, "mutation must not be nullptr, decree = %" PRId64 "", d);
            mu->write_to(writer, nullptr);
            count++;
        }
        response.type = learn_type::LT_CACHE;
        response.state.meta = writer.get_buffer();
        ddebug("%s: on_learn[%016" PRIx64 "]: learner = %s, learn mutation cache succeed, "
               "learn_start_decree = %" PRId64 ", prepare_start_decree = %" PRId64 ", "
               "learn_mutation_count = %d, learn_data_size = %d",
               name(),
               request.signature,
               request.learner.to_string(),
               learn_start_decree,
               response.prepare_start_decree,
               count,
               response.state.meta.length());
        return true;
    }
    return false;
}

void replica::on_copy_remote_cluster_state_completed(error_code err,
                                                     size_t size,
                                                     uint64_t copy_start_time,
                                                     learn_request &&req,
                                                     learn_response &&resp)
{
    decree old_prepared = last_prepared_decree();
    decree old_committed = last_committed_decree();
    decree old_app_committed = _app->last_committed_decree();
    decree old_app_durable = _app->last_durable_decree();


    if (resp.type == learn_type::LT_APP) {
        --_stub->_learn_app_concurrent_count;
        _potential_secondary_states.learn_app_concurrent_count_increased = false;
    }


    if (err != ERR_OK) {
        // do nothing
    } else if (_potential_secondary_states.learning_status == learner_status::LearningWithPrepare) {
        dassert(resp.type == learn_type::LT_CACHE,
                "invalid learn_type, type = %s",
                enum_to_string(resp.type));
    } else {
        dassert(resp.type == learn_type::LT_APP || resp.type == learn_type::LT_LOG,
                "invalid learn_type, type = %s",
                enum_to_string(resp.type));

        learn_state lstate;
        lstate.from_decree_excluded = resp.state.from_decree_excluded;
        lstate.to_decree_included = resp.state.to_decree_included;
        lstate.meta = resp.state.meta;
        if (resp.state.__isset.learn_start_decree) {
            lstate.__set_learn_start_decree(resp.state.learn_start_decree);
        }

        for (auto &f : resp.state.files) {
            std::string file = utils::filesystem::path_combine(_app->learn_dir(), f);
            lstate.files.push_back(file);
        }

        // apply app learning
        if (resp.type == learn_type::LT_APP) {
            err = _app->apply_checkpoint(replication_app_base::chkpt_apply_mode::learn, lstate);
        } else {
            err = apply_cluster_learned_state_from_private_log(lstate);
        }
        // reset prepare list to make it catch with app
        _prepare_list->reset(_app->last_committed_decree());
    }

    // if catch-up done, do flush to enable all learned state is durable
    if (err == ERR_OK && resp.prepare_start_decree != invalid_decree &&
        _app->last_committed_decree() + 1 >=
            _potential_secondary_states.learning_start_prepare_decree &&
        _app->last_committed_decree() > _app->last_durable_decree()) {
        err = background_sync_checkpoint();
        if (err == ERR_OK) {
            dassert(_app->last_committed_decree() == _app->last_durable_decree(),
                    "%" PRId64 " VS %" PRId64 "",
                    _app->last_committed_decree(),
                    _app->last_durable_decree());
        }
    }
}

void replica::on_learn_remote_cluster_state_completed(error_code err)
{
    _checker.only_one_thread_access();

    if (partition_status::PS_POTENTIAL_SECONDARY != status()) {
        dwarn("%s: on_learn_remote_state_completed[%016" PRIx64
              "]: learnee = %s, learn_duration = %" PRIu64 " ms, err = %s, "
              "the learner status is not PS_POTENTIAL_SECONDARY, but %s, ignore",
              name(),
              _potential_secondary_states.learning_version,
              _config.primary.to_string(),
              _potential_secondary_states.duration_ms(),
              err.to_string(),
              enum_to_string(status()));
        return;
    }

    ddebug("%s: on_learn_remote_state_completed[%016" PRIx64
           "]: learnee = %s, learn_duration = %" PRIu64 " ms, err = %s, "
           "local_committed_decree = %" PRId64 ", app_committed_decree = %" PRId64
           ", app_durable_decree = %" PRId64 ", current_learning_status = %s",
           name(),
           _potential_secondary_states.learning_version,
           _config.primary.to_string(),
           _potential_secondary_states.duration_ms(),
           err.to_string(),
           last_committed_decree(),
           _app->last_committed_decree(),
           _app->last_durable_decree(),
           enum_to_string(_potential_secondary_states.learning_status));

    _potential_secondary_states.learning_round_is_running = false;

    if (err != ERR_OK) {
        handle_learning_error(err, true);
    } else {
        // continue
        init_learn(_potential_secondary_states.learning_version);
    }
}

void replica::handle_cluster_learning_error(error_code err, bool is_local_error)
{
    _checker.only_one_thread_access();

    derror("%s: handle_learning_error[%016" PRIx64 "]: learnee = %s, learn_duration = %" PRIu64
           " ms, err = %s, %s",
           name(),
           _potential_secondary_states.learning_version,
           _config.primary.to_string(),
           _potential_secondary_states.duration_ms(),
           err.to_string(),
           is_local_error ? "local_error" : "remote error");

    _stub->_counter_replicas_learning_recent_learn_fail_count->increment();

    update_local_configuration_with_no_ballot_change(
        is_local_error ? partition_status::PS_ERROR : partition_status::PS_INACTIVE);
}

error_code replica::handle_cluster_learning_succeeded_on_primary(::dsn::rpc_address node,
                                                                 uint64_t learn_signature)
{
    auto it = _primary_states.learners.find(node);
    if (it == _primary_states.learners.end()) {
        derror("%s: handle_learning_succeeded_on_primary[%016" PRIx64 "]: learner = %s, "
               "learner not found on primary, return ERR_LEARNER_NOT_FOUND",
               name(),
               learn_signature,
               node.to_string());
        return ERR_LEARNER_NOT_FOUND;
    }

    if (it->second.signature != (int64_t)learn_signature) {
        derror("%s: handle_learning_succeeded_on_primary[%016" PRIx64 "]: learner = %s, "
               "signature not matched, current signature on primary is [%016" PRIx64
               "], return ERR_INVALID_STATE",
               name(),
               learn_signature,
               node.to_string(),
               it->second.signature);
        return ERR_INVALID_STATE;
    }

    upgrade_to_secondary_on_primary(node);
    return ERR_OK;
}

void replica::notify_cluster_learn_completion()
{
    group_check_response report;
    report.pid = get_gpid();
    report.err = ERR_OK;
    report.last_committed_decree_in_app = _app->last_committed_decree();
    report.last_committed_decree_in_prepare_list = last_committed_decree();
    report.learner_signature = _potential_secondary_states.learning_version;
    report.learner_status_ = _potential_secondary_states.learning_status;
    report.node = _stub->_primary_address;

    ddebug("%s: notify_learn_completion[%016" PRIx64 "]: learnee = %s, "
           "learn_duration = %" PRIu64 " ms, local_committed_decree = %" PRId64 ", "
           "app_committed_decree = %" PRId64 ", app_durable_decree = %" PRId64
           ", current_learning_status = %s",
           name(),
           _potential_secondary_states.learning_version,
           _config.primary.to_string(),
           _potential_secondary_states.duration_ms(),
           last_committed_decree(),
           _app->last_committed_decree(),
           _app->last_durable_decree(),
           enum_to_string(_potential_secondary_states.learning_status));

    if (_potential_secondary_states.completion_notify_task != nullptr) {
        _potential_secondary_states.completion_notify_task->cancel(false);
    }

    dsn::message_ex *msg =
        dsn::message_ex::create_request(RPC_LEARN_COMPLETION_NOTIFY, 0, get_gpid().thread_hash());
    dsn::marshall(msg, report);

    _potential_secondary_states.completion_notify_task =
        rpc::call(_config.primary, msg, &_tracker, [
            this,
            report = std::move(report)
        ](error_code err, learn_notify_response && resp) mutable {
            on_learn_completion_notification_reply(err, std::move(report), std::move(resp));
        });
}

void replica::on_cluster_learn_completion_notification(const group_check_response &report,
                                                       /*out*/ learn_notify_response &response)
{
    _checker.only_one_thread_access();

    ddebug("%s: on_learn_completion_notification[%016" PRIx64
           "]: learner = %s, learning_status = %s",
           name(),
           report.learner_signature,
           report.node.to_string(),
           enum_to_string(report.learner_status_));

    if (status() != partition_status::PS_PRIMARY) {
        response.err = (partition_status::PS_INACTIVE == status() && _inactive_is_transient)
                           ? ERR_INACTIVE_STATE
                           : ERR_INVALID_STATE;
        derror("%s: on_learn_completion_notification[%016" PRIx64
               "]: learner = %s, this replica is not primary, but %s, reply %s",
               name(),
               report.learner_signature,
               report.node.to_string(),
               enum_to_string(status()),
               response.err.to_string());
    } else if (report.learner_status_ != learner_status::LearningSucceeded) {
        response.err = ERR_INVALID_STATE;
        derror("%s: on_learn_completion_notification[%016" PRIx64 "]: learner = %s, "
               "learner_status is not LearningSucceeded, but %s, reply ERR_INVALID_STATE",
               name(),
               report.learner_signature,
               report.node.to_string(),
               enum_to_string(report.learner_status_));
    } else {
        response.err = handle_learning_succeeded_on_primary(report.node, report.learner_signature);
        if (response.err != ERR_OK) {
            derror("%s: on_learn_completion_notification[%016" PRIx64 "]: learner = %s, "
                   "handle learning succeeded on primary failed, reply %s",
                   name(),
                   report.learner_signature,
                   report.node.to_string(),
                   response.err.to_string());
        }
    }
}

void replica::on_cluster_learn_completion_notification_reply(error_code err,
                                                             group_check_response &&report,
                                                             learn_notify_response &&resp)
{
    _checker.only_one_thread_access();

    dassert(partition_status::PS_POTENTIAL_SECONDARY == status(),
            "invalid partition_status, status = %s",
            enum_to_string(status()));
    dassert(_potential_secondary_states.learning_status == learner_status::LearningSucceeded,
            "invalid learner_status, status = %s",
            enum_to_string(_potential_secondary_states.learning_status));
    dassert(report.learner_signature == (int64_t)_potential_secondary_states.learning_version,
            "%" PRId64 " VS %" PRId64 "",
            report.learner_signature,
            (int64_t)_potential_secondary_states.learning_version);

    if (err != ERR_OK) {
        handle_learning_error(err, false);
        return;
    }

    if (resp.signature != (int64_t)_potential_secondary_states.learning_version) {
        derror("%s: on_learn_completion_notification_reply[%016" PRIx64
               "]: learnee = %s, learn_duration = %" PRIu64 " ms, "
               "signature not matched, current signature on primary is [%016" PRIx64 "]",
               name(),
               report.learner_signature,
               _config.primary.to_string(),
               _potential_secondary_states.duration_ms(),
               resp.signature);
        handle_learning_error(ERR_INVALID_STATE, false);
        return;
    }

    ddebug("%s: on_learn_completion_notification_reply[%016" PRIx64
           "]: learnee = %s, learn_duration = %" PRIu64 " ms, response_err = %s",
           name(),
           report.learner_signature,
           _config.primary.to_string(),
           _potential_secondary_states.duration_ms(),
           resp.err.to_string());

    if (resp.err != ERR_OK) {
        if (resp.err == ERR_INACTIVE_STATE) {
            dwarn("%s: on_learn_completion_notification_reply[%016" PRIx64
                  "]: learnee = %s, learn_duration = %" PRIu64 " ms, "
                  "learnee is updating ballot, delay to start another round of learning",
                  name(),
                  report.learner_signature,
                  _config.primary.to_string(),
                  _potential_secondary_states.duration_ms());
            _potential_secondary_states.learning_round_is_running = false;
            _potential_secondary_states.delay_learning_task = tasking::create_task(
                LPC_DELAY_LEARN,
                &_tracker,
                std::bind(&replica::init_learn, this, report.learner_signature),
                get_gpid().thread_hash());
            _potential_secondary_states.delay_learning_task->enqueue(std::chrono::seconds(1));
        } else {
            handle_learning_error(resp.err, false);
        }
    } else {
        _stub->_counter_replicas_learning_recent_learn_succ_count->increment();
    }
}

void replica::on_add_slave_learner(const group_check_request &request)
{
    init_cluster_learn(request.config.learner_signature);
}

// in non-replication thread
error_code replica::apply_cluster_learned_state_from_private_log(learn_state &state)
{
    bool duplicating = is_duplicating();
    int64_t offset;
    error_code err;
    prepare_list plist(this,
                       _app->last_committed_decree(),
                       _options->max_mutation_count_in_prepare_list,
                       [this](mutation_ptr &mu) {
                           if (mu->data.header.decree == _app->last_committed_decree() + 1) {
                               // TODO: assign the returned error_code to err and check it
                               _app->apply_mutation(mu);
                           }
                       });
    err = mutation_log::replay(state.files,
                               [&plist](int log_length, mutation_ptr &mu) {
                                   auto d = mu->data.header.decree;
                                   if (d <= plist.last_committed_decree())
                                       return false;

                                   auto old = plist.get_mutation_by_decree(d);
                                   if (old != nullptr &&
                                       old->data.header.ballot >= mu->data.header.ballot)
                                       return false;

                                   plist.prepare(mu, partition_status::PS_SECONDARY);
                                   return true;
                               },
                               offset);

    if (state.__isset.learn_start_decree &&
        (_potential_secondary_states.first_learn_start_decree < 0 ||
         _potential_secondary_states.first_learn_start_decree > state.learn_start_decree)) {
        _potential_secondary_states.first_learn_start_decree = state.learn_start_decree;
    }

    // apply in-buffer private logs
    if (err == ERR_OK) {
        int replay_count = 0;
        binary_reader reader(state.meta);
        while (!reader.is_eof()) {
            auto mu = mutation::read_from(reader, nullptr);
            auto d = mu->data.header.decree;
            if (d <= plist.last_committed_decree())
                continue;

            auto old = plist.get_mutation_by_decree(d);
            if (old != nullptr && old->data.header.ballot >= mu->data.header.ballot)
                continue;

            mu->set_logged();
            plist.prepare(mu, partition_status::PS_SECONDARY);
            ++replay_count;
        }

        if (state.to_decree_included > last_committed_decree()) {
            plist.commit(state.to_decree_included, COMMIT_TO_DECREE_SOFT);
        }
    }

    return err;
}
} // namespace replication
} // namespace dsn
