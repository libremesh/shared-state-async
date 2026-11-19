cmake_minimum_required(VERSION 3.14)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# Set project name, version and laguages here. (change as needed)
# Version numbers are available by including "exampleConfig.h" in
# the source. See exampleConfig.h.in for some more details.
project(shared-state VERSION 0.0.0.1 LANGUAGES CXX)


if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option( SS_STAT_FILE_LOCKING
        "Enable shared-state network statistics file locking"
        OFF )

option( SS_DEVELOPMENT_BUILD
        "Disable optimization to speed up build, enable verbose build log. \
         just for development purposes, not suitable for library usage"
         OFF )

option( SS_TESTS
        "Build tests" OFF )

option( SS_CPPTRACE_STACKTRACE
        "Use Jeremy Rifkin Cpptrace library to print stacktrace instead of \
         our implementation"
        ON )

option(SS_STATIC_LIBSTDCPP "Statically link libstdc++" ON)
option(SS_STRIP_SYMBOLS "Strip unused symbols from binary" ON)

set(OPTIMIZATION_FLAGS
    "-Os"                         # Optimization for size
    "-ffunction-sections"         # Place each function in its own section
    "-fdata-sections"            # Place each data item in its own section
    "-fvisibility=hidden"        # Hide symbols by default
    "-fvisibility-inlines-hidden" # Hide inline function symbols
)
set(LINKER_FLAGS
    "-Wl,--gc-sections"
    "-Wl,--as-needed"
)
if(SS_STATIC_LIBSTDCPP)
    list(APPEND LINKER_FLAGS "-static-libstdc++")
endif()

set(LIBRARY_SOURCES
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
    src/shared_state_errors.cc
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

set(LIBRARY_SOURCES
    ${LIBRARY_SOURCES}
    ${LIBRETROSHARE_DIR}/src/serialiser/rsbaseserial.cc
    ${LIBRETROSHARE_DIR}/src/serialiser/rsserializer.cc
    ${LIBRETROSHARE_DIR}/src/serialiser/rsserializable.cc
    ${LIBRETROSHARE_DIR}/src/serialiser/rsserial.cc
    ${LIBRETROSHARE_DIR}/src/serialiser/rstlvbase.cc
    ${LIBRETROSHARE_DIR}/src/serialiser/rstypeserializer.cc
    ${LIBRETROSHARE_DIR}/src/util/rsbase64.cc
    ${LIBRETROSHARE_DIR}/src/util/rsdebug.cc
    ${LIBRETROSHARE_DIR}/src/util/rsjson.cc
    ${LIBRETROSHARE_DIR}/src/util/rsnet.cc
    ${LIBRETROSHARE_DIR}/src/util/rsnet_ss.cc
    ${LIBRETROSHARE_DIR}/src/util/rsstring.cc
    ${LIBRETROSHARE_DIR}/src/util/rsstacktrace.cc
    ${LIBRETROSHARE_DIR}/src/util/rsthreads.cc
    ${LIBRETROSHARE_DIR}/src/util/rsurl.cc
    ${LIBRETROSHARE_DIR}/src/util/smallobject.cc
)

set(CLI_SOURCES
    app/${EXECUTABLE_NAME}.cc
    app/shared_state_cli.cc
)

################################################################################

add_library(${LIBRARY_NAME} OBJECT ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
# TODO: check if coroutines support has been added to target_compile_features()
target_compile_options(${LIBRARY_NAME} PUBLIC "-fcoroutines" ${OPTIMIZATION_FLAGS})
target_link_options(${LIBRARY_NAME} PUBLIC ${LINKER_FLAGS})

add_executable(${EXECUTABLE_NAME} ${CLI_SOURCES})
target_link_libraries(${EXECUTABLE_NAME} PRIVATE ${LIBRARY_NAME})
# TODO: check if coroutines support has been added to target_compile_features()
target_compile_options(${EXECUTABLE_NAME} PRIVATE "-fcoroutines" ${OPTIMIZATION_FLAGS})
target_link_options(${EXECUTABLE_NAME} PRIVATE ${LINKER_FLAGS})

set_target_properties(
    ${LIBRARY_NAME} ${EXECUTABLE_NAME}
      PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        #CXX_EXTENSIONS YES
)

if(SS_STRIP_SYMBOLS)
    # Auto-detect strip tool from the cross-compiler path if not already set to an existing tool
    # OpenWrt 24 uses "-DCMAKE_STRIP=:"
    if(NOT EXISTS "${CMAKE_STRIP}")
        get_filename_component(TOOLCHAIN_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(TOOLCHAIN_PREFIX "${CMAKE_CXX_COMPILER}" NAME)
        # Derive strip path from C++ compiler: e.g. mips-...-g++ -> mips-...-strip
        string(REGEX REPLACE "(g\\+\\+|gcc)$" "strip" STRIP_NAME "${TOOLCHAIN_PREFIX}")
        set(TOOLCHAIN_STRIP "${TOOLCHAIN_BIN_DIR}/${STRIP_NAME}")

        if(EXISTS "${TOOLCHAIN_STRIP}")
            set(CMAKE_STRIP "${TOOLCHAIN_STRIP}" CACHE FILEPATH "Path to toolchain strip" FORCE)
            message(STATUS "Auto-detected toolchain strip: ${CMAKE_STRIP}")
        else()
            message(WARNING "No toolchain strip found at ${TOOLCHAIN_STRIP}, will skip stripping")
        endif()
    endif()

    # Strip the binary if we have a strip tool (either provided or auto-detected)
    if(EXISTS "${CMAKE_STRIP}")
        add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
            COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:${EXECUTABLE_NAME}>
            COMMENT "Stripping unused symbols from ${EXECUTABLE_NAME}")
    endif()
endif()

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

if(SS_CPPTRACE_STACKTRACE)
    # Apparently PreventInSourceBuilds check shipped within Cpptrace give false
    # positive with OpenWrt build system
    FetchContent_Declare(
      cpptrace
      GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
      GIT_TAG v0.3.1
      GIT_SHALLOW TRUE
      GIT_PROGRESS TRUE
      TIMEOUT 10
      PATCH_COMMAND echo > cmake/PreventInSourceBuilds.cmake
    )
    FetchContent_MakeAvailable(cpptrace)
    target_link_libraries(${LIBRARY_NAME} PRIVATE cpptrace::cpptrace)
    target_compile_definitions(
        ${LIBRARY_NAME} PRIVATE RS_JEREMY_RIFKIN_CPPTRACE )
endif(SS_CPPTRACE_STACKTRACE)
