#pragma once
#include <dsn/service_api_cpp.h>
#include "deploy_svc.types.h"

namespace dsn {
namespace dist {
// define your own thread pool using DEFINE_THREAD_POOL_CODE(xxx)
// define RPC task code for service 'deploy_svc'
DEFINE_TASK_CODE_RPC(RPC_DEPLOY_SVC_DEPLOY_SVC_DEPLOY,
                     TASK_PRIORITY_COMMON,
                     ::dsn::THREAD_POOL_DEFAULT)
DEFINE_TASK_CODE_RPC(RPC_DEPLOY_SVC_DEPLOY_SVC_UNDEPLOY,
                     TASK_PRIORITY_COMMON,
                     ::dsn::THREAD_POOL_DEFAULT)
DEFINE_TASK_CODE_RPC(RPC_DEPLOY_SVC_DEPLOY_SVC_GET_SERVICE_LIST,
                     TASK_PRIORITY_COMMON,
                     ::dsn::THREAD_POOL_DEFAULT)
DEFINE_TASK_CODE_RPC(RPC_DEPLOY_SVC_DEPLOY_SVC_GET_SERVICE_INFO,
                     TASK_PRIORITY_COMMON,
                     ::dsn::THREAD_POOL_DEFAULT)
DEFINE_TASK_CODE_RPC(RPC_DEPLOY_SVC_DEPLOY_SVC_GET_CLUSTER_LIST,
                     TASK_PRIORITY_COMMON,
                     ::dsn::THREAD_POOL_DEFAULT)
// test timer task code
DEFINE_TASK_CODE(LPC_DEPLOY_SVC_TEST_TIMER, TASK_PRIORITY_COMMON, ::dsn::THREAD_POOL_DEFAULT)
}
}
