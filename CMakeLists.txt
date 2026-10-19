# Top level build description.
#
# Two ways to build:
#
#   1. against a Qt 6 development installation (the normal case on a workstation
#      or on Ubuntu 24.04 with "apt install qt6-base-dev qt6-tools-dev"):
#
#          cmake -B build -S . -DUSE_SYSTEM_QT6=ON
#
#   2. against the runtime provisioned by scripts/toolchain/setup-toolchain.sh,
#      which is what this sandbox uses because it has no package manager:
#
#          cmake -B build -S .
#
# The application sources are identical in both cases: the imported targets
# created by cmake/Qt6Runtime.cmake have the same names as the ones
# find_package(Qt6) provides.
cmake_minimum_required(VERSION 3.20)

project(pcb-workbench
    VERSION 0.1.0
    DESCRIPTION "PCB inspection, documentation and diagnostics workbench"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# Every target is compiled with -fPIC, executables included, even though CMake
# would use -fPIE for them.  Qt's meta object system compares QMetaObject
# *pointers* (qobject_cast, findChild<T>, QMetaObject::cast), and with -fPIE the
# linker resolves such a reference with a copy relocation: the executable ends up
# with its own copy of QFoo::staticMetaObject, which never compares equal to the
# one inside the Qt library, so qobject_cast silently returns nullptr.  -fPIC
# reaches the symbol through the GOT and keeps a single copy.  The cost is a
# negligible amount of addressing overhead.
add_compile_options(-fPIC)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(USE_SYSTEM_QT6 "Build against the Qt 6 development packages instead of the provisioned runtime" OFF)
option(BUILD_TESTING "Build the automated tests" ON)

# Helpers of this project (eda_add_test(), ...).
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Qt
if(USE_SYSTEM_QT6)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql Concurrent Test PrintSupport)
    set(QT_VERSION "${Qt6_VERSION}")
else()
    include(cmake/Qt6Runtime.cmake)
    set(QT_VERSION "${QT6_RUNTIME_VERSION}")
    if(NOT BUILD_TESTING STREQUAL "OFF")
        # nothing: tests are linked against the same runtime
    endif()
    include(cmake/Qt6Bundle.cmake)
endif()

# Paths that end up in the compiled binary (help menu, About dialog).
if(CMAKE_INSTALL_PREFIX STREQUAL "/usr" OR CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
    set(EDA_DOCUMENT_DIR "${CMAKE_INSTALL_PREFIX}/share/doc/${PROJECT_NAME}")
else()
    set(EDA_DOCUMENT_DIR "${CMAKE_INSTALL_PREFIX}/share/doc/${PROJECT_NAME}")
endif()
set(EDA_MANUAL_PATH "${EDA_DOCUMENT_DIR}/UserManual.md")
set(EDA_TARGET_SYSTEM "${CMAKE_SYSTEM_NAME}")

configure_file(src/core/Version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/eda/Version.h" @ONLY)

add_subdirectory(src)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# installation
# ---------------------------------------------------------------------------
# The executable itself is installed by src/CMakeLists.txt, because its
# destination depends on whether the Qt runtime is bundled or not.
install(FILES assets/pcb-workbench.desktop DESTINATION share/applications)
install(FILES assets/pcb-workbench.png DESTINATION share/icons/hicolor/256x256/apps)
install(FILES assets/pcb-workbench.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES docs/UserManual.md docs/supported_formats.md docs/architecture.md
              docs/troubleshooting.md docs/safety.md docs/data_sources.md
        DESTINATION share/doc/${PROJECT_NAME})
install(FILES README.md DESTINATION share/doc/${PROJECT_NAME})

# ---------------------------------------------------------------------------
# packaging
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "pcb-workbench")
set(CPACK_PACKAGE_VENDOR "PCB Workbench contributors")
set(CPACK_PACKAGE_CONTACT "PCB Workbench contributors <noreply@example.invalid>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PCB inspection, documentation and diagnostics workbench")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_STRIP_FILES TRUE)
set(CPACK_PACKAGE_EXECUTABLES "pcb-workbench;PCB Workbench")
# Packages are written to dist/ at the top of the source tree, not into the
# build directory, so that `cmake --build build --target package` leaves a
# result that is easy to find and that a clean build does not throw away.
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

# Debian package. The Qt runtime libraries are provided by the distribution
# (Ubuntu 24.04 ships Qt 6.4/6.6 in its archive); the dependencies therefore
# name the distribution packages instead of bundling copies of them, which is
# what Debian and Ubuntu packaging policy expects for system libraries.
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "electronics")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/Draxx143/money")
set(CPACK_DEB_COMPONENT_INSTALL OFF)
if(EDA_BUNDLE_PRIVATE_QT)
    # The bundled libraries are not Debian packages, so dpkg-shlibdeps cannot
    # resolve them; the dependencies are listed explicitly instead.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
    # Written as a list and joined here: a hand written multi line string picks
    # up stray commas that dpkg then rejects.
    set(_eda_deb_deps
        "libc6 (>= 2.36)"
        "libstdc++6"
        "libgcc-s1"
        "zlib1g"
        "libgl1"
        "libegl1"
        "libxkbcommon-x11-0"
        "libxkbcommon0"
        "libdbus-1-3"
        "libfontconfig1"
        "libfreetype6"
        "libx11-6"
        "libxext6"
        "libxcb1"
        "libxcb-cursor0"
        "libxcb-glx0"
        "libxcb-icccm4"
        "libxcb-image0"
        "libxcb-keysyms1"
        "libxcb-randr0"
        "libxcb-render-util0"
        "libxcb-render0"
        "libxcb-shape0"
        "libxcb-shm0"
        "libxcb-sync1"
        "libxcb-xfixes0"
        "libxcb-xinerama0"
        "libxcb-xkb1"
        "libxcb-util1"
        "libxi6"
        "libsm6"
        "libice6"
        "libsqlite3-0"
        "xdg-utils")
    string(REPLACE ";" ", " CPACK_DEBIAN_PACKAGE_DEPENDS "${_eda_deb_deps}")
else()
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

if(USE_SYSTEM_QT6)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libqt6widgets6, libqt6gui6, libqt6core6, libqt6network6, libqt6sql6, "
        "libqt6sql6-sqlite, qt6-qpa-plugins")
endif()

# Debian package on Linux (a .deb is the release artefact of this project);
# a plain tarball next to it for everything else.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(DPKG_DEB_EXECUTABLE dpkg-deb)
    if(DPKG_DEB_EXECUTABLE)
        set(CPACK_GENERATOR "DEB;TGZ")
    else()
        set(CPACK_GENERATOR "TGZ")
    endif()
endif()

include(CPack)
