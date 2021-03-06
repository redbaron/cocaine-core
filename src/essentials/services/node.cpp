/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cocaine/essentials/services/node.hpp"

#include "cocaine/api/storage.hpp"

#include "cocaine/app.hpp"
#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/messages.hpp"

#include "cocaine/detail/traits/json.hpp"

#include <tuple>

using namespace cocaine::service;
using namespace std::placeholders;

namespace {
    typedef std::map<
        std::string,
        std::string
    > runlist_t;
}

node_t::node_t(context_t& context,
               reactor_t& reactor,
               const std::string& name,
               const Json::Value& args):
    category_type(context, reactor, name, args),
    m_context(context),
    m_log(new logging::log_t(context, name)),
    m_zmq_context(1),
    m_announces(m_zmq_context, ZMQ_PUB),
    m_announce_timer(reactor.native()),
#if defined(__clang__) || defined(HAVE_GCC47)
    m_birthstamp(std::chrono::steady_clock::now())
#else
    m_birthstamp(std::chrono::monotonic_clock::now())
#endif
{
    on<io::node::start_app>("start_app", std::bind(&node_t::on_start_app, this, _1));
    on<io::node::pause_app>("pause_app", std::bind(&node_t::on_pause_app, this, _1));
    on<io::node::info     >("info",      std::bind(&node_t::on_info,      this));

    // Configuration

    const ev::tstamp interval = args.get("announce-interval", 5.0f).asDouble();
    const std::string runlist_id = args.get("runlist", "default").asString();

    // Autodiscovery

    if(!args["announce"].empty()) {
#if ZMQ_VERSION > 30200
        int watermark = 5;
#else
        uint64_t watermark = 5;
#endif

        // Avoids announce data being cached for every disconnected peer.
#if ZMQ_VERSION > 30200
        m_announces.setsockopt(ZMQ_SNDHWM, &watermark, sizeof(watermark));
#else
        m_announces.setsockopt(ZMQ_HWM, &watermark, sizeof(watermark));
#endif

        for(Json::Value::const_iterator it = args["announce"].begin();
            it != args["announce"].end();
            ++it)
        {
            std::string endpoint = (*it).asString();

            COCAINE_LOG_INFO(
                m_log,
                "announcing this node on %s every %.01f %s",
                endpoint,
                interval,
                interval == 1.0f ? "second" : "seconds"
            );

            try {
                m_announces.bind(endpoint.c_str());
            } catch(const zmq::error_t& e) {
                throw configuration_error_t(
                    "unable to bind at '%s' - %s",
                    endpoint,
                    e.what()
                );
            }
        }

        m_announce_timer.set<node_t, &node_t::on_announce>(this);
        m_announce_timer.start(0.0f, interval);
    }

    // Runlist

    runlist_t runlist;

    COCAINE_LOG_INFO(m_log, "reading the '%s' runlist", runlist_id);

    try {
        runlist = api::storage(m_context, "core")->get<
            std::map<std::string, std::string>
        >("runlists", runlist_id);
    } catch(const cocaine::error_t& e) {
        COCAINE_LOG_WARNING(
            m_log,
            "unable to read the '%s' runlist - %s",
            runlist_id,
            e.what()
        );
    }

    if(!runlist.empty()) {
        COCAINE_LOG_INFO(
            m_log,
            "starting %d %s",
            runlist.size(),
            runlist.size() == 1 ? "app" : "apps"
        );

        // NOTE: Ignore the return value here, as there's nowhere to return it.
        // It might be nice to parse and log it in case of errors or simply die.
        on_start_app(runlist);
    }
}

node_t::~node_t() {
    if(!m_apps.empty()) {
        COCAINE_LOG_INFO(m_log, "stopping the apps");
        m_apps.clear();
    }
}

void
node_t::on_announce(ev::timer&, int) {
    zmq::message_t message;

    message.rebuild(m_context.config.network.hostname.size());

    std::memcpy(
        message.data(),
        m_context.config.network.hostname.data(),
        m_context.config.network.hostname.size()
    );

    m_announces.send(message, ZMQ_SNDMORE);

    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer(buffer);

    io::type_traits<Json::Value>::pack(
        packer,
        on_info()
    );

    message.rebuild(buffer.size());

    std::memcpy(
        message.data(),
        buffer.data(),
        buffer.size()
    );

    m_announces.send(message);
}

Json::Value
node_t::on_start_app(const runlist_t& runlist) {
    Json::Value result(Json::objectValue);
    app_map_t::iterator app;

    for(runlist_t::const_iterator it = runlist.begin();
        it != runlist.end();
        ++it)
    {
        if(m_apps.find(it->first) != m_apps.end()) {
            result[it->first] = "the app is already running";
            continue;
        }

        COCAINE_LOG_INFO(m_log, "starting the '%s' app", it->first);

        try {
            std::tie(app, std::ignore) = m_apps.emplace(
                it->first,
                std::make_shared<app_t>(
                    m_context,
                    it->first,
                    it->second
                )
            );
        } catch(const cocaine::error_t& e) {
            COCAINE_LOG_ERROR(
                m_log,
                "unable to initialize the '%s' app - %s",
                it->first,
                e.what()
            );

            result[it->first] = e.what();

            continue;
        }

        try {
            app->second->start();
        } catch(const cocaine::error_t& e) {
            COCAINE_LOG_ERROR(
                m_log,
                "unable to start the '%s' app - %s",
                it->first,
                e.what()
            );

            m_apps.erase(app);

            result[it->first] = e.what();

            continue;
        }

        result[it->first] = "the app has been started";
    }

    return result;
}

Json::Value
node_t::on_pause_app(const std::vector<std::string>& applist) {
    Json::Value result(Json::objectValue);

    for(std::vector<std::string>::const_iterator it = applist.begin();
        it != applist.end();
        ++it)
    {
        app_map_t::iterator app(m_apps.find(*it));

        if(app == m_apps.end()) {
            result[*it] = "the app is not running";
            continue;
        }

        COCAINE_LOG_INFO(m_log, "stopping the '%s' app", *it);

        app->second->stop();
        m_apps.erase(app);

        result[*it] = "the app has been stopped";
    }

    return result;
}

Json::Value
node_t::on_info() const {
    Json::Value result(Json::objectValue);

    for(app_map_t::const_iterator it = m_apps.begin();
        it != m_apps.end();
        ++it)
    {
        result["apps"][it->first] = it->second->info();
    }

    result["identity"] = m_context.config.network.hostname;

    using namespace std::chrono;

    auto uptime = duration_cast<seconds>(
#if defined(__clang__) || defined(HAVE_GCC47)
        steady_clock::now() - m_birthstamp
#else
        monotonic_clock::now() - m_birthstamp
#endif
    );

    result["uptime"] = static_cast<Json::LargestUInt>(uptime.count());

    return result;
}
