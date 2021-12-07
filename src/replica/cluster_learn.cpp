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

// learner
void replica::on_add_cluster_learner(configuration_update_request &proposal)
{
    _checker.only_one_thread_access();

    dassert_f(_app_duplication_status == app_duplication_status::ClusterLearning,
              "app_duplication_status must be at {}",
              app_duplication_status::ClusterLearning);
    dassert_f(status() == partition_status::PS_PRIMARY,
              "replica must be at {}",
              partition_status::PS_PRIMARY);

    derror_replica("process add cluster learner, remote = {},"
                   "last_committed_decree = {} vs {}, duplicating = {}",
                   proposal.duplication_config.primary,
                   proposal.duplication_config.last_committed_decree,
                   last_committed_decree(),
                   proposal.info.duplicating);

    _duplicating = proposal.info.duplicating;
    init_learn(_potential_secondary_states.learning_version, proposal.duplication_config.primary);
}

// learner
bool replica::is_cluster_primary_learner()
{
    return _duplicating && status() == partition_status::PS_PRIMARY;
}

// learner
std::string replica::cluster_learn_status()
{
    return fmt::format("[{}]{}", _duplicating, enum_to_string(_app_duplication_status));
}

void replica::add_duplication_learner(const rpc_address& learner, uint64_t signature) {
    auto it = _primary_states.learners.find(learner);
    if (it == _primary_states.learners.end()) {
        remote_learner_state state;
        state.prepare_start_decree = invalid_decree;
        state.timeout_task = nullptr; // TODO: add timer for learner task

        state.signature = signature;// TODO 这里的校验可能会有一些问题
        _primary_states.learners[learner] = state;
        _primary_states.statuses[learner] = partition_status::PS_PRIMARY;
    }
}


} // namespace replication
} // namespace dsn
