# This file specifies how the project should be built, using CMake.
# If you are unfamiliar with CMake, don't worry about all the details.
# The sections you might want to edit are marked as such, and
# the comments should hopefully make most of it clear.
#
# For many purposes, you may not need to change anything about this file.

cmake_minimum_required(VERSION 3.14)

# Set project name, version and laguages here. (change as needed)
# Version numbers are available by including "exampleConfig.h" in
# the source. See exampleConfig.h.in for some more details.
project(SHAREDSTATE VERSION 0.0.0.1 LANGUAGES CXX)

set(FETCHCONTENT_QUIET OFF)
include(FetchContent)

# Options: Things you can set via commandline options to cmake (e.g. -DENABLE_LTO=[ON|OFF])
option(ENABLE_WARNINGS_SETTINGS "Allow target_set_warnings to add flags and defines.
                                 Set this to OFF if you want to provide your own warning parameters." OFF)
option(ENABLE_LTO "Enable link time optimization" ON)
option(ENABLE_DOCTESTS "Include tests in the library. Setting this to OFF will remove all doctest related code.
                        Tests in tests/*.cpp will still be enabled." ON)

# Include stuff. No change needed.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
#include(ConfigSafeGuards)
include(Colors)
include(CTest)
include(Doctest)
include(Documentation)
include(LTO)
include(Misc)
include(Warnings)

# Check for LTO support.
find_lto(CXX)

# --------------------------------------------------------------------------------
#                         Locate files (change as needed).
# --------------------------------------------------------------------------------
set(SOURCES
    src/io_context.cc
    src/sharedstate.cc
    src/connect_operation.cc
    src/socket_accept_operation.cc
    src/socket.cc
    src/socket_recv_operation.cc
    src/socket_send_operation.cc
    src/shared_state_error_code.cc
    src/file_read_operation.cc
    src/popen_async_command.cc
    src/piped_async_command.cc
    src/popen_file_read_operation.cc
    src/file_write_operation.cc
    src/dying_process_wait_operation.cc
)

set(TESTFILES
    tests/main.cpp
)

set(LIBRARY_NAME shared-state)
set(SERVER_NAME shared-state-async-server)
set(CLIENT_NAME shared-state-async-client)


################################################################################

add_compile_definitions(RS_DISABLE_DEPRECATED_DEBUG_UTILS)

set(LIBRETROSHARE_DEVEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libretroshare/")
if(EXISTS "${LIBRETROSHARE_DEVEL_DIR}/CMakeLists.txt" )
        message(
                STATUS
                "libretroshare source found at ${LIBRETROSHARE_DEVEL_DIR} using it" )
#        add_subdirectory("${LIBRETROSHARE_DEVEL_DIR}" "${CMAKE_BINARY_DIR}/libretroshare")
    set(LIBRETROSHARE_DIR "${LIBRETROSHARE_DEVEL_DIR}")
else()
        FetchContent_Declare(
                libretroshare
                GIT_REPOSITORY "https://gitlab.com/RetroShare/libretroshare.git"
                GIT_TAG "origin/master"
                GIT_SHALLOW TRUE
                GIT_PROGRESS TRUE
                TIMEOUT 10
        )
        FetchContent_MakeAvailable(libretroshare)
        set(LIBRETROSHARE_DIR "${libretroshare_SOURCE_DIR}")
endif()

#target_link_libraries(${PROJECT_NAME} PRIVATE libretroshare)

include_directories(${LIBRETROSHARE_DIR}/src/)

set(SOURCES
    ${SOURCES}
    ${LIBRETROSHARE_DIR}/src/util/rsdebug.cc
)

################################################################################

# --------------------------------------------------------------------------------
#                            Build! (Change as needed)
# --------------------------------------------------------------------------------
# Compile all sources into a library.
add_library(${LIBRARY_NAME} OBJECT ${SOURCES})

# Lib needs its header files, and users of the library must also see these (PUBLIC). (No change needed)
target_include_directories(${LIBRARY_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)

# There's also (probably) doctests within the library, so we need to see this as well.
target_link_libraries(${LIBRARY_NAME} PUBLIC doctest)

# TODO: check if coroutines support has been added to target_compile_features()
target_compile_options(${LIBRARY_NAME} PUBLIC "-fcoroutines")

# Set the compile options you want (change as needed).
target_set_warnings(${LIBRARY_NAME} ENABLE ALL AS_ERROR ALL DISABLE Annoying)
# target_compile_options(${LIBRARY_NAME} ... )  # For setting manually.

add_executable(${SERVER_NAME} app/${SERVER_NAME}.cc)
target_link_libraries(${SERVER_NAME} PRIVATE ${LIBRARY_NAME})
target_set_warnings(${SERVER_NAME} ENABLE ALL AS_ERROR ALL DISABLE Annoying)
target_enable_lto(${SERVER_NAME} optimized)
# TODO: check if coroutines support has been added to target_compile_features()
target_compile_options(${SERVER_NAME} PRIVATE "-fcoroutines")

add_executable(${CLIENT_NAME} app/${CLIENT_NAME}.cc)
target_link_libraries(${CLIENT_NAME} PRIVATE ${LIBRARY_NAME})
target_set_warnings(${CLIENT_NAME} ENABLE ALL AS_ERROR ALL DISABLE Annoying)
target_enable_lto(${CLIENT_NAME} optimized)
# TODO: check if coroutines support has been added to target_compile_features()
target_compile_options(${CLIENT_NAME} PRIVATE "-fcoroutines")

# Set the properties you require, e.g. what C++ standard to use.
# Here applied to library and main (change as needed).
set_target_properties(
    ${LIBRARY_NAME} ${SERVER_NAME} ${CLIENT_NAME}
      PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        #CXX_EXTENSIONS YES
)

# Set up tests (see tests/CMakeLists.txt).
add_subdirectory(tests)
