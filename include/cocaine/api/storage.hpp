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

#ifndef COCAINE_STORAGE_API_HPP
#define COCAINE_STORAGE_API_HPP

#include "cocaine/common.hpp"
#include "cocaine/json.hpp"
#include "cocaine/repository.hpp"
#include "cocaine/traits.hpp"

#include <mutex>
#include <sstream>

namespace cocaine {

struct storage_error_t:
    public error_t
{
    template<typename... Args>
    storage_error_t(const std::string& format,
                    const Args&... args):
        error_t(format, args...)
    { }
};

namespace api {

class storage_t {
    public:
        typedef storage_t category_type;

    public:
        virtual
       ~storage_t() {
            // Empty.
        }

        virtual
        std::string
        read(const std::string& collection,
             const std::string& key) = 0;

        template<class T>
        T
        get(const std::string& collection,
            const std::string& key);

        virtual
        void
        write(const std::string& collection,
              const std::string& key,
              const std::string& blob) = 0;

        template<class T>
        void
        put(const std::string& collection,
            const std::string& key,
            const T& object);

        virtual
        std::vector<std::string>
        list(const std::string& collection) = 0;

        virtual
        void
        remove(const std::string& collection,
               const std::string& key) = 0;

    protected:
        storage_t(context_t&,
                  const std::string& /* name */,
                  const Json::Value& /* args */)
        { }
};

template<class T>
T
storage_t::get(const std::string& collection,
               const std::string& key)
{
    T result;
    msgpack::unpacked unpacked;

    std::string blob(read(collection, key));

    try {
        msgpack::unpack(&unpacked, blob.data(), blob.size());
    } catch(const msgpack::unpack_error& e) {
        throw storage_error_t("corrupted object");
    }

    try {
        io::type_traits<T>::unpack(unpacked.get(), result);
    } catch(const msgpack::type_error& e) {
        throw storage_error_t("object type mismatch");
    }

    return result;
}

template<class T>
void
storage_t::put(const std::string& collection,
               const std::string& key,
               const T& object)
{
    std::ostringstream buffer;
    msgpack::packer<std::ostringstream> packer(buffer);

    io::type_traits<T>::pack(packer, object);

    write(collection, key, buffer.str());
}

template<>
struct category_traits<storage_t> {
    typedef std::shared_ptr<storage_t> ptr_type;

    struct factory_type:
        public basic_factory<storage_t>
    {
        virtual
        ptr_type
        get(context_t& context,
            const std::string& name,
            const Json::Value& args) = 0;
    };

    template<class T>
    struct default_factory:
        public factory_type
    {
        virtual
        ptr_type
        get(context_t& context,
            const std::string& name,
            const Json::Value& args)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            typename instance_map_t::iterator it(m_instances.find(name));

            ptr_type instance;

            if(it != m_instances.end()) {
                instance = it->second.lock();
            }

            if(!instance) {
                instance = std::make_shared<T>(
                    std::ref(context),
                    name,
                    args
                );

                m_instances.emplace(name, instance);
            }

            return instance;
        }

    private:
#if BOOST_VERSION >= 103600
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            std::string,
            std::weak_ptr<storage_t>
        > instance_map_t;

        instance_map_t m_instances;
        std::mutex m_mutex;
    };
};

category_traits<storage_t>::ptr_type
storage(context_t& context,
        const std::string& name);

}} // namespace cocaine::api

#endif
