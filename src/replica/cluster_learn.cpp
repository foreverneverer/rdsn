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

void replica::init_cluster_learn(const dsn::gpid &gpid)
{
    derror_replica("start copy");
    derror_replica("start copy {}", gpid.to_string());
    // TODO(jiashuo) need use own _potential_secondary_states
    _potential_secondary_states.learning_start_ts_ns = dsn_now_ns();
    _potential_secondary_states.learning_status = learner_status::LearningWithoutPrepare;

    learn_request request;
    request.pid = gpid;
    request.__set_max_gced_decree(get_max_gced_decree_for_learn());
    request.last_committed_decree_in_app = _app->last_committed_decree();
    request.last_committed_decree_in_prepare_list = _prepare_list->last_committed_decree();
    request.learner = _stub->_primary_address; // TODO 改成master的primary地址
    request.signature = _potential_secondary_states.learning_version;
    _app->prepare_get_checkpoint(request.app_specific_learn_request);

    std::vector<rpc_address> remote_meta_list = _stub->_duplication_apps[_app_info.app_name];
    if (remote_meta_list.empty()) {
        derror_replica("mete_list empty");
        return;
    }
    auto pconfig = _stub->query_duplication_app_info(_app_info.app_name, remote_meta_list);
    if (pconfig.empty()) {
        derror_replica("pconfig empty");
        return;
    }

    auto remote_primary = pconfig.at(gpid.get_partition_index()).primary;
    derror_replica("start copy file: {}", remote_primary.to_string());

    dsn::message_ex *msg =
        dsn::message_ex::create_request(RPC_CLUSTER_LEARN, 0, get_gpid().thread_hash());
    dsn::marshall(msg, request);
    rpc::call(remote_primary,
              msg,
              &_tracker,
              [ this,learnee = remote_primary, req_cap = request ](
                  error_code err, learn_response && resp) mutable {
                  on_cluster_learn_reply(
                      err, learnee, std::move(req_cap), std::move(resp));
              });
}

void replica::on_cluster_learn(dsn::message_ex *msg, const learn_request &request)
{
    derror_replica("receive slave learn request: {}, {}",
                   request.pid.to_string(),
                   request.learner.to_string());
    learn_response response;
    remote_learner_state learner_state;
    learner_state.prepare_start_decree = invalid_decree;
    learner_state.timeout_task = nullptr; // TODO: add timer for learner task
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
            dassert_replica(err == ERR_OK, "checkpoint failed");
            response.base_local_dir = _app->data_dir();
            response.__set_replica_disk_tag(get_replica_disk_tag());
        }
    } else {
        derror_replica("start cache, so break");
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

void replica::on_cluster_learn_reply(error_code err,
                                     dsn::rpc_address learnee,
                                     learn_request &&req,
                                     learn_response &&resp)
{
    derror_replica(
        "receive master response: {}, {}, {}", err.to_string(), resp.err, resp.base_local_dir);
    if (err != ERR_OK) {
        return;
    }

    if (resp.err != ERR_OK) {
        return;
    }

    if (resp.prepare_start_decree != invalid_decree) {
        derror_replica("start copy cache and end the cluster learn");
        return;
    } else if (!resp.state.files.empty()) {
        auto learn_dir = _app->learn_dir();
        utils::filesystem::remove_path(learn_dir);
        utils::filesystem::create_directory(learn_dir);

        if (!dsn::utils::filesystem::directory_exists(learn_dir)) {
            dassert_replica(false, "directory_exists");
        }

        bool high_priority = resp.type != learn_type::LT_APP;
        _potential_secondary_states.learn_remote_files_task = _stub->_nfs->copy_remote_files(
            learnee,
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
              req_cap = req,
              resp_copy = resp
            ](error_code err, size_t sz) mutable {
                on_copy_remote_cluster_state_completed(
                    err, sz, copy_start, std::move(req_cap), std::move(resp_copy));
            });
    } else {
        dassert_replica(resp.state.files.size(), "must lager than 0");
    }
}

void replica::on_copy_remote_cluster_state_completed(error_code err,
                                                     size_t size,
                                                     uint64_t copy_start_time,
                                                     learn_request &&req,
                                                     learn_response &&resp)
{
    if (resp.type == learn_type::LT_APP) {
        --_stub->_learn_app_concurrent_count;
        _potential_secondary_states.learn_app_concurrent_count_increased = false;
    }
    if (err == ERR_OK) {
        derror_replica("copy ok, {}", resp.base_local_dir);
    }

    if (err != ERR_OK) {
        derror_replica("copy failed");
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
            if (err == ERR_OK) {
                derror_replica("applay checkpoint ok");
            }
            if (err != ERR_OK) {
                derror_replica("applay checkpoint err: {}", err);
            }
        } else {
            err = apply_cluster_learned_state_from_private_log(lstate);
            if (err == ERR_OK) {
                derror_replica("applay log ok");
            }
            if (err != ERR_OK) {
                derror_replica("applay log err: {}", err);
            }
        }
        // reset prepare list to make it catch with app
        _prepare_list->reset(_app->last_committed_decree());
    }

    // if catch-up done, do flush to enable all learned state is durable
    if (err == ERR_OK && resp.prepare_start_decree != invalid_decree &&
        _app->last_committed_decree() + 1 >=
            _potential_secondary_states.learning_start_prepare_decree &&
        _app->last_committed_decree() > _app->last_durable_decree()) {
        derror_replica("start background_sync_checkpoint ok");
        err = background_sync_checkpoint();
        if (err == ERR_OK) {
            derror_replica("sync checkpoint ok");
            dassert(_app->last_committed_decree() == _app->last_durable_decree(),
                    "%" PRId64 " VS %" PRId64 "",
                    _app->last_committed_decree(),
                    _app->last_durable_decree());
            secondary_learn = true;
            derror_replica("wait add secondary copy own file={}", secondary_learn);
        } else {
            derror_replica("sync checkpoint error");
        }
    }

    secondary_learn = true;
    derror_replica("wait add secondary copy own file={}", secondary_learn);
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
