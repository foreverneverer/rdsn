// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once
#include <dsn/service_api_c.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/tool-api/zlocks.h>
#include <dsn/tool-api/task.h>

namespace dsn {
namespace tool {

/**
 * latency_tracer is a simple tool for tracking request time consuming in different stages, which
 * can help user find the latency bottleneck. user needs to use it to "add_point" in one stage,
 * which will record the name of point and the time_used. when the request is finshed, you can dump
 * the formated result.
 *
 * for example: one request experiences four stages, latency_tracer need be held by request and
 * passes all stages:
 * class request {
 *      latency_tracer tracer
 * }
 * void start(request req){
 *      req.tracer.add_point("start", now);
 * };
 * void stageA(request req){
 *      req.tracer.add_point("stageA", now);
 * };
 * void stageB(request req){
 *      req.tracer.add_point("stageB", now, true);
 * };
 * void end(request req){
 *      req.tracer.add_point("end", now);
 * };
 *
 *  point1     point2     point3    point4
 *    |         |           |         |
 *    |         |           |         |
 *  start---->stageA----->stageB---->end
 *
 * the "points" will record the all points' time_used_from_previous and time_used_from_start
**/

struct latency_tracer
{

public:
    dsn::zrwlock_nr point_lock;
    dsn::zrwlock_nr link_tracer_lock;

    uint64_t id;
    std::string type;
    std::map<int64_t, std::string> points;

    bool is_open = false;

    std::vector<std::shared_ptr<latency_tracer>> link_tracers;

public:
    latency_tracer(int id, const std::string &start_name, const std::string &type)
        : id(id), type(type)
    {
        points[dsn_now_ns()] = start_name;
    };

    // this method is called for any other method which will be recorded methed name and ts
    //
    // -name: generally, it is the name of that call this method. but you can define the more
    // significant name to show the events of one moment
    // -ts: current timestamp
    void add_point(std::string name)
    {
        if (!is_open) {
            return;
        }

        int64_t ts = dsn_now_ns();
        dsn::zauto_write_lock write(point_lock);
        points[ts] = name;
    }

    void open_trace(bool open) { is_open = open; }

    void add_link_tracer(std::shared_ptr<latency_tracer> link_tracer)
    {
        if (!is_open) {
            return;
        }

        dsn::zauto_write_lock write(link_tracer_lock);

        link_tracers.emplace_back(link_tracer);
    }

    void dump_trace_points(int threshold)
    {
        if (threshold <= 0 || !is_open) {
            return;
        }

        if (points.empty()) {
            return;
        }

        dsn::zauto_read_lock read(point_lock);

        int64_t start_time = points.begin()->first;
        int64_t time_used = points.rbegin()->first - start_time;

        // derror_f("TEST:id={},s={},e={},u={}", id, start_time, points.rbegin()->first, time_used);

        if (time_used < threshold) {
            return;
        }

        int64_t previous_time = points.begin()->first;
        std::string trace;
        for (const auto &point : points) {
            trace = fmt::format(
                "{}\n\tTRACER[{:<10}|{:<10}]:name={:<40}, from_previous={:<20}, from_start={:<20}, "
                "ts={:<13}",
                trace,
                type,
                id,
                point.second,
                point.first - previous_time,
                point.first - start_time,
                point.first);
            previous_time = point.first;
        }

        derror_f("TRACE:time_used={}\n{}", time_used, trace);

        for (auto const &tracer : link_tracers) {
            derror_f("TRACE:link------->id[{}]", tracer->id);
            tracer->dump_trace_points(1);
        }
    }
};
} // namespace tool
} // namespace dsn