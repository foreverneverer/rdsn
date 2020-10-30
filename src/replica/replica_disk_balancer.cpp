#include "replica.h"
#include "replica_stub.h"
#include "backup/cold_backup_context.h"
#include "bulk_load/replica_bulk_loader.h"

#include <dsn/utility/filesystem.h>
#include <dsn/dist/fmt_logging.h>
#include <boost/algorithm/string/replace.hpp>
#include <dsn/dist/replication/replication_app_base.h>

namespace dsn {
namespace replication {
// TODO(jiashuo1) log need show migratition status
void replica::on_migrate_replica(const migrate_replica_request &req,
                                 /*out*/ migrate_replica_response &resp)
{
    //_checker.only_one_thread_access();
    if (!check_migration_replica_on_disk(req, resp)) {
        return;
    }
    // need task queue and assign replica long pool
    copy_migration_replica_checkpoint(req, resp);
}

// TODO(jiashuo1) need assign replication pool to make sure single thread
bool replica::check_migration_replica_on_disk(const migrate_replica_request &req,
                                              /*out*/ migrate_replica_response &resp)
{
    // TODO(jiashuo1) need manager control migration flow
    if (_disk_replica_migration_status != disk_replica_migration_status::IDLE) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "existed running task(migration_status={},partition_status={})",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      enum_to_string(_disk_replica_migration_status),
                      enum_to_string(status()));
        resp.err = ERR_BUSY;
        return false;
    }

    set_disk_replica_migration_status(disk_replica_migration_status::MOVING);

    bool is_running_cold_backup = false;
    for (auto const &cold_backup_context : _cold_backup_contexts) {
        if (cold_backup_context.second->status() != cold_backup_status::ColdBackupInvalid) {
            is_running_cold_backup = true;
            break;
        }
    }

    if (_bulk_loader->get_bulk_load_status() != bulk_load_status::BLS_INVALID ||
        is_running_cold_backup || (_restore_progress != 0 && _restore_progress != 1000)) {
        dwarn_replica(
            "received disk replica migration request(gpid={}, origin={}, target={}) but "
            "but has other task(bulkload={}, backup={}, restore_process={}), partition_status={}",
            req.pid.to_string(),
            req.origin_disk,
            req.target_disk,
            enum_to_string(_bulk_loader->get_bulk_load_status()),
            is_running_cold_backup,
            _restore_progress,
            enum_to_string(status()));
        reset_replica_migration_status();
        resp.err = ERR_INVALID_STATE;
        return false;
    }

    // TODO(jiashuo1) auto downgrade to secondary if primary
    if (status() != partition_status::type::PS_SECONDARY) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "invalid partition_status={}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      enum_to_string(status()));
        reset_replica_migration_status();
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
                reset_replica_migration_status();
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
                reset_replica_migration_status();
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
        reset_replica_migration_status();
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

// need assign replica default
void replica::copy_migration_replica_checkpoint(const migrate_replica_request &req,
                                                /*out*/ migrate_replica_response &resp)
{
    if (_disk_replica_migration_status != disk_replica_migration_status::MOVING) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "invalid migration_status = {} "
                      "partition_status = {}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      enum_to_string(_disk_replica_migration_status),
                      enum_to_string(status()));
        reset_replica_migration_status();
        resp.err = ERR_INVALID_STATE;
        return;
    }

    // TODO(jiashuo1) need copy but reference
    std::string replica_dir = _dir;
    // using origin dir init new dir
    boost::replace_first(replica_dir, req.origin_disk, req.target_disk);
    _disk_replica_migration_target_dir = replica_dir;
    if (utils::filesystem::directory_exists(_disk_replica_migration_target_dir)) {
        derror_replica("migration target replica dir {} has existed",
                       _disk_replica_migration_target_dir);
        reset_replica_migration_status();
        return;
    }
    std::string data_dir =
        utils::filesystem::path_combine(_disk_replica_migration_target_dir, "/data/rdb/");

    // using origin dir init new tmp dir
    std::string replica_name = fmt::format("{}.{}", get_gpid(), _app_info.app_type);
    std::string replica_tmp_name = fmt::format("{}.disk.balance", replica_name);
    boost::replace_first(replica_dir, replica_name, replica_tmp_name);
    _disk_replica_migration_target_temp_dir = replica_dir;
    std::string tmp_data_dir =
        utils::filesystem::path_combine(_disk_replica_migration_target_temp_dir, "/data/rdb/");

    if (utils::filesystem::directory_exists(_disk_replica_migration_target_temp_dir)) {
        dwarn_replica("migration target temp replica dir {} has existed, it will be deleted",
                      _disk_replica_migration_target_temp_dir);
        utils::filesystem::remove_path(_disk_replica_migration_target_temp_dir);
    }

    if (!utils::filesystem::create_directory(tmp_data_dir)) {
        derror_replica("create migration target temp data dir {} failed", tmp_data_dir);
        // TODO(jiashuo1) remember reset/clear status and data
        reset_replica_migration_status();
        return;
    }

    error_code sync_checkpoint_err = _app->sync_checkpoint();
    if (sync_checkpoint_err != ERR_OK) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "sync_checkpoint failed, error = {} "
                      "partition_status = {}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      sync_checkpoint_err.to_string(),
                      enum_to_string(status()));
        reset_replica_migration_status();
        resp.err = sync_checkpoint_err;
        return;
    }

    error_code copy_checkpoint_err =
        _app->copy_checkpoint_to_dir(tmp_data_dir.c_str(), 0 /*last_decree*/);
    if (copy_checkpoint_err != ERR_OK) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "copy_checkpoint_to_dir failed, the temp target_dir will be deleted, "
                      "target_dir = {}, error = {} "
                      "partition_status = {}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      tmp_data_dir,
                      copy_checkpoint_err.to_string(),
                      enum_to_string(status()));
        reset_replica_migration_status();
        utils::filesystem::remove_path(tmp_data_dir);
        resp.err = copy_checkpoint_err;
        return;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    replica_init_info init_info = _app->init_info();
    error_code store_init_info_err = init_info.store(_disk_replica_migration_target_temp_dir);
    if (store_init_info_err != ERR_OK) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "store init info failed, error = {} "
                      "partition_status = {}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      store_init_info_err.to_string(),
                      enum_to_string(status()));
        reset_replica_migration_status();
        resp.err = store_init_info_err;
        return;
    }

    replica_app_info info((app_info *)&_app_info);
    std::string path =
        utils::filesystem::path_combine(_disk_replica_migration_target_temp_dir, ".app-info");
    info.store(path.c_str());
    error_code store_info_err = info.store(path.c_str());
    if (store_info_err != ERR_OK) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "store info failed, error = {} "
                      "partition_status = {}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      store_info_err.to_string(),
                      enum_to_string(status()));
        reset_replica_migration_status();
        resp.err = store_info_err;
        return;
    }

    set_disk_replica_migration_status(disk_replica_migration_status::MOVED);

    ddebug_replica("disk replica migration request(gpid={}, origin={}, target={})"
                   "has completed copy checkpoint, it will response primary err = {} after next "
                   "receive group check and be trigered to close"
                   "partition_status = {}",
                   req.pid.to_string(),
                   req.origin_disk,
                   req.target_disk,
                   ERR_EXPIRED.to_string(),
                   enum_to_string(status()));
}

// TODO(jiashuo1) wait will increase the durable
// TODO(jiashuo1) need default
void replica::update_migration_replica_dir()
{
    if (status() != partition_status::type::PS_SECONDARY) {
        dwarn_replica("disk replica migration request(gpid={}, origin={}, target={}) but "
                      "has completed copy checkpoint but the partition_status = {}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      enum_to_string(status()));
        reset_replica_migration_status();
        return;
    }

    dsn::utils::filesystem::rename_path(_dir, fmt::format("{}.{}", _dir, "disk.balance.gar"));
    dsn::utils::filesystem::rename_path(_disk_replica_migration_target_temp_dir,
                                        _disk_replica_migration_target_dir);

    ddebug_replica(
        "disk replica migration request(gpid={}, origin={}, target={})"
        "has closed origin replica, partition_status = {}, wait reload new dir in learning",
        req.pid.to_string(),
        req.origin_disk,
        req.target_disk,
        enum_to_string(status()));
}
}
}