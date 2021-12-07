//
// Created by mi on 2021/12/7.
//
#include <dsn/tool-api/rpc_address.h>
#include <dsn.layer2_types.h>
#include "cluster_learn_helper.h"
#include "replication_common.h"
#include <dsn/dist/fmt_logging.h>

namespace dsn {
namespace replication {

// todo refactor for meta use the function
std::vector<partition_configuration>
query_duplication_app_info(const std::string &app_name, const std::vector<rpc_address> &meta_list)
{
    std::vector<partition_configuration> configurations;
    for (const auto &meta : meta_list) {
        configuration_query_by_index_request meta_config_request;
        meta_config_request.app_name = app_name;
        dsn::message_ex *msg =
            dsn::message_ex::create_request(RPC_CM_QUERY_PARTITION_CONFIG_BY_INDEX, 0, 0);
        dsn::marshall(msg, meta_config_request);
        auto task = rpc::call(meta, msg, nullptr, [
            &configurations,
            req_cap = meta_config_request
        ](error_code err, configuration_query_by_index_response && resp) mutable {
            if (err != ERR_OK) {
                derror_f("failed:{}", err.to_string());
            } else {
                if (resp.err != ERR_OK) {
                    derror_f("failed: {}", resp.err.to_string());
                } else {
                    configurations = resp.partitions;
                }
            }
        });
        task->wait(); // todo
        if (!configurations.empty()) {
            return configurations;
        }
    }
    return configurations;
}
} // namespace replication
} // namespace dsn
