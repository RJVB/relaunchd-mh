/*
 * Copyright (c) 2022 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <iostream>
#include <stdexcept>
#include <string>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "rpc_server.h"
#include "log.h"
#include "manager.h"

static void _rpc_op_disable(Channel &chan, const json &args) {
    const auto &label = args[1]["Label"];
    manager_set_job_enabled(label, false);
    chan.writeMessage("OK");
}

static void _rpc_op_enable(Channel &chan, const json &args) {
    const auto &label = args[1]["Label"];
    manager_set_job_enabled(label, true);
    chan.writeMessage("OK");
}

static void _rpc_op_kill(Channel &chan, const json &args) {
    const std::string &label = args[1]["Label"];
    const std::string &signame_or_num = args[1]["Signal"];
    auto job = manager_get_job_by_label(label);
    job.kill(signame_or_num);
    chan.writeMessage("OK");
}

static void _rpc_op_list(Channel &chan, const json &) {
    // FIXME: handle Label argument
    auto msg = manager_list_jobs();
    chan.writeMessage(msg);
}

static void _rpc_op_load_or_unload(Channel &chan, const json &args) {
    // FIXME: handle Force and OverrideDisabled arguments
    int (*manager_func)(const std::filesystem::path &);
    if (args[0] == "load") {
        manager_func = &manager_load_manifest;
    } else if (args[0] == "unload") {
        manager_func = &manager_unload_manifest;
    } else {
        throw std::logic_error("unsupported operation");
    }
    for (const auto &path : args[1]["Paths"]) {
        if (!std::filesystem::exists(path)) {
            chan.writeMessage("ERROR-TODO");
            return;
        }
        if (std::filesystem::is_directory(path)) {
            using std::filesystem::directory_iterator;
            for (const auto &file: directory_iterator(path)) {
                (*manager_func)(file.path());
            }
        } else {
            std::filesystem::path p{path.get<std::string>()};
            (*manager_func)(p);
        }
    }

    chan.writeMessage("OK");
}

static void _rpc_op_start(Channel &chan, const json &args) {
    const std::string &label = args[1]["Label"];
    auto job = manager_get_job_by_label(label);
    job.run();
    chan.writeMessage("OK");
}

static void _rpc_op_stop(Channel &chan, const json &args) {
    const std::string &label = args[1]["Label"];
    auto job = manager_get_job_by_label(label);
    job.kill("SIGTERM");
    chan.writeMessage("OK");
}

static void _rpc_op_remove(Channel &chan, const json &args) {
    const auto &label = args[1]["Label"];
    manager_unload_by_label(label);
    chan.writeMessage("OK");
}

static void _rpc_op_submit(Channel &chan, const json &args) {
    std::string path = ""; // TODO: maybe create a fake path? do we even need this?
    manager_load_manifest(args[1], path);
    chan.writeMessage("OK");
}

static void _rpc_op_version(Channel &chan, const json &) {
    chan.writeMessage("relaunchd version unknown"); // FIXME get version number
}

// FIXME: needs a lot more error checking
int rpc_dispatch(Channel &chan) {
    static const std::unordered_map<std::string, void(*)(Channel &chan, const json &j)> handlers = {
            {"disable", _rpc_op_disable},
            {"enable", _rpc_op_enable},
            {"kill", _rpc_op_kill},

            {"list", _rpc_op_list},
            {"load", _rpc_op_load_or_unload},
            {"remove", _rpc_op_remove},
            {"start", _rpc_op_start},
            {"submit", _rpc_op_submit},

            {"stop", _rpc_op_stop},
            {"unload", _rpc_op_load_or_unload},
            {"version", _rpc_op_version},
    };
    chan.accept();
    try {
        auto msg = chan.readMessage();
        auto method = msg.at(0).get<std::string>();
        auto funcptr = handlers.at(method);
        (*funcptr)(chan, msg);
    } catch (const std::exception &exc) {
        log_error("dispatch failed: %s", exc.what());
    }

    chan.disconnect();

    return 0;
}

