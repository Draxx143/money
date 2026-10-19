# Qt6Runtime.cmake
#
# Declares the Qt 6 modules as imported CMake targets when the application is
# built against the runtime provisioned by scripts/toolchain/setup-toolchain.sh
# (USE_SYSTEM_QT6=OFF, the default inside this sandbox).
#
# The targets defined here mirror the ones `find_package(Qt6 COMPONENTS ...)`
# would create, so the rest of the build system and every source file looks
# exactly like an ordinary Qt 6 project:
#
#     target_link_libraries(app PRIVATE Qt6::Widgets Qt6::Network)
#
# When a real Qt 6 development installation is available (Ubuntu 24.04:
# apt install qt6-base-dev qt6-base-dev-tools qt6-tools-dev) the build uses
# find_package() instead - see the USE_SYSTEM_QT6 option in the top level
# CMakeLists.txt.  Nothing in src/ knows which of the two is in use.

set(_qt6_runtime_modules
    Core Gui Widgets Network Sql Concurrent Xml Test PrintSupport)

if(NOT DEFINED QT6_RUNTIME_ROOT AND DEFINED ENV{QT_ROOT})
    set(QT6_RUNTIME_ROOT "$ENV{QT_ROOT}")
endif()
if(NOT DEFINED QT6_INCLUDE_ROOT AND DEFINED ENV{QT_INCLUDE_ROOT})
    set(QT6_INCLUDE_ROOT "$ENV{QT_INCLUDE_ROOT}")
endif()
if(NOT DEFINED QT6_TOOLS_ROOT AND DEFINED ENV{QT_TOOLS_ROOT})
    set(QT6_TOOLS_ROOT "$ENV{QT_TOOLS_ROOT}")
endif()

if(NOT QT6_RUNTIME_ROOT)
    set(QT6_RUNTIME_ROOT "${CMAKE_SOURCE_DIR}/external/qt-runtime/Qt")
endif()
if(NOT QT6_INCLUDE_ROOT)
    set(QT6_INCLUDE_ROOT "${CMAKE_SOURCE_DIR}/external/qt-headers")
endif()
if(NOT QT6_TOOLS_ROOT)
    set(QT6_TOOLS_ROOT "${CMAKE_SOURCE_DIR}/external/qt-tools")
endif()

if(NOT EXISTS "${QT6_RUNTIME_ROOT}/lib/libQt6Core.so.6")
    message(FATAL_ERROR
        "No Qt 6 runtime in ${QT6_RUNTIME_ROOT}. Run scripts/toolchain/setup-toolchain.sh "
        "or configure with -DUSE_SYSTEM_QT6=ON to use the Qt 6 development package.")
endif()
if(NOT EXISTS "${QT6_INCLUDE_ROOT}/QtCore/qobject.h")
    message(FATAL_ERROR
        "No Qt 6 headers in ${QT6_INCLUDE_ROOT}. Run scripts/toolchain/setup-toolchain.sh.")
endif()

# Qt version, taken from <QtCore/qconfig.h>
file(STRINGS "${QT6_INCLUDE_ROOT}/QtCore/qconfig.h" _qt6_version_line
     REGEX "^#define QT_VERSION_STR ")
string(REGEX REPLACE ".*\"([0-9.]+)\".*" "\\1" QT6_RUNTIME_VERSION "${_qt6_version_line}")
message(STATUS "Qt 6 runtime: ${QT6_RUNTIME_VERSION} in ${QT6_RUNTIME_ROOT}")
message(STATUS "Qt 6 headers: ${QT6_INCLUDE_ROOT}")

# ---------------------------------------------------------------------------
# module -> dependencies
# ---------------------------------------------------------------------------
set(_qt6_Core_DEPS "")
set(_qt6_Gui_DEPS Qt6::Core)
set(_qt6_Widgets_DEPS Qt6::Gui Qt6::Core)
set(_qt6_Network_DEPS Qt6::Core)
set(_qt6_Sql_DEPS Qt6::Core)
set(_qt6_Concurrent_DEPS Qt6::Core)
set(_qt6_Xml_DEPS Qt6::Core)
set(_qt6_Test_DEPS Qt6::Core)
set(_qt6_PrintSupport_DEPS Qt6::Widgets Qt6::Gui Qt6::Core)

foreach(module ${_qt6_runtime_modules})
    if(TARGET Qt6::${module})
        continue()
    endif()
    string(TOUPPER ${module} _module_upper)
    add_library(Qt6::${module} SHARED IMPORTED GLOBAL)
    set_target_properties(Qt6::${module} PROPERTIES
        IMPORTED_LOCATION "${QT6_RUNTIME_ROOT}/lib/libQt6${module}.so.6"
        IMPORTED_SONAME "libQt6${module}.so.6"
        INTERFACE_INCLUDE_DIRECTORIES "${QT6_INCLUDE_ROOT};${QT6_INCLUDE_ROOT}/Qt${module}"
        INTERFACE_COMPILE_DEFINITIONS
            "QT_SHARED;QT_${_module_upper}_LIB;$<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG>"
        INTERFACE_LINK_LIBRARIES "${_qt6_${module}_DEPS}"
        INTERFACE_COMPILE_FEATURES cxx_std_20
        # Read by CMake's AUTOMOC/AUTOUIC/AUTORCC support to pick the Qt major
        # version; without it the generators silently switch themselves off.
        INTERFACE_QT_MAJOR_VERSION 6
        INTERFACE_QT_VERSION "${QT6_RUNTIME_VERSION}"
    )
endforeach()

# ---------------------------------------------------------------------------
# moc / uic / rcc -- CMake's AUTOMOC, AUTOUIC and AUTORCC pick these up by name
# ---------------------------------------------------------------------------
foreach(tool moc uic rcc)
    if(TARGET Qt6::${tool})
        continue()
    endif()
    if(tool STREQUAL "moc")
        set(_tool_path "${QT6_TOOLS_ROOT}/moc")
    else()
        set(_tool_path "${QT6_RUNTIME_ROOT}/libexec/${tool}")
    endif()
    if(NOT EXISTS "${_tool_path}")
        message(WARNING "Qt 6 tool ${tool} not found at ${_tool_path}")
        continue()
    endif()
    add_executable(Qt6::${tool} IMPORTED GLOBAL)
    set_property(TARGET Qt6::${tool} PROPERTY IMPORTED_LOCATION "${_tool_path}")
endforeach()

# Plugins, translations and the QPA platform plugins live next to the runtime.
set(QT6_RUNTIME_PLUGIN_DIR "${QT6_RUNTIME_ROOT}/plugins" CACHE PATH "Qt 6 plugin directory")
set(QT6_RUNTIME_LIB_DIR "${QT6_RUNTIME_ROOT}/lib" CACHE PATH "Qt 6 library directory")

# ---------------------------------------------------------------------------
# stub host libraries
#
# The prebuilt Qt libraries reference host libraries that are not installed
# everywhere (libxkbcommon, libdbus-1, libEGL, ...).  make-stub-libs.sh builds
# tiny stand ins for the handful of symbols Qt actually looks up, purely so
# that linking succeeds; at run time the real libraries are used when they are
# present.  On a machine that has the development packages installed none of
# this is needed and the directory simply does not exist.
# ---------------------------------------------------------------------------
if(NOT QT6_STUB_LIB_DIR AND DEFINED ENV{QT_STUB_LIBS})
    set(QT6_STUB_LIB_DIR "$ENV{QT_STUB_LIBS}")
endif()
if(NOT QT6_STUB_LIB_DIR)
    set(QT6_STUB_LIB_DIR "${CMAKE_SOURCE_DIR}/external/stub-libs")
endif()
if(IS_DIRECTORY "${QT6_STUB_LIB_DIR}")
    message(STATUS "Qt 6 stub libraries: ${QT6_STUB_LIB_DIR}")
    link_directories("${QT6_STUB_LIB_DIR}")
    list(APPEND CMAKE_BUILD_RPATH "${QT6_STUB_LIB_DIR}")
endif()

# The runtime libraries themselves are not in the default search path either, so
# every built binary gets an rpath pointing at them.  That keeps the tests and
# the application runnable straight out of the build directory.
list(APPEND CMAKE_BUILD_RPATH "${QT6_RUNTIME_LIB_DIR}")

# DT_RUNPATH (what GNU ld emits today) is only searched for the direct
# dependencies of an executable -- not for the dependencies of the Qt libraries
# themselves, which is exactly where libxkbcommon, libdbus-1 and friends are
# needed.  DT_RPATH *is* inherited down the dependency chain, so the binaries ask
# for the old tag: without it every binary would need LD_LIBRARY_PATH.
add_link_options("LINKER:--disable-new-dtags")

# Helper used by the packaging rules: absolute path of a Qt 6 shared library.
function(qt6_runtime_library module out_var)
    set(${out_var} "${QT6_RUNTIME_ROOT}/lib/libQt6${module}.so.6" PARENT_SCOPE)
endfunction()
