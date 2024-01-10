cmake_minimum_required(VERSION 3.14)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# Set project name, version and laguages here. (change as needed)
# Version numbers are available by including "exampleConfig.h" in
# the source. See exampleConfig.h.in for some more details.
project(shared-state VERSION 0.0.0.1 LANGUAGES CXX)

option( SS_STAT_FILE_LOCKING
        "Enable shared-state network statistics file locking"
        OFF )

option( SS_DEVELOPMENT_BUILD
        "Disable optimization to speed up build, enable verbose build log. \
         just for development purposes, not suitable for library usage"
         OFF )

option( SS_TESTS
        "Build tests" OFF )

set(SOURCES
    src/accept_operation.cc
    src/async_command.cc
    src/async_file_descriptor.cc
    src/async_socket.cc
    src/async_timer.cc
    src/close_operation.cc
    src/connect_operation.cc
    src/epoll_events_to_string.cc
    src/io_context.cc
    src/read_operation.cc
    src/recv_operation.cc
    src/send_operation.cc
    src/sharedstate.cc
    src/waitpid_operation.cc
    src/write_operation.cc
)

set(LIBRARY_NAME shared-state)
set(EXECUTABLE_NAME shared-state-async)


################################################################################
# Fetch libretroshare without adding it to the build and avoid consequent error
# for missing dependencies at OpenWrt build time.
# We just need a few source file not libretroshare full build.
# Use FetchContent_Populate plus a bit of custom logit
# @see https://cmake.org/cmake/help/latest/module/FetchContent.html#command:fetchcontent_populate
# @see https://www.reddit.com/r/cmake/comments/13frtwr/fetchcontent_without_adding_to_the_build/jjy3lmg/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button
# and not
# @see https://cmake.org/cmake/help/latest/module/FetchContent.html#populating-content-without-adding-it-to-the-build
# which add libretroshare to the build anyway -_-
set(LIBRETROSHARE_DEVEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libretroshare/")
if(EXISTS "${LIBRETROSHARE_DEVEL_DIR}/CMakeLists.txt" )
    message( STATUS
             "libretroshare source found at ${LIBRETROSHARE_DEVEL_DIR} using it" )
    set(LIBRETROSHARE_DIR "${LIBRETROSHARE_DEVEL_DIR}")
else()
    FetchContent_Declare(
        libretroshare
        GIT_REPOSITORY "https://github.com/RetroShare/libretroshare.git"
        GIT_TAG "origin/master"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        TIMEOUT 10
        )
    FetchContent_GetProperties(libretroshare)
    if(NOT libretroshare_POPULATED)
        FetchContent_Populate(libretroshare)
        set(LIBRETROSHARE_DIR "${libretroshare_SOURCE_DIR}")
    endif()

    FetchContent_Declare(
        rapidjson
        GIT_REPOSITORY "https://github.com/Tencent/rapidjson.git"
        GIT_TAG "origin/master"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        TIMEOUT 10
    )
    FetchContent_GetProperties(rapidjson)
    if(NOT rapidjson_POPULATED)
        FetchContent_Populate(rapidjson)
        set(RAPIDJSON_DIR "${rapidjson_SOURCE_DIR}")
    endif()

    include_directories(${RAPIDJSON_DIR}/include)
endif()

include_directories(${LIBRETROSHARE_DIR}/src/)

add_compile_definitions(RS_DISABLE_DEPRECATED_DEBUG_UTILS)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

set(SOURCES
    ${SOURCES}
    ${LIBRETROSHARE_DIR}/src/serialiser/rsbaseserial.cc
    ${LIBRETROSHARE_DIR}/src/serialiser/rstlvbase.cc
    ${LIBRETROSHARE_DIR}/src/serialiser/rstypeserializer.cc
    ${LIBRETROSHARE_DIR}/src/util/rsbase64.cc
    ${LIBRETROSHARE_DIR}/src/util/rsdebug.cc
    ${LIBRETROSHARE_DIR}/src/util/rsnet.cc
    ${LIBRETROSHARE_DIR}/src/util/rsnet_ss.cc
    ${LIBRETROSHARE_DIR}/src/util/rsstring.cc
    ${LIBRETROSHARE_DIR}/src/util/rsurl.cc
)

################################################################################

add_library(${LIBRARY_NAME} OBJECT ${SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
# TODO: check if coroutines support has been added to target_compile_features()
target_compile_options(${LIBRARY_NAME} PUBLIC "-fcoroutines")

target_link_libraries(${LIBRARY_NAME} PRIVATE nlohmann_json::nlohmann_json)

add_executable(${EXECUTABLE_NAME} app/${EXECUTABLE_NAME}.cc)
target_link_libraries(${EXECUTABLE_NAME} PRIVATE ${LIBRARY_NAME})
# TODO: check if coroutines support has been added to target_compile_features()
target_compile_options(${EXECUTABLE_NAME} PRIVATE "-fcoroutines")

set_target_properties(
    ${LIBRARY_NAME} ${EXECUTABLE_NAME}
      PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        #CXX_EXTENSIONS YES
)

install(TARGETS ${LIBRARY_NAME} ${EXECUTABLE_NAME} )

# Optional IPO/LTO. Do not enable them if it's not supported by compiler.
# @see https://cmake.org/cmake/help/latest/module/CheckIPOSupported.html#module:CheckIPOSupported
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_LTO_AVAILABLE)
if(IPO_LTO_AVAILABLE)
    set_target_properties(
        ${LIBRARY_NAME} ${EXECUTABLE_NAME}
          PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION YES
    )
endif(IPO_LTO_AVAILABLE)

if(SS_DEVELOPMENT_BUILD)
        set(CMAKE_VERBOSE_MAKEFILE ON)
        set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif(SS_DEVELOPMENT_BUILD)

if(SS_TESTS)
    add_subdirectory(tests)
endif(SS_TESTS)

if(SS_STAT_FILE_LOCKING)
    target_compile_definitions(
        ${LIBRARY_NAME}
        PRIVATE SHARED_STATE_STAT_FILE_LOCKING )
endif(SS_STAT_FILE_LOCKING)
