# Copyright © 2026 Khrustal & Mann
#              MELBOURNE, VICTORIA, AUSTRALIA, 3000
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing
# permissions and limitations under the License.
#
# Platform/ — the cross-platform shim layer (p2pplatform).
# See the Linux port plan §4. Header-only for now; .cpp implementations land per phase
# (p2piocp.cpp Phase 2, p2pfile/p2pthread Phase 1, p2psock Phase 3, p2pserial/crypto Phase 4).

set(P2PPLATFORM_HEADERS
    p2pexport.h
    p2ptypes.h
    p2pstr.h
    mfcshim.h
    p2pthread.h
    p2pfile.h
    p2piocp.h
    p2psock.h
    p2pserial.h
    p2pcrypto.h
    platform.h)

# Linux-only implementation TUs, added as they are authored:
set(P2PPLATFORM_SOURCES_LINUX
    # p2piocp.cpp      # Phase 2
    # p2pfile.cpp      # Phase 1
    # p2pthread.cpp    # Phase 1
    # p2psock.cpp      # Phase 3
    # p2pserial.cpp    # Phase 4
    # crypto/openssl.cpp  # Phase 4
)

if(WIN32 OR NOT P2PPLATFORM_SOURCES_LINUX)
    # Header-only: Windows pass-through, or Linux before the Phase-1 .cpp exist.
    # A STATIC lib with no sources fails at generate, so use INTERFACE here.
    add_library(p2pplatform INTERFACE)
    target_include_directories(p2pplatform INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
    target_sources(p2pplatform INTERFACE ${P2PPLATFORM_HEADERS})
    if(NOT WIN32)
        # 17, not 23. An INTERFACE compile feature is a FLOOR imposed on every
        # consumer, not a description of this shim -- and it is transitive, so
        # whatever is written here becomes the language level of everything that
        # links p2pplatform, overriding that target's own CXX_STANDARD when it is
        # lower. cxx_std_23 here silently raised Msgcore to C++23 in the CMake
        # build while Msgcore(2026).vcxproj compiled the same sources at stdcpp17
        # (its deliberate choice, 3f7bf5e). Setting CXX_STANDARD on the Msgcore
        # target could not fix that on its own; this line had to come down first.
        # These headers only need C++17, so that is what they should ask for.
        target_compile_features(p2pplatform INTERFACE cxx_std_17)
    endif()
else()
    add_library(p2pplatform STATIC ${P2PPLATFORM_SOURCES_LINUX})
    target_include_directories(p2pplatform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    # PRIVATE and INTERFACE rather than PUBLIC, which is both at once. The shim's
    # own translation units may use anything the top-level default allows; its
    # consumers are told only the floor its HEADERS require. PUBLIC cxx_std_23
    # conflated the two and exported the shim's freedom as everyone's obligation.
    target_compile_features(p2pplatform PRIVATE   cxx_std_23)
    target_compile_features(p2pplatform INTERFACE cxx_std_17)
    # Phase 2+: target_link_libraries(p2pplatform PUBLIC uring crypto ssl)
endif()

# Linux: generate and expose the win-compat compatibility headers (gen_wincompat.sh) so
# legacy `#include <afxwin.h>` / <windows.h> / "wtypes.h" etc. resolve to the platform
# shims with no per-TU edits. Generated at configure time; win-compat/ is gitignored.
if(NOT WIN32)
    execute_process(
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/gen_wincompat.sh"
        RESULT_VARIABLE _p2p_gw)
    if(NOT _p2p_gw EQUAL 0)
        message(FATAL_ERROR "gen_wincompat.sh failed: ${_p2p_gw}")
    endif()
    target_include_directories(p2pplatform INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/win-compat)
endif()

# Phase 2: standalone IOCP-over-io_uring shim test (the Linux port plan §5/§8). Opt-in and
# gated on liburing being available, so it never touches the Msgcore-only build path
# (p2piocp.cpp + liburing are pulled only by this target and, later, Targetcore).
if(NOT WIN32)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBURING QUIET liburing)
    endif()
    option(MSCS_BUILD_IOCP_TEST "Build the p2piocp io_uring shim test" ${LIBURING_FOUND})
    if(MSCS_BUILD_IOCP_TEST AND LIBURING_FOUND)
        add_executable(platform_iocp_test checks/iocp_test.cpp p2piocp.cpp)
        target_include_directories(platform_iocp_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR} ${LIBURING_INCLUDE_DIRS})
        target_link_libraries(platform_iocp_test PRIVATE ${LIBURING_LIBRARIES} pthread)
        target_compile_features(platform_iocp_test PRIVATE cxx_std_23)
        enable_testing()
        add_test(NAME platform_iocp_test COMMAND platform_iocp_test)
    endif()
endif()
