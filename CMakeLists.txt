# Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
# SPDX-License-Identifier: GPL-2.0-only
#
# swarm-core: the contrib module every swarm-routing protocol links (Phase 2,
# epic #6): the ns-3 adapter of swarm-core's ports. model/ and helper/ hold the
# ns-3 glue (#9: SwarmRoutingBase, DeferredPacketQueue, the link monitors,
# helpers). The simulator-free primitives come from swarm-routing/swarm-core's
# release (#231): its core/, the target swarm::core, linked PUBLIC, so a module
# that links ${libswarm-core} gets its objects, its include directory (for
# `#include "swarm-ports.h"` and "testbench/swarm-testbench.h") and C++23.
# Its headers are also exported as ns3/<name>.h, so ns-3 generates the
# umbrella header ns3/swarm-core-module.h with both: a protocol gets the
# primitives and the glue with `#include "ns3/swarm-core-module.h"`.
#
# Where swarm-core comes from, first match:
#   -DSWARM_CORE_DIR=<swarm-core checkout>/core   a local swarm-core (an
#                                                 unreleased change; nothing
#                                                 committed names it)
#   -DSWARM_CORE_FROM_PACKAGE=ON                  the installed CMake package
#                                                 swarm-core (find_package; the
#                                                 vcpkg port swarm-adapter-ns3
#                                                 builds this module on the
#                                                 swarm-core port, outside an
#                                                 ns-3 tree: swarm-routing/vcpkg-registry#76)
#   otherwise                                     the release swarm-core.lock
#                                                 names, its tarball fetched by
#                                                 URL and checked by SHA256
set(SWARM_CORE_DIR "" CACHE PATH "swarm-core's core/ directory; empty: the release swarm-core.lock names")
option(SWARM_CORE_FROM_PACKAGE "link the installed swarm-core CMake package instead of building swarm-core.lock's release" OFF)
if(SWARM_CORE_FROM_PACKAGE)
  # The package's own target and headers: nothing of swarm-core is built or
  # installed here, so this module's package depends on swarm-core's.
  find_package(swarm-core CONFIG REQUIRED)
  get_target_property(swarm_core_include swarm::core INTERFACE_INCLUDE_DIRECTORIES)
  list(GET swarm_core_include 0 swarm_core_include)
  file(GLOB SWARM_CORE_HEADERS RELATIVE "${swarm_core_include}" "${swarm_core_include}/*.h")
  list(SORT SWARM_CORE_HEADERS)
  set(swarm_core_src "${swarm_core_include}")
  message(STATUS "swarm-core: the installed package ${swarm-core_VERSION} (${swarm-core_DIR})")
elseif(SWARM_CORE_DIR)
  get_filename_component(swarm_core_src "${SWARM_CORE_DIR}" ABSOLUTE)
  if(NOT EXISTS "${swarm_core_src}/swarm-core-sources.cmake")
    message(FATAL_ERROR "swarm-core: SWARM_CORE_DIR=${SWARM_CORE_DIR} is not swarm-core's core/ directory")
  endif()
  add_subdirectory("${swarm_core_src}" "${CMAKE_CURRENT_BINARY_DIR}/swarm-core-local")
else()
  # The lock: KEY=VALUE lines, `#` comments (swarm-routing/.github
  # tools/checks/release_lock.py writes it from the release).
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/swarm-core.lock")
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/swarm-core.lock" swarm_core_lock REGEX "^SWARM_CORE_[A-Z0-9_]+=")
  foreach(line IN LISTS swarm_core_lock)
    string(REGEX REPLACE "^(SWARM_CORE_[A-Z0-9_]+)=(.*)$" "\\1" key "${line}")
    string(REGEX REPLACE "^(SWARM_CORE_[A-Z0-9_]+)=(.*)$" "\\2" value "${line}")
    set(swarm_core_lock_${key} "${value}")
  endforeach()
  foreach(key REPO TAG ASSET SHA256)
    if(NOT swarm_core_lock_SWARM_CORE_${key})
      message(FATAL_ERROR "swarm-core: swarm-core.lock has no SWARM_CORE_${key}")
    endif()
  endforeach()
  include(FetchContent)
  FetchContent_Declare(swarm_core_release
    URL "https://github.com/${swarm_core_lock_SWARM_CORE_REPO}/releases/download/${swarm_core_lock_SWARM_CORE_TAG}/${swarm_core_lock_SWARM_CORE_ASSET}"
    URL_HASH SHA256=${swarm_core_lock_SWARM_CORE_SHA256}
    SOURCE_SUBDIR core
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
  FetchContent_MakeAvailable(swarm_core_release)
  set(swarm_core_src "${swarm_core_release_SOURCE_DIR}/core")
  message(STATUS "swarm-core: ${swarm_core_lock_SWARM_CORE_REPO} ${swarm_core_lock_SWARM_CORE_TAG} (swarm-core.lock)")
endif()

# swarm_core is a static library inside this shared one: ns-3 installs every
# module through the export set ns3ExportTargets, which must then hold it too,
# under a name that is not ns-3's own core module (ns3::core). From the
# package, it is the package's (imported) and stays the package's to install.
if(NOT SWARM_CORE_FROM_PACKAGE)
  set_target_properties(swarm_core PROPERTIES EXPORT_NAME swarm-core-primitives)
  install(TARGETS swarm_core EXPORT ns3ExportTargets
          ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/)
endif()

# swarm-core's headers as ns3/<name>.h (and in the umbrella header). ns-3
# writes the ns3/<name>.h stub of a HEADER_FILES entry from a path relative to
# this directory only, and skips a stub that exists: so each is written here
# first, naming the header by its absolute path (rewritten only when the path
# changes, so a reconfigure rebuilds nothing). The testbench stays out, as it
# always has (the package's top-level headers are the same list).
if(NOT SWARM_CORE_FROM_PACKAGE)
  include("${swarm_core_src}/swarm-core-sources.cmake")
endif()
list(TRANSFORM SWARM_CORE_HEADERS PREPEND "${swarm_core_src}/")
foreach(header IN LISTS SWARM_CORE_HEADERS)
  get_filename_component(header_name "${header}" NAME)
  file(CONFIGURE OUTPUT "${CMAKE_HEADER_OUTPUT_DIRECTORY}/${header_name}"
       CONTENT "#include \"${header}\"\n" @ONLY)
endforeach()

build_lib(
  LIBNAME swarm-core
  SOURCE_FILES
    helper/swarm-lifecycle-helper.cc
    helper/swarm-routing-helper.cc
    model/swarm-core.cc
    model/swarm-deferred-packet-queue.cc
    model/swarm-energy-metric-hook.cc
    model/swarm-isl-link-monitor.cc
    model/swarm-mobility-location-provider.cc
    model/swarm-ns3-ports.cc
    model/swarm-routing-base.cc
    model/swarm-wifi-link-monitor.cc
  HEADER_FILES
    ${SWARM_CORE_HEADERS}
    helper/swarm-lifecycle-helper.h
    helper/swarm-routing-helper.h
    model/swarm-core.h
    model/swarm-deferred-packet-queue.h
    model/swarm-energy-metric-hook.h
    model/swarm-isl-link-monitor.h
    model/swarm-mobility-location-provider.h
    model/swarm-ns3-ports.h
    model/swarm-routing-base.h
    model/swarm-wifi-link-monitor.h
    model/swarm-wire-buffer.h
  LIBRARIES_TO_LINK ${libcore} ${libnetwork} ${libinternet} ${libwifi} ${libpoint-to-point}
                    ${libenergy} ${libmobility}
  TEST_SOURCES
    test/swarm-core-test-suite.cc
    test/swarm-glue-tests.cc
)
# swarm-core's core/ as a PUBLIC dependency (#231): its objects, its include
# directory and its cxx_std_23 reach every module that links this one.
target_link_libraries(${libswarm-core} PUBLIC swarm::core)
