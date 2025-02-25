set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} -g3 -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall -Wextra")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -g3 -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -Wextra")

check_include_files(sys/limits.h, HAVE_SYS_LIMITS_H)

set(LAUNCH_SRC
        channel.h channel.cc
        log.cc log.h
        util.h
        options.cc options.h
        signal.cc signal.h
        state_file.cc state_file.hpp)
if (USE_PRIVATE_DEPENDENCIES)
    set(LAUNCH_SRC ${LAUNCH_SRC}
        ../vendor/tinyxml2.cpp ../vendor/tinyxml2.h)
endif()
add_library(launch STATIC ${LAUNCH_SRC})

if (NOT USE_PRIVATE_DEPENDENCIES)
    # we register libtinyxml2 as a public dependency of liblaunch
    # instead of including the tinyxml2 sources in it; also
    # add the tinyxml2 include directory to all targets in the current dir.
    target_link_libraries(launch PUBLIC tinyxml2::tinyxml2)
    include_directories(tinyxml2::tinyxml2)
endif()
if (CMAKE_SYSTEM_NAME MATCHES Linux)
    # this makes adding libthread explicitly (below) redundant
    # but adding just that library is not necessarily sufficient
    # (go figure why...)
    target_link_options(launch PUBLIC "-pthread")
endif()

add_executable(launchd
        calendar.cc calendar.h
        channel.h
        keepalive.cc keepalive.h
        job.cc job.h
        launchd.cc
        log.h
        manager.cc manager.h
        manifest.cc manifest.h
        options.h
        rpc_server.cc rpc_server.h
        timer.cc timer.h
        util.h)
target_link_libraries(launchd launch pthread)

# DISABLED: this symbol exists in glibc but warns that it will always fail
# need to actually try compiling a program and using the function.
#check_symbol_exists(setlogin "unistd.h" HAVE_SETLOGIN)
#if(HAVE_SETLOGIN)
#    target_compile_definitions(launchd PRIVATE -DHAVE_SETLOGIN)
#endif()

check_symbol_exists(sys_signame "signal.h" HAVE_SYS_SIGNAME)

if(HAVE_SYS_EVENT_H)
else()
    target_link_libraries(launchd "${LibKQueue_LIBRARIES}")
    # add the libkqueue header and library directories to all targets in this source directory:
    include_directories("${LibKQueue_INCLUDE_DIR}")
    add_link_options("${LibKQueue_LIBRARY_LDFLAGS}")
endif()

# for asprintf() in glibc
add_compile_definitions(_GNU_SOURCE)

if ((CMAKE_INSTALL_PREFIX MATCHES "^/usr$") OR (CMAKE_INSTALL_PREFIX MATCHES "^/usr/local$"))
    set(VARDIR "/var")
    set(SYSCONFDIR "/etc")
else()
    set(VARDIR "${CMAKE_INSTALL_PREFIX}/var")
    set(SYSCONFDIR "${CMAKE_INSTALL_PREFIX}/etc")
endif()
set(PKGSTATEDIR "${VARDIR}/db/relaunchd")

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

install(TARGETS launchd DESTINATION ${CMAKE_INSTALL_PREFIX}/sbin)

install(CODE "FILE(MAKE_DIRECTORY \$ENV{DESTDIR}\/${PKGSTATEDIR})")
install(CODE "FILE(MAKE_DIRECTORY \$ENV{DESTDIR}\/${VARDIR}/run)")

#
# launchctl
#

add_executable(launchctl
        channel.h
        launchctl.cc
        log.h
        util.h
        options.h)

target_link_libraries(launchctl launch pthread)

if(HAVE_SYS_EVENT_H)
else()
    target_link_libraries(launchctl "${LibKQueue_LIBRARIES}")
endif()

install(TARGETS launchctl DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
