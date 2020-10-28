#include "replica.h"
#include "replica_stub.h"

namespace dsn {
namespace replication {

void replica::on_migrate_replica(const migrate_replica_request &req,
                                 /*out*/ migrate_replica_response &resp)
{
    _checker.only_one_thread_access();

    check_replica_on_disk(req, resp);
    init_checkpoint(true);
    migrate_checkpoint(req, resp);
    migrate_app_info(req, resp);
    update_migration_replica(req, resp);
}

void replica::check_replica_on_disk(const migrate_replica_request &req,
                                    /*out*/ migrate_replica_response &resp)
{
    // TODO(jiashuo1) need manager control migattion flow
    if (_disk_replica_migration_status != disk_replica_migration_status::IDLE) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "existed running task, partition_status = {}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      enum_to_string(status()));
        resp.err = ERR_BUSY;
        return;
    }

    // TODO(jiashuo1) auto downgrade to secondary if primary
    if (status() != partition_status::type::PS_SECONDARY) {
        dwarn_replica("received disk replica migration request(gpid={}, origin={}, target={}) but "
                      "invalid partition_status = {}",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      enum_to_string(status()));
        resp.err = ERR_INVALID_STATE;
        return;
    }

    _stub->update_disk_holding_replicas();

    bool valid_origin_disk = false;
    bool valid_target_disk = false;
    for (const auto &dir_node : _stub->_fs_manager._dir_nodes) {

        if (dir_node->tag == req.origin_disk) {
            valid_origin_disk = true;
            std::set<gpid> disk_holding_replicas = dir_node->holding_replicas[req.pid.get_app_id];
            if (disk_holding_replicas.find(req.pid) == disk_holding_replicas.end()) {
                dwarn_replica("received disk replica migration request(gpid={}, origin={}, "
                              "target={}) but not exist on origin disk, partition_status = {}",
                              req.pid.to_string(),
                              req.origin_disk,
                              req.target_disk,
                              enum_to_string(status()));
                resp.err = ERR_OBJECT_NOT_FOUND;
                return;
            }
        }

        if (dir_node->tag == req.target_disk) {
            valid_target_disk = true;
            std::set<gpid> disk_holding_replicas = dir_node->holding_replicas[req.pid.get_app_id];
            if (disk_holding_replicas.find(req.pid) != disk_holding_replicas.end()) {
                dwarn_replica("received disk replica migration request(gpid={}, origin={}, "
                              "target={}) but replica has existed on target disk, partition_status "
                              "= {}",
                              req.pid.to_string(),
                              req.origin_disk,
                              req.target_disk,
                              enum_to_string(status()));
                resp.err = ERR_OBJECT_NOT_FOUND;
                return;
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
        return;
    }

    // TODO(jiashuo1) check if in bulkload,restore,coldbackup,split, if true, return;

    ddebug_replica("received disk replica migration request(gpid={}, origin={}, target={}) "
                   "partition_status = {}",
                   req.pid.to_string(),
                   req.origin_disk,
                   req.target_disk,
                   enum_to_string(status()));
}

void replica::migrate_checkpoint(const migrate_replica_request &req,
                                 /*out*/ migrate_replica_response &resp)
{
    if (false) {

    } else {
        _app->copy_checkpoint_to_dir(req.target_disk.c_str(), 0 /*last_decree*/);
    }
}

void replica::migrate_app_info(const migrate_replica_request &req,
                               /*out*/ migrate_replica_response &resp)
{
}

// set moved
void replica::update_migration_replica(const migrate_replica_request &req,
                                       /*out*/ migrate_replica_response &resp)
{
}
}
}