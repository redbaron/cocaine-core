CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(COCAINE-CORE)

SET(COCAINE_VERSION 0.10)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

CONFIGURE_FILE(
    "${PROJECT_SOURCE_DIR}/config.hpp.in"
    "${PROJECT_SOURCE_DIR}/include/cocaine/config.hpp")

FIND_PACKAGE(Boost 1.40.0
    REQUIRED COMPONENTS
        system
        filesystem
        program_options)

IF(NOT APPLE)
    FIND_PACKAGE(OpenSSL REQUIRED)
    SET(CRYPTO_SOURCES "src/auth")
    SET(CRYPTO_LIBRARY "crypto")
ENDIF()

INCLUDE(cmake/locate_library.cmake)

LOCATE_LIBRARY(LIBEV "ev++.h" "ev" "libev")
LOCATE_LIBRARY(LIBMSGPACK "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(LIBZMQ "zmq.h" "zmq")
LOCATE_LIBRARY(LIBARCHIVE "archive.h" "archive")
LOCATE_LIBRARY(LIBLTDL "ltdl.h" "ltdl")

IF(NOT APPLE)
    LOCATE_LIBRARY(LIBUUID "uuid/uuid.h" "uuid")
    SET(LIBUUID_LIBRARY "uuid")
ENDIF()

IF(NOT APPLE)
    LOCATE_LIBRARY(LIBBFD "bfd.h" "bfd")
    SET(LIBBFD_LIBRARY "bfd" "dl")
ENDIF()

INCLUDE_DIRECTORIES(
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${LIBEV_INCLUDE_DIRS}
    ${LIBMSGPACK_INCLUDE_DIRS}
    ${LIBZMQ_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS}
    ${LIBLTDL_INCLUDE_DIRS})

LINK_DIRECTORIES(
    ${Boost_LIBRARY_DIRS}
    ${LIBEV_LIBRARY_DIRS}
    ${LIBMSGPACK_LIBRARY_DIRS}
    ${LIBZMQ_LIBRARY_DIRS}
    ${LIBARCHIVE_LIBRARY_DIRS}
    ${LIBLTDL_LIBRARY_DIRS})

INCLUDE_DIRECTORIES(BEFORE
    ${PROJECT_SOURCE_DIR}/foreign/jsoncpp-0.6.0-rc2/include
    ${PROJECT_SOURCE_DIR}/foreign/backward-cpp
    ${PROJECT_SOURCE_DIR}/foreign/cppzmq
    ${PROJECT_SOURCE_DIR}/include)

ADD_LIBRARY(json
    foreign/jsoncpp-0.6.0-rc2/src/lib_json/json_value
    foreign/jsoncpp-0.6.0-rc2/src/lib_json/json_reader
    foreign/jsoncpp-0.6.0-rc2/src/lib_json/json_writer)

SET_TARGET_PROPERTIES(json PROPERTIES
    COMPILE_FLAGS "-fPIC")

ADD_LIBRARY(cocaine-core SHARED
    src/actor
    src/api
    src/app
    src/archive
    src/context
    ${CRYPTO_SOURCES}
    src/dispatch
    src/engine
    src/essentials/isolates/process
    src/essentials/loggers/files
    src/essentials/loggers/stdout
    src/essentials/loggers/syslog
    src/essentials/services/logging
    src/essentials/services/node
    src/essentials/services/storage
    src/essentials/storages/files
    src/essentials/module
    src/locator
    src/logging
    src/manifest
    src/profile
    src/repository
    src/session
    src/slave
    src/unique_id)

TARGET_LINK_LIBRARIES(cocaine-core
    archive
    boost_system-mt
    boost_filesystem-mt
    ${CRYPTO_LIBRARY}
    ev
    json
    ltdl
    msgpack
    ${LIBUUID_LIBRARY}
    zmq)

SET_TARGET_PROPERTIES(cocaine-core PROPERTIES
    VERSION 2)

ADD_EXECUTABLE(cocaine-runtime
    src/runtime/pid_file
    src/runtime/runtime)

TARGET_LINK_LIBRARIES(cocaine-runtime
    ${LIBBFD_LIBRARY}
    boost_program_options-mt
    cocaine-core)

SET_TARGET_PROPERTIES(cocaine-core cocaine-runtime PROPERTIES
    COMPILE_FLAGS "-std=c++0x -W -Wall -Werror -pedantic")

ADD_SUBDIRECTORY(tools)

INSTALL(
    TARGETS
        cocaine-core
        cocaine-runtime
        json
    RUNTIME DESTINATION bin COMPONENT runtime
    LIBRARY DESTINATION lib COMPONENT runtime
    ARCHIVE DESTINATION lib COMPONENT developement)

INSTALL(
    DIRECTORY
        include/
        foreign/jsoncpp-0.6.0-rc2/include/
    DESTINATION include
    COMPONENT development)
