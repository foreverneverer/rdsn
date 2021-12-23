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

void replica::init_cluster_learn(uint64_t signature) // todo 可能不需要这个参数
{
    _checker.only_one_thread_access(); // todo 依然必须在replication线程池，并保持单线程
    if (!check_cluster_learner_state()) {
        derror_replica(""); // todo
        return;
    }

    if (signature == invalid_signature) {
        if (!_learner_states.cleanup(false)) {
            derror_replica("previous learning with signature {} and can't stop",
                           _learner_states.learning_version);
            return;
        }
        init_learn_status(_learner_states.learning_version++, [this] { start_cluster_learn(); });
    } else {
        handle_current_learn_status([this] { start_cluster_learn(); });
    }
}

bool replica::check_cluster_learner_state()
{
    if (status() != partition_status::PS_PRIMARY) { // todo 需要支持Primary done
        derror_replica("state is not potential secondary or duplicating but "
                       "replica_learn={}，cluster_learn={}, skip "
                       "learning with signature",
                       enum_to_string(status()),
                       cluster_learn_status());
        return false;
    }

    if (_learner_states.learning_status >= learner_status::LearningWithPrepare) {
        derror_replica("app duplication is {}, step to next stage {} for learning status is {}",
                       cluster_learn_status(),
                       enum_to_string(app_duplication_status::ClusterLearningSucceeded),
                       enum_to_string(_learner_states.learning_status));
        _app_duplication_status = app_duplication_status::ClusterLearningSucceeded;
        return false;
    }

    if (_learner_states.learning_round_is_running) {
        derror_replica("previous learning is still running, skip this learnning");
        return false;
    }

    return true;
}

void replica::init_learn_status(uint64_t signature, const std::function<void()> &learn_execute)
{
    _learner_states.learning_version = signature;
    _learner_states.learning_start_ts_ns = dsn_now_ns();
    _learner_states.learning_status = learner_status::LearningWithoutPrepare;
    _prepare_list->truncate(_app->last_committed_decree());

    learn_execute();
}

void replica::handle_current_learn_status(const std::function<void()> &learn_execute)
{
    switch (_learner_states.learning_status) {
    case learner_status::LearningWithoutPrepare:
    case learner_status::LearningFailed:
        break;
    case learner_status::LearningWithPrepare:
        if (!catch_up_latest_decree()) {
            return;
        }
        break;
    case learner_status::LearningSucceeded:
    {
        check_state_completeness();//todo 等待重构
        notify_learn_completion();//todo 等待重构
        break;
    }
    default:
        dassert_replica(false,"invalid learner_status, status = %s",enum_to_string(_learner_states.learning_status));
    }

    learn_execute();
}

bool replica::catch_up_latest_decree()
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
    _learner_states.learning_status = learner_status::LearningSucceeded;;
    return true;
    // fall through to success
}

void replica::start_cluster_learn() {

}

void replica::get_learnee_state() {

}

void replica::get_learnee_state_completed() {

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
        init_cluster_learn(proposal);
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
void replica::init_cluster_learn(configuration_update_request &proposal)
{
    _checker.only_one_thread_access();

    dassert_f(_app_duplication_status == app_duplication_status::ClusterLearning,
              "app_duplication_status must be at {}",
              enum_to_string(app_duplication_status::ClusterLearning));
    dassert_f(status() == partition_status::PS_PRIMARY,
              "replica must be at {} when start cluster learning",
              enum_to_string(partition_status::PS_PRIMARY));
    dassert_f(proposal.info.duplicating, "app must be at duplicating when start cluster learning");

    derror_replica("process add cluster learner, remote = {}, "
                   "last_committed_decree = {} vs {}, duplicating = {}",
                   proposal.duplication_config.primary.to_string(),
                   proposal.duplication_config.last_committed_decree,
                   last_committed_decree(),
                   proposal.info.duplicating);

    _duplicating = proposal.info.duplicating;
    _duplication_remote_node = proposal.duplication_config.primary;
    init_learn(invalid_signature); // todo 初始值是0，即无效值，发出时会出现校验error done
}

// learner
bool replica::is_cluster_learner_with_primary_status() const
{
    return _duplicating && status() == partition_status::PS_PRIMARY;
}

// learner
std::string replica::cluster_learn_status()
{
    return fmt::format("{}[{}]", enum_to_string(_app_duplication_status), _duplicating);
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

} // namespace replication
} // namespace dsn
