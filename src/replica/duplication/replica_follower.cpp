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

#include <dsn/tool-api/group_address.h>

namespace dsn {
namespace replication {

replica_follower::replica_follower(replica *r) : replica_base(r), _replica(r) {}

replica_follower::~replica_follower() = default;

error_code replica_follower::duplicate_checkpoint()
{

    rpc_address target_node;
    gpid target_gpid;

    error_code err_code = get_master_config(target_node, target_gpid);
    if (err_code != ERR_OK) {
        return err_code;
    }

    derror_replica("start copy checkpoint because get remote duplication replica config[{}.{}] "
                   "succeeded: address = {}, gpid = {}",
                   _replica->get_app_info()->dup_options.cluster_name,
                   _replica->get_app_info()->dup_options.app_name,
                   target_node,
                   target_gpid);

    _replica->copy_checkpoint(target_node, target_gpid);
}

error_code replica_follower::get_master_config(rpc_address &target, gpid &pid)
{
    error_code err_code = ERR_OK;

    rpc_address meta_servers;
    meta_servers.assign_group(_replica->get_app_info()->dup_options.cluster_name.c_str());
    meta_servers.group_address()->add_list(_replica->get_app_info()->dup_options.metas);

    configuration_query_by_index_request meta_config_request;
    meta_config_request.app_name = _replica->get_app_info()->app_name;
    meta_config_request.partition_indices = std::vector<int32_t>(get_gpid().get_partition_index());

    dsn::message_ex *msg =
        dsn::message_ex::create_request(RPC_CM_QUERY_PARTITION_CONFIG_BY_INDEX, 1000, 1);
    dsn::marshall(msg, meta_config_request);
    rpc::call(
        meta_servers,
        msg,
        _replica->tracker(),
        [&](error_code err, configuration_query_by_index_response &&resp) mutable {
            if (err != ERR_OK) {
                err_code = err;
                derror_replica("get remote duplication replica config[{}.{}] failed[NET]: {}",
                               _replica->get_app_info()->dup_options.cluster_name,
                               _replica->get_app_info()->dup_options.app_name,
                               err_code.to_string());
            } else {
                if (err_code != ERR_OK) {
                    err_code = resp.err;
                    derror_replica("get remote duplication app config[{}.{}] failed[RPC]: {}",
                                   _replica->get_app_info()->dup_options.cluster_name,
                                   _replica->get_app_info()->dup_options.app_name,
                                   err_code.to_string());
                    return;
                }

                if (resp.partitions.empty()) {
                    err_code = ERR_OBJECT_NOT_FOUND;
                    derror_replica("get remote duplication app config[{}.{}] is empty[RPC]: {}",
                                   _replica->get_app_info()->dup_options.cluster_name,
                                   _replica->get_app_info()->dup_options.app_name,
                                   err_code.to_string());
                    return;
                }

                if (resp.partition_count != _replica->get_app_info()->partition_count) {
                    err_code = ERR_INCONSISTENT_STATE;
                    derror_replica("get remote duplication app config[{}.{}] is inconsistent[RPC]: "
                                   "local_partition_count = {} vs master_partition_count = {}",
                                   _replica->get_app_info()->dup_options.cluster_name,
                                   _replica->get_app_info()->dup_options.app_name,
                                   _replica->get_app_info()->partition_count,
                                   resp.partition_count);
                    return;
                }

                target = resp.partitions[0].primary;
                pid = resp.partitions[0].pid;
            }
        })
        ->wait();
    return err_code;
}

} // namespace replication
} // namespace dsn