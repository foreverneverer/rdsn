/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 *
 * -=- Robust Distributed System Nucleus (rDSN) -=-
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "native_linux_aio_provider.h"

#include <dsn/dist/fmt_logging.h>
#include <dsn/tool-api/async_calls.h>
#include <dsn/perf_counter/perf_counter_wrapper.h>

namespace dsn {
const std::string code = "LPC_WRITE_REPLICATION_LOG_SHARED";

dsn::perf_counter_wrapper aio_create2submit_latency;
dsn::perf_counter_wrapper aio_submit2exec_latency;
dsn::perf_counter_wrapper aio_exec2complete_latency;

dsn::perf_counter_wrapper callback_submit2exec_latency;
dsn::perf_counter_wrapper callback_exec2complete_latency;

native_linux_aio_provider::native_linux_aio_provider(disk_engine *disk) : aio_provider(disk)
{
    static std::once_flag flag;
    std::call_once(flag, [&]() {
        aio_create2submit_latency.init_global_counter(
            "replica",
            "app.pegasus",
            "aio_create2submit_latency",
            COUNTER_TYPE_NUMBER_PERCENTILES,
            "statistic the through bytes of rocksdb write rate limiter");

        aio_submit2exec_latency.init_global_counter(
            "replica",
            "app.pegasus",
            "aio_submit2exec_latency",
            COUNTER_TYPE_NUMBER_PERCENTILES,
            "statistic the through bytes of rocksdb write rate limiter");

        aio_exec2complete_latency.init_global_counter(
            "replica",
            "app.pegasus",
            "aio_exec2complete_latency",
            COUNTER_TYPE_NUMBER_PERCENTILES,
            "statistic the through bytes of rocksdb write rate limiter");

        callback_submit2exec_latency.init_global_counter(
            "replica",
            "app.pegasus",
            "callback_submit2exec_latency",
            COUNTER_TYPE_NUMBER_PERCENTILES,
            "statistic the through bytes of rocksdb write rate limiter");

        callback_exec2complete_latency.init_global_counter(
            "replica",
            "app.pegasus",
            "callback_exec2complete_latency",
            COUNTER_TYPE_NUMBER_PERCENTILES,
            "statistic the through bytes of rocksdb write rate limiter");
    });
}

native_linux_aio_provider::~native_linux_aio_provider() {}

dsn_handle_t native_linux_aio_provider::open(const char *file_name, int flag, int pmode)
{
    dsn_handle_t fh = (dsn_handle_t)(uintptr_t)::open(file_name, flag, pmode);
    if (fh == DSN_INVALID_FILE_HANDLE) {
        derror("create file failed, err = %s", strerror(errno));
    }
    return fh;
}

error_code native_linux_aio_provider::close(dsn_handle_t fh)
{
    if (fh == DSN_INVALID_FILE_HANDLE || ::close((int)(uintptr_t)(fh)) == 0) {
        return ERR_OK;
    } else {
        derror("close file failed, err = %s", strerror(errno));
        return ERR_FILE_OPERATION_FAILED;
    }
}

error_code native_linux_aio_provider::flush(dsn_handle_t fh)
{
    if (fh == DSN_INVALID_FILE_HANDLE || ::fsync((int)(uintptr_t)(fh)) == 0) {
        return ERR_OK;
    } else {
        derror("flush file failed, err = %s", strerror(errno));
        return ERR_FILE_OPERATION_FAILED;
    }
}

error_code native_linux_aio_provider::write(const aio_context &aio_ctx,
                                            /*out*/ uint32_t *processed_bytes)
{
    ssize_t ret = pwrite(static_cast<int>((ssize_t)aio_ctx.file),
                         aio_ctx.buffer,
                         aio_ctx.buffer_size,
                         aio_ctx.file_offset);
    if (ret < 0) {
        return ERR_FILE_OPERATION_FAILED;
    }
    *processed_bytes = static_cast<uint32_t>(ret);
    return ERR_OK;
}

error_code native_linux_aio_provider::read(const aio_context &aio_ctx,
                                           /*out*/ uint32_t *processed_bytes)
{
    ssize_t ret = pread(static_cast<int>((ssize_t)aio_ctx.file),
                        aio_ctx.buffer,
                        aio_ctx.buffer_size,
                        aio_ctx.file_offset);
    if (ret < 0) {
        return ERR_FILE_OPERATION_FAILED;
    }
    if (ret == 0) {
        return ERR_HANDLE_EOF;
    }
    *processed_bytes = static_cast<uint32_t>(ret);
    return ERR_OK;
}

void native_linux_aio_provider::submit_aio_task(aio_task *aio_tsk)
{
    if (aio_tsk->code().to_string() == code) {
        aio_tsk->slog = true;
    }
    aio_tsk->aioSubmitTime = dsn_now_ns();
    if (aio_tsk->slog) {
        aio_create2submit_latency->set(aio_tsk->aioSubmitTime - aio_tsk->aioCreateTime);
    }

    tasking::enqueue(aio_tsk->code(),
                     aio_tsk->tracker(),
                     [=]() { aio_internal(aio_tsk, true); },
                     aio_tsk->hash());
}

error_code native_linux_aio_provider::aio_internal(aio_task *aio_tsk,
                                                   bool async,
                                                   /*out*/ uint32_t *pbytes /*= nullptr*/)
{
    aio_tsk->aioExecTime = dsn_now_ns();
    if (aio_tsk->slog) {
        aio_submit2exec_latency->set(aio_tsk->aioExecTime - aio_tsk->aioSubmitTime);
    }

    aio_context *aio_ctx = aio_tsk->get_aio_context();
    error_code err = ERR_UNKNOWN;
    uint32_t processed_bytes = 0;
    switch (aio_ctx->type) {
    case AIO_Read:
        err = read(*aio_ctx, &processed_bytes);
        break;
    case AIO_Write:
        err = write(*aio_ctx, &processed_bytes);
        break;
    default:
        return err;
    }

    if (pbytes) {
        *pbytes = processed_bytes;
    }

    if (async) {
        aio_tsk->aioCompleteTime = dsn_now_ns();
        aio_tsk->callbackSubmitTime = dsn_now_ns();
        if (aio_tsk->slog) {
            aio_exec2complete_latency->set(aio_tsk->aioCompleteTime - aio_tsk->aioExecTime);
        }
        complete_io(aio_tsk, err, processed_bytes);
    } else {
        utils::notify_event notify;
        notify.notify();
    }

    return err;
}

} // namespace dsn
