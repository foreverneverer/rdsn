// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include <dsn/utils/latency_tracer.h>
#include <dsn/service_api_c.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/flags.h>

#include <utility>
#include <dsn/utility/singleton.h>
#include "lockp.std.h"

namespace dsn {
namespace utils {

DSN_DEFINE_bool("replication", enable_latency_tracer, false, "whether enable the latency tracer");
DSN_DEFINE_bool("replication",
                open_latency_tracer_report,
                false,
                "whether open the latency tracer report perf counter");

const std::string kStageLinkFlag = "stage_to_link";
const std::string kReportCounterName = "trace_latency";
utils::rw_lock_nr _counter_lock;
std::map<std::string, perf_counter_ptr> _counters_trace_latency;

perf_counter_ptr get_trace_counter(const std::string &name)
{
    utils::auto_write_lock read(_counter_lock);
    auto iter = _counters_trace_latency.find(name);
    if (iter != _counters_trace_latency.end()) {
        return iter->second;
    }
    return nullptr;
}

perf_counter_ptr init_trace_counter(const std::string &name)
{
    utils::auto_write_lock write(_counter_lock);
    auto iter = _counters_trace_latency.find(name);
    if (iter != _counters_trace_latency.end()) {
        return iter->second;
    }

    auto perf_counter =
        dsn::perf_counters::instance().get_app_counter(kReportCounterName.c_str(),
                                                       name.c_str(),
                                                       COUNTER_TYPE_NUMBER_PERCENTILES,
                                                       name.c_str(),
                                                       true);

    _counters_trace_latency.emplace(name, perf_counter);
    return perf_counter;
}

latency_tracer::latency_tracer(std::string name, bool is_sub, uint64_t threshold)
    : _parent_name("root"),
      _name(std::move(name)),
      _threshold(threshold),
      _is_sub(is_sub),
      _start_time(dsn_now_ns())
{
}

latency_tracer::~latency_tracer()
{
    if (_is_sub) {
        return;
    }

    std::string traces;
    dump_trace_points(traces);
}

void latency_tracer::add_point(const std::string &function_line,
                               const std::string &function_name,
                               const std::string &extern_info)
{
    if (!FLAGS_enable_latency_tracer) {
        return;
    }

    uint64_t ts = dsn_now_ns();
    utils::auto_write_lock write(_point_lock);
    _points[ts] = stage(function_line, function_name, extern_info);
}

void latency_tracer::add_sub_tracer(const std::string &root,
                                    const std::string &name,
                                    const std::shared_ptr<latency_tracer> &tracer)
{
    if (!FLAGS_enable_latency_tracer) {
        return;
    }

    {
        uint64_t ts = dsn_now_ns();
        utils::auto_write_lock write(_point_lock);
        _points[ts] = stage(root, name, kStageLinkFlag);
    }

    utils::auto_write_lock write(_sub_lock);
    tracer->set_parent_name(root);
    auto iter = _sub_tracers.find(root);
    if (iter != _sub_tracers.end()) {
        iter->second.emplace(name, tracer);
    }
    _sub_tracers[root] = {{name, tracer}};
}

void latency_tracer::dump_trace_points(/*out*/ std::string &traces)
{
    if (!FLAGS_enable_latency_tracer || _threshold < 0 || _points.empty()) {
        return;
    }

    utils::auto_read_lock read_point(_point_lock);
    utils::auto_read_lock read_sub(_sub_lock);
    uint64_t time_used = _points.rbegin()->first - _start_time;

    traces.append(fmt::format("\t***************[TRACE:{}]***************\n", _name));
    uint64_t previous_point_ts = _start_time;
    struct stage previous_point_stage(_parent_name, _parent_name, _parent_name);
    for (const auto &point : _points) {
        if (point.second.extern_info == kStageLinkFlag) {
            auto iter = _sub_tracers.find(
                fmt::format("{}_{}", point.second.function_name, point.second.extern_info));
            if (iter != _sub_tracers.end()) {
                for (const auto &sub : iter->second) {
                    sub.second->dump_trace_points(traces);
                }
            }
            continue;
        }

        auto stage = point.second;
        auto ts = point.first;
        auto span_duration = point.first - previous_point_ts;
        auto total_latency = point.first - _start_time;

        if (FLAGS_open_latency_tracer_report) {
            std::string counter_name =
                fmt::format("{}@{}", previous_point_stage.function_name, stage.function_name);
            report_trace_point(counter_name, total_latency);
        }

        if (time_used >= _threshold) {
            auto stage_name =
                _is_sub
                    ? fmt::format("  {}@{}_{}",
                                  _parent_name,
                                  point.second.function_name,
                                  point.second.extern_info)
                    : fmt::format("{}_{}", point.second.function_name, point.second.extern_info);
            std::string trace = fmt::format("\tTRACE:stage={:<70}, span={:>20}, total={:>20}, "
                                            "ts={:<20}\n",
                                            stage_name,
                                            span_duration,
                                            total_latency,
                                            ts);
            traces.append(trace);
        }

        previous_point_ts = ts;
        previous_point_stage = stage;
    }

    if (!_is_sub) {
        if (time_used >= _threshold) {
            dwarn_f("TRACE:the traces as fallow:\n{}", traces);
        }
        return;
    }
}

void latency_tracer::report_trace_point(const std::string &name, uint64_t span)
{
    auto perf_counter = get_trace_counter(name);
    if (!perf_counter) {
        dwarn_f("installed latency tracer counter({}) for it has not been ready", name);
        perf_counter = init_trace_counter(name);
    }
    perf_counter->set(span);
}

} // namespace utils
} // namespace dsn
