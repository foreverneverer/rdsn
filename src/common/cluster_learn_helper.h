//
// Created by mi on 2021/12/7.
//

namespace dsn {
namespace replication {

std::vector<partition_configuration>
query_duplication_app_info(const std::string &app_name, const std::vector<rpc_address> &meta_list);
}
}
