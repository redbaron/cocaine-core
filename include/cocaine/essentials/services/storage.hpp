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

#ifndef COCAINE_STORAGE_SERVICE_HPP
#define COCAINE_STORAGE_SERVICE_HPP

#include "cocaine/api/service.hpp"
#include "cocaine/api/storage.hpp"

namespace cocaine { namespace service {

using io::reactor_t;

class storage_t:
    public api::service_t
{
    public:
        storage_t(context_t& context,
                  reactor_t& reactor,
                  const std::string& name,
                  const Json::Value& args);

    private:
        // NOTE: This will keep the underlying storage active, as opposed to
        // the usual usecase when the storage object is destroyed after the
        // node service finishes its initialization.
        api::category_traits<api::storage_t>::ptr_type m_storage;
};

}} // namespace cocaine::service

#endif
