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

#include "cocaine/detail/actor.hpp"

#include "cocaine/asio/acceptor.hpp"
#include "cocaine/asio/connector.hpp"
#include "cocaine/asio/socket.hpp"
#include "cocaine/asio/tcp.hpp"

#include "cocaine/context.hpp"
#include "cocaine/dispatch.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/messages.hpp"

#include "cocaine/rpc/channel.hpp"

using namespace cocaine;
using namespace cocaine::io;
using namespace std::placeholders;

namespace {
    struct upstream_t:
        public api::stream_t
    {
        upstream_t(const std::shared_ptr<channel<io::socket<tcp>>>& channel,
                   uint64_t band):
            m_channel(channel),
            m_band(band)
        { }

        virtual
        void
        write(const char* chunk, size_t size) {
            auto ptr = m_channel.lock();

            if(ptr) {
                ptr->wr->write<rpc::chunk>(m_band, std::string(chunk, size));
            }
        }

        virtual
        void
        error(error_code code, const std::string& reason) {
            auto ptr = m_channel.lock();

            if(ptr) {
                ptr->wr->write<rpc::error>(m_band, code, reason);
            }
        }

        virtual
        void
        close() {
            auto ptr = m_channel.lock();

            if(ptr) {
                ptr->wr->write<rpc::choke>(m_band);
            }
        }

    private:
        const std::weak_ptr<channel<io::socket<tcp>>> m_channel;
        const uint64_t m_band;
    };
}

actor_t::actor_t(const std::shared_ptr<reactor_t>& reactor,
                 std::unique_ptr<dispatch_t>&& dispatch,
                 uint16_t port):
    m_reactor(reactor),
    m_dispatch(std::move(dispatch)),
    m_terminate(m_reactor->native())
{
    tcp::endpoint endpoint("127.0.0.1", port);

    try {
        m_connector.reset(new connector<acceptor<tcp>>(
            *m_reactor,
            std::unique_ptr<acceptor<tcp>>(new acceptor<tcp>(endpoint)))
        );
    } catch(const cocaine::io_error_t& e) {
        throw configuration_error_t(
            "unable to bind at '%s' - %s - %s",
            endpoint,
            e.what(),
            e.describe()
        );
    }

    m_connector->bind(std::bind(&actor_t::on_connection, this, _1));

    m_terminate.set<actor_t, &actor_t::on_terminate>(this);
    m_terminate.start();
}

actor_t::~actor_t() {
    // Empty.
}

void
actor_t::run() {
    BOOST_ASSERT(!m_thread);

    auto runnable = std::bind(
        &reactor_t::run,
        m_reactor
    );

    m_thread.reset(new std::thread(runnable));
}

void
actor_t::terminate() {
    BOOST_ASSERT(m_thread);

    m_terminate.send();

    m_thread->join();
    m_thread.reset();
}

tcp::endpoint
actor_t::endpoint() const {
    return m_connector->endpoint();
}

dispatch_t&
actor_t::dispatch() {
    return *m_dispatch;
}

void
actor_t::on_connection(const std::shared_ptr<io::socket<tcp>>& socket_) {
    auto fd = socket_->fd();
    auto channel_ = std::make_shared<channel<io::socket<tcp>>>(*m_reactor, socket_);

    channel_->rd->bind(
        std::bind(&actor_t::on_message,    this, fd, _1),
        std::bind(&actor_t::on_disconnect, this, fd, _1)
    );

    channel_->wr->bind(
        std::bind(&actor_t::on_disconnect, this, fd, _1)
    );

    m_channels[fd] = channel_;
}

void
actor_t::on_message(int fd,
                    const message_t& message)
{
    auto it = m_channels.find(fd);

    if(it == m_channels.end()) {
        return;
    }

    m_dispatch->invoke(message, std::make_shared<upstream_t>(
        it->second,
        message.band()
    ));
}

void
actor_t::on_disconnect(int fd,
                       const std::error_code& /* ec */)
{
    m_channels.erase(fd);
}

void
actor_t::on_terminate(ev::async&, int) {
    m_reactor->stop();
}

