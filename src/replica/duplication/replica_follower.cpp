/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "replica_follower.h"
#include "replica/replica_stub.h"
#include "dsn/utility/filesystem.h"
#include "dsn/dist/replication/duplication_common.h"

#include <dsn/tool-api/group_address.h>
#include <dsn/dist/nfs_node.h>

namespace dsn {
namespace replication {

replica_follower::replica_follower(replica *r)
    : replica_base(r),
      _replica(r),
      _master_cluster_name(r->get_app_info()->dup_options.cluster_name),
      _master_app_name(r->get_app_info()->dup_options.app_name),
      _master_meta_list(r->get_app_info()->dup_options.metas)
{
}

replica_follower::~replica_follower() { _tracker.cancel_outstanding_tasks(); };

error_code replica_follower::duplicate_checkpoint()
{
    if (_duplicating) {
        derror_replica("duplicate master[{}] checkpoint is running", master_replica_name());
        return ERR_BUSY;
    }

    derror_replica("start duplicate master[{}] checkpoint", master_replica_name());
    zauto_lock l(_lock);
    _duplicating = true;
    async_duplicate_checkpoint_from_master_replica();
    _tracker.wait_outstanding_tasks();
    _duplicating = false;
    return _tracker.result();
}

void replica_follower::async_duplicate_checkpoint_from_master_replica()
{
    rpc_address meta_servers;
    meta_servers.assign_group(_master_cluster_name.c_str());
    meta_servers.group_address()->add_list(_master_meta_list);

    configuration_query_by_index_request meta_config_request;
    meta_config_request.app_name = _master_app_name;
    meta_config_request.partition_indices = {get_gpid().get_partition_index()};

    derror_replica("query master[{}] replica configuration", master_replica_name());
    dsn::message_ex *msg = dsn::message_ex::create_request(
        RPC_CM_QUERY_PARTITION_CONFIG_BY_INDEX, 0, get_gpid().thread_hash());
    dsn::marshall(msg, meta_config_request);
    rpc::call(meta_servers,
              msg,
              &_tracker,
              [&](error_code err, configuration_query_by_index_response &&resp) mutable {
                  tasking::enqueue(LPC_DUPLICATE_CHECKPOINT, &_tracker, [=]() mutable {
                      update_master_replica_config_callback(err, std::move(resp));
                  });
              });
}

void replica_follower::update_master_replica_config_callback(
    error_code err, configuration_query_by_index_response &&resp)
{
    error_code err_code = err != ERR_OK ? err : resp.err;
    if (err_code != ERR_OK) {
        _tracker.set_result(err_code);
        derror_replica(
            "query master[{}] config failed: {}", master_replica_name(), err_code.to_string());
        return;
    }

    if (resp.partition_count != _replica->get_app_info()->partition_count) {
        _tracker.set_result(ERR_INCONSISTENT_STATE);
        derror_replica("master[{}] partition count is inconsistent: local = {} vs master = {}",
                       master_replica_name(),
                       _replica->get_app_info()->partition_count,
                       resp.partition_count);
        return;
    }

    if (resp.partitions.empty()) {
        _tracker.set_result(ERR_OBJECT_NOT_FOUND);
        derror_replica("master[{}] config is empty", master_replica_name());
        return;
    }

    if (resp.partitions[0].pid.get_partition_index() != get_gpid().get_partition_index()) {
        _tracker.set_result(ERR_INCONSISTENT_STATE);
        derror_replica("master[{}] partition index is inconsistent: local = {} vs master = {}",
                       master_replica_name(),
                       get_gpid().get_partition_index(),
                       resp.partitions[0].pid.get_partition_index());
        return;
    }

    _master_replica_config = resp.partitions[0];
    copy_master_replica_checkpoint(_master_replica_config.primary, _master_replica_config.pid);
}

void replica_follower::copy_master_replica_checkpoint(const rpc_address &node, const gpid &pid)
{
    derror_replica("query master[{}] replica checkpoint", master_replica_name());
    learn_request request;
    request.pid = pid;
    dsn::message_ex *msg =
        dsn::message_ex::create_request(RPC_QUERY_LAST_CHECKPOINT_INFO, 0, pid.thread_hash());
    dsn::marshall(msg, request);
    rpc::call(node, msg, &_tracker, [&](error_code err, learn_response &&resp) mutable {
        query_last_checkpoint_info_callback(err, std::move(resp));
    });
}

void replica_follower::query_last_checkpoint_info_callback(error_code err, learn_response &&resp)
{
    error_code err_code = err != ERR_OK ? err : resp.err;
    if (err_code != ERR_OK) {
        _tracker.set_result(err_code);
        derror_replica("query master[{}] replica checkpoint info failed, err = {}",
                       master_replica_name(),
                       err_code.to_string());
        return;
    }

    std::string dest = utils::filesystem::path_combine(
        _replica->dir(), duplication_constants::DUPLICATION_FOLLOWER_ROOT_DIR);
    if (!utils::filesystem::remove_path(dest)) {
        _tracker.set_result(ERR_FILE_OPERATION_FAILED);
        derror_replica(
            "clear master[{}] replica checkpoint dest dir {} failed", master_replica_name(), dest);
        return;
    }

    nfs_copy_remote_files(
        resp.address, resp.replica_disk_tag, resp.base_local_dir, resp.state.files, dest);
}

void replica_follower::nfs_copy_remote_files(const rpc_address &remote_node,
                                             const std::string &remote_disk,
                                             const std::string &remote_dir,
                                             std::vector<std::string> &file_list,
                                             const std::string &dest_dir)
{
    derror_replica(
        "nfs start copy master[{}] replica checkpoint: {}", master_replica_name(), remote_dir);
    std::shared_ptr<remote_copy_request> request = std::make_shared<remote_copy_request>();
    request->source = remote_node;
    request->source_disk_tag = remote_disk;
    request->source_dir = remote_dir;
    request->files = file_list;
    request->dest_disk_tag = _replica->get_replica_disk_tag();
    request->dest_dir = dest_dir;
    request->overwrite = true;
    request->high_priority = false;

    _replica->_stub->_nfs->copy_remote_files(
        request,
        LPC_DUPLICATE_CHECKPOINT_COMPLETED,
        &_tracker,
        [&, remote_dir](error_code err, size_t size) mutable {
            if (err != ERR_OK) {
                _tracker.set_result(err);
                derror_replica("nfs copy master[{}] checkpoint failed: checkpoint = {}, err = {}",
                               master_replica_name(),
                               remote_dir,
                               err.to_string());
                return;
            }
            derror_replica("nfs copy master[{}] checkpoint completed: checkpoint = {}, size = {}",
                           master_replica_name(),
                           remote_dir,
                           size);
        });
}

} // namespace replication
} // namespace dsn