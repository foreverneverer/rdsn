#include <dsn/utility/smart_pointers.h>
#include <dsn/tool-api/async_calls.h>
#include <dsn/dist/nfs_node.h>
#include <dsn/dist/fmt_logging.h>
#include <boost/algorithm/string.hpp>
#include <dsn/utility/filesystem.h>

#include "nfs_node_simple.h"

namespace dsn {

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t written = fwrite(ptr, size, nmemb, stream);
    dassert_f(written == nmemb, "unconplete!!!!!!!!!!");
    return written;
}

std::unique_ptr<nfs_node> nfs_node::create()
{
    curl_global_init(CURL_GLOBAL_ALL);
    return dsn::make_unique<dsn::service::nfs_node_simple>();
}

aio_task_ptr nfs_node::copy_remote_directory(rpc_address remote,
                                             const std::string &source_dir,
                                             const std::string &dest_dir,
                                             bool overwrite,
                                             bool high_priority,
                                             task_code callback_code,
                                             task_tracker *tracker,
                                             aio_handler &&callback,
                                             int hash)
{
    return copy_remote_files(remote,
                             source_dir,
                             {},
                             dest_dir,
                             overwrite,
                             high_priority,
                             callback_code,
                             tracker,
                             std::move(callback),
                             hash);
}

std::atomic<int> count;
aio_task_ptr nfs_node::copy_remote_files(rpc_address remote,
                                         const std::string &source_dir,
                                         const std::vector<std::string> &files,
                                         const std::string &dest_dir,
                                         bool overwrite,
                                         bool high_priority,
                                         task_code callback_code,
                                         task_tracker *tracker,
                                         aio_handler &&callback,
                                         int hash)
{
    // auto cb = dsn::file::create_aio_task(callback_code, tracker, std::move(callback), hash);

    std::shared_ptr<remote_copy_request> rci = std::make_shared<remote_copy_request>();
    rci->source = remote;
    rci->source_dir = source_dir;
    rci->files = files;
    rci->dest_dir = dest_dir;
    rci->overwrite = overwrite;
    rci->high_priority = high_priority;
    // call(rci, cb);

    CURLcode res;
    FILE *fp;
    CURL *curl = curl_easy_init();
    // derror_f("AAAAAAA: ={}", files.size());
    for (const auto &file_name : files) {
        // derror_f("HHH={}", file_name);
        std::string url =
            fmt::format("http://{}:8000{}/{}", remote.ipv4_str(), source_dir, file_name);
        std::vector<std::string> args;
        boost::split(args, file_name, boost::is_any_of("/"));
        if (args.size() != 2) {
            derror_f("FATATL: {}", file_name);
            continue;
        }
        if (!dsn::utils::filesystem::file_exists(fmt::format("{}/{}", dest_dir, args[0]))) {
            dsn::utils::filesystem::create_directory(fmt::format("{}/{}", dest_dir, args[0]));
        }
        std::string dst = fmt::format("{}/{}", dest_dir, file_name);
        // derror_f("WARN:{}=>{}", url, dst);
        if (curl) {
            fp = fopen(dst.c_str(), "wb");
            dassert_f(fp, "nullllllll");
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                derror_f("jiashuo_debug: ERROR copy {}=>{}", res, url);
                callback(ERR_FILE_OPERATION_FAILED, 0);
            }
            /* always cleanup */
            derror_f("jiashuo_debug: complete copy {}=>{}", url, dst);
            fclose(fp);
        } else {
            dassert_f("curl init failed = {}", "null");
        }
    }
    curl_easy_cleanup(curl);
    callback(ERR_OK, 1000);
    return nullptr;
}
}
