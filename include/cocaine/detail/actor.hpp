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

#ifndef COCAINE_ACTOR_HPP
#define COCAINE_ACTOR_HPP

#include "cocaine/common.hpp"
#include "cocaine/json.hpp"

#include "cocaine/asio/reactor.hpp"
#include "cocaine/asio/tcp.hpp"

#include <set>
#include <thread>

namespace cocaine {

class dispatch_t;

class actor_t:
    boost::noncopyable
{
    public:
        actor_t(const std::shared_ptr<io::reactor_t>& reactor,
                std::unique_ptr<dispatch_t>&& dispatch,
                uint16_t port = 0);

       ~actor_t();

        void
        run();

        void
        terminate();

    public:
        io::tcp::endpoint
        endpoint() const;

        dispatch_t&
        dispatch();

    private:
        void
        on_connection(const std::shared_ptr<io::socket<io::tcp>>& socket);

        void
        on_message(int fd, const io::message_t& message);

        void
        on_disconnect(int fd, const std::error_code& ec);

        void
        on_terminate(ev::async&, int);

    private:
        const std::shared_ptr<io::reactor_t> m_reactor;
        const std::unique_ptr<dispatch_t> m_dispatch;

        // Event loop

        ev::async m_terminate;

        // Actor I/O

        std::unique_ptr<
            io::connector<io::acceptor<io::tcp>>
        > m_connector;

        std::map<
            int,
            std::shared_ptr<io::channel<io::socket<io::tcp>>>
        > m_channels;

        // Execution context

        std::unique_ptr<std::thread> m_thread;
};

} // namespace cocaine

#endif
