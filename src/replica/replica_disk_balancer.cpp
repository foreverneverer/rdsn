#include "replica.h"
#include "replica_stub.h"

#include <dsn/utility/filesystem.h>
#include <dsn/dist/fmt_logging.h>
#include <boost/algorithm/string/replace.hpp>
#include <dsn/dist/replication/replication_app_base.h>

namespace dsn {
namespace replication {

void replica::on_migrate_replica(const migrate_replica_request &req,
                                 /*out*/ migrate_replica_response &resp)
{
    //_checker.only_one_thread_access();

    if (!check_replica_on_disk(req, resp)) {
        return;
    }
    migrate_checkpoint(req, resp);
    update_migration_replica(req, resp);
}

bool replica::check_replica_on_disk(const migrate_replica_request &req,
                                    /*out*/ migrate_replica_response &resp)
{
    // TODO(jiashuo1) need manager control migattion flow
    if (_disk_replica_migration_status != disk_replica_migration_status::IDLE) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "existed running task(migration status = {}), partition_status = {}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      enum_to_string(_disk_replica_migration_status),
                      enum_to_string(status()));
        resp.err = ERR_BUSY;
        return false;
    }

    // TODO(jiashuo1) check if in bulkload,restore,coldbackup,split, if true, return;
    // TODO(jiashuo1) whether add `checkpointing/checkpointed` status to replcace `moved` status
    // TODO(jiashuo1) need automic to makesure thread safe
    _disk_replica_migration_status = disk_replica_migration_status::MOVING;

    // TODO(jiashuo1) auto downgrade to secondary if primary
    if (status() != partition_status::type::PS_SECONDARY) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "invalid partition_status = {}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      enum_to_string(status()));
        resp.err = ERR_INVALID_STATE;
        return false;
    }

    _stub->update_disk_holding_replicas();

    bool valid_origin_disk = false;
    bool valid_target_disk = false;
    for (const auto &dir_node : _stub->_fs_manager._dir_nodes) {

        if (dir_node->tag == req.origin_disk) {
            valid_origin_disk = true;
            std::set<gpid> disk_holding_replicas = dir_node->holding_replicas[req.pid.get_app_id()];
            if (disk_holding_replicas.find(req.pid) == disk_holding_replicas.end()) {
                dwarn_replica("received disk replica migration request(gpid={}, origin={}, "
                              "target={}) but not exist on origin disk, partition_status = {}",
                              req.pid.to_string(),
                              req.origin_disk,
                              req.target_disk,
                              enum_to_string(status()));
                resp.err = ERR_OBJECT_NOT_FOUND;
                return false;
            }
        }

        if (dir_node->tag == req.target_disk) {
            valid_target_disk = true;
            std::set<gpid> disk_holding_replicas = dir_node->holding_replicas[req.pid.get_app_id()];
            if (disk_holding_replicas.find(req.pid) != disk_holding_replicas.end()) {
                dwarn_replica("received disk replica migration request(gpid={}, origin={}, "
                              "target={}) but replica has existed on target disk, partition_status "
                              "= {}",
                              req.pid.to_string(),
                              req.origin_disk,
                              req.target_disk,
                              enum_to_string(status()));
                resp.err = ERR_OBJECT_NOT_FOUND;
                return false;
            }
        }
    }

    if (!valid_origin_disk || !valid_target_disk) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "not invalid disk, partition_status = {}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      enum_to_string(status()));
        resp.err = ERR_OBJECT_NOT_FOUND;
        return false;
    }

    ddebug_replica("received disk replica migration request(gpid={}, origin={}, target={}) "
                   "partition_status = {}",
                   req.pid.to_string(),
                   req.origin_disk,
                   req.target_disk,
                   enum_to_string(status()));
    return true;
}

void replica::migrate_checkpoint(const migrate_replica_request &req,
                                 /*out*/ migrate_replica_response &resp)
{
    if (_disk_replica_migration_status != disk_replica_migration_status::MOVING) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "invalid migration status = {}"
                      "partition_status = {}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      enum_to_string(_disk_replica_migration_status),
                      enum_to_string(status()));
    }

    error_code sync_checkpoint_err = _app->sync_checkpoint();
    if (sync_checkpoint_err != ERR_OK) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "sync_checkpoint failed, error = {}"
                      "partition_status = {}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      sync_checkpoint_err.to_string(),
                      enum_to_string(status()));
        resp.err = sync_checkpoint_err;
        return;
    }

    // TODO(jiashuo1) need copy but reference
    std::string replica_target_dir = _dir;
    boost::replace_first(replica_target_dir, req.origin_disk, req.target_disk);
    _disk_replica_migration_target_dir = replica_target_dir;
    _disk_replica_migration_target_data_dir =
        utils::filesystem::path_combine(replica_target_dir, "/data/rdb/");

    if (!utils::filesystem::directory_exists(_disk_replica_migration_target_data_dir)) {
        derror_replica("create migration target data dir {} failed coz exist dir",
                       _disk_replica_migration_target_data_dir);
        // TODO(jiashuo1) remember reset/clear status and data
        return;
    }

    error_code copy_checkpoint_err = _app->copy_checkpoint_to_dir(
        _disk_replica_migration_target_data_dir.c_str(), 0 /*last_decree*/);
    if (copy_checkpoint_err != ERR_OK) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "copy_checkpoint_to_dir failed, error = {}"
                      "partition_status = {}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      sync_checkpoint_err.to_string(),
                      enum_to_string(status()));
        resp.err = copy_checkpoint_err;
        return;
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // TODO(jiashuo1) .init_info seem no need copy
    replica_app_info info((app_info *)&_app_info);
    std::string path =
        utils::filesystem::path_combine(_disk_replica_migration_target_dir, ".app-info");
    info.store(path.c_str());
    error_code store_info_err = info.store(path.c_str());
    if (store_info_err != ERR_OK) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "store info failed, error = {}"
                      "partition_status = {}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      store_info_err.to_string(),
                      enum_to_string(status()));
        resp.err = store_info_err;
        return;
    }

    ////////////////////////////////////////////////////////////////////////
    // TODO(jiashuo1) need call primary and let primary update replica config
    update_local_configuration_with_no_ballot_change(partition_status::PS_ERROR);
    _stub->begin_close_replica(this);

    // TODO(jiashuo1) before here, have same replica dir, it may cause crash
    // TODO(jiashuo1) check rename success
    dsn::utils::filesystem::rename_path(_dir, fmt::format("{}.{}", _dir, ".balance.temp"));
    _disk_replica_migration_status = disk_replica_migration_status::MOVED;
}
}
}