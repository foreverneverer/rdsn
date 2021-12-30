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

replica_follower::~replica_follower() = default;

error_code replica_follower::duplicate_checkpoint()
{
    derror_replica("start duplicate master[{}] checkpoint", master_replica_name());

    error_code err_code = update_master_replica_config();
    if (err_code != ERR_OK) {
        return err_code;
    }
    return copy_master_replica_checkpoint(_master_replica_config.primary,
                                          _master_replica_config.pid);
}

error_code replica_follower::update_master_replica_config()
{
    error_code err_code = ERR_OK;

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
              _replica->tracker(),
              [&](error_code err, configuration_query_by_index_response &&resp) mutable {
                  err_code = update_master_replica_config_callback(err, std::move(resp));
              })
        ->wait();
    return err_code;
}

error_code replica_follower::update_master_replica_config_callback(
    error_code err, configuration_query_by_index_response &&resp)
{
    error_code err_code = err != ERR_OK ? err : resp.err;
    if (err_code != ERR_OK) {
        derror_replica(
            "query master[{}] config failed: {}", master_replica_name(), err_code.to_string());
        return err_code;
    }

    if (resp.partition_count != _replica->get_app_info()->partition_count) {
        derror_replica("master[{}] partition count is inconsistent: local = {} vs master = {}",
                       master_replica_name(),
                       _replica->get_app_info()->partition_count,
                       resp.partition_count);
        return ERR_INCONSISTENT_STATE;
    }

    if (resp.partitions.empty()) {
        derror_replica("master[{}] config is empty", master_replica_name());
        return ERR_OBJECT_NOT_FOUND;
    }

    if (resp.partitions[0].pid.get_partition_index() != get_gpid().get_partition_index()) {
        derror_replica("master[{}] partition index is inconsistent: local = {} vs master = {}",
                       master_replica_name(),
                       get_gpid().get_partition_index(),
                       resp.partitions[0].pid.get_partition_index());
        return ERR_INCONSISTENT_STATE;
    }

    _master_replica_config = resp.partitions[0];
    return ERR_OK;
}

error_code replica_follower::copy_master_replica_checkpoint(const rpc_address &node,
                                                            const gpid &pid)
{
    derror_replica("query master[{}] replica checkpoint", master_replica_name());

    error_code err_code = ERR_OK;

    learn_request request;
    request.pid = pid;
    dsn::message_ex *msg =
        dsn::message_ex::create_request(RPC_QUERY_LAST_CHECKPOINT_INFO, 0, pid.thread_hash());
    dsn::marshall(msg, request);
    rpc::call(node, msg, _replica->tracker(), [&](error_code err, learn_response &&resp) mutable {
        err_code = copy_master_checkpoint_callback(err, std::move(resp));
    })->wait();
    return err_code;
}

error_code replica_follower::copy_master_checkpoint_callback(error_code err, learn_response &&resp)
{
    error_code err_code = err != ERR_OK ? err : resp.err;
    if (err_code != ERR_OK) {
        derror_replica("query master[{}] replica checkpoint info failed, err = {}",
                       master_replica_name(),
                       err_code.to_string());
        return err_code;
    }

    std::string dest = utils::filesystem::path_combine(
        _replica->dir(), duplication_constants::DUPLICATION_FOLLOWER_ROOT_DIR);
    if (!utils::filesystem::remove_path(dest)) {
        derror_replica(
            "clear master[{}] replica checkpoint dest dir {} failed", master_replica_name(), dest);
        return ERR_FILE_OPERATION_FAILED;
    }

    err_code = nfs_copy_remote_files(
        resp.address, resp.replica_disk_tag, resp.base_local_dir, resp.state.files, dest);
    if (err_code != ERR_OK) {
        return err_code;
    }
    return ERR_OK;
}

error_code replica_follower::nfs_copy_remote_files(const rpc_address &remote_node,
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

    error_code err_code = ERR_OK;
    _replica->_stub->_nfs
        ->copy_remote_files(
            request,
            LPC_NFS_COPY_CHECKPOINT_FILES,
            _replica->tracker(),
            [&](error_code err, size_t size) mutable {
                if (err != ERR_OK) {
                    err_code = err;
                    derror_replica(
                        "nfs copy master[{}] checkpoint failed: checkpoint = {}, err = {}",
                        master_replica_name(),
                        remote_dir,
                        err.to_string());
                    return;
                }
                derror_replica(
                    "nfs copy master[{}] checkpoint completed: checkpoint = {}, size = {}",
                    master_replica_name(),
                    remote_dir,
                    size);
            })
        ->wait();
    return err_code;
}

} // namespace replication
} // namespace dsn