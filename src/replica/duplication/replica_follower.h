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

#pragma once
#include "replica/replica.h"

namespace dsn {
namespace replication {

class replica_follower : replica_base
{
public:
    explicit replica_follower(replica *r);
    ~replica_follower();
    error_code duplicate_checkpoint();

private:
    replica *_replica;

    std::string _master_cluster_name;
    std::string _master_app_name;
    std::vector<rpc_address> _master_meta_list;
    partition_configuration _master_replica_config;

    error_code update_master_replica_config();
    error_code update_master_replica_config_callback(error_code err,
                                                     configuration_query_by_index_response &&resp);

    error_code copy_master_checkpoint(const rpc_address &node, const gpid &pid);
    error_code copy_master_checkpoint_callback(error_code err, learn_response &&resp);

    error_code nfs_copy_remote_files(const rpc_address &remote_node,
                                     const std::string &remote_disk,
                                     const std::string &remote_dir,
                                     std::vector<std::string> &file_list,
                                     const std::string &dest);

    std::string master_replica_name()
    {
        std::string app_info = fmt::format("{}.{}", _master_cluster_name, _master_app_name);
        if (_master_replica_config.primary != rpc_address::s_invalid_address) {
            return fmt::format("{}({}|{})",
                               app_info,
                               _master_replica_config.primary.to_string(),
                               _master_replica_config.pid.to_string());
        }
        return app_info;
    }
};

} // namespace replication
} // namespace dsn