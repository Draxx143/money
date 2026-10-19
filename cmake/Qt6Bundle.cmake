# Qt6Bundle.cmake
#
# Installs the Qt runtime the application was linked against next to it, so the
# resulting package runs on a machine that does not have this exact Qt version.
#
# Why
# ---
# The normal, policy conformant way to package a Qt application for Debian and
# Ubuntu is to depend on the Qt libraries of the distribution
# ("Depends: libqt6widgets6, ...").  That is what -DUSE_SYSTEM_QT6=ON does, and
# it is what should be used when the package is built on the target system.
#
# This repository, however, is built against a self provisioned Qt runtime (see
# scripts/toolchain/setup-toolchain.sh) whose version is newer than the one in
# any distribution archive.  A package that declares "Depends: libqt6widgets6"
# would install cleanly and then fail to start, because the distribution's
# libQt6Core.so.6 is a different Qt build.  Rather than ship a package that
# looks fine and does not work, the libraries it was actually built against are
# installed privately under /usr/lib/<project> and are found through the RPATH
# of the binary and a small wrapper script in /usr/bin.
#
# Nothing of this runs when USE_SYSTEM_QT6=ON.

set(EDA_BUNDLE_PRIVATE_QT ON)

set(EDA_BUNDLE_DIR "lib/${PROJECT_NAME}")
set(EDA_BUNDLE_BIN_DIR "${EDA_BUNDLE_DIR}/bin")
set(EDA_BUNDLE_LIB_DIR "${EDA_BUNDLE_DIR}/lib")
set(EDA_BUNDLE_PLUGIN_DIR "${EDA_BUNDLE_DIR}/plugins")

# --- Qt modules the application uses -----------------------------------------
# Everything else in the runtime (designer, qml, 3d, ...) is left out; shipping
# libraries the application never loads would only make the package bigger.
set(_eda_bundle_modules Core Gui Widgets Network Sql DBus Concurrent Xml PrintSupport)
set(_eda_bundle_libs "")
foreach(_module ${_eda_bundle_modules})
    file(GLOB _found "${QT6_RUNTIME_LIB_DIR}/libQt6${_module}.so.*")
    list(APPEND _eda_bundle_libs ${_found})
endforeach()

# QtCore is built against ICU; without it the application cannot start.
file(GLOB _eda_icu_libs "${QT6_RUNTIME_LIB_DIR}/libicu*.so.*")
list(APPEND _eda_bundle_libs ${_eda_icu_libs})

list(LENGTH _eda_bundle_libs _eda_bundle_count)
if(_eda_bundle_count EQUAL 0)
    message(FATAL_ERROR "No Qt runtime libraries found in ${QT6_RUNTIME_LIB_DIR}")
endif()
list(REMOVE_DUPLICATES _eda_bundle_libs)
install(FILES ${_eda_bundle_libs} DESTINATION "${EDA_BUNDLE_LIB_DIR}")

# --- plugins -----------------------------------------------------------------
# Only the plugin categories a Widgets application needs.  The SQL driver is
# limited to SQLite: the other drivers would pull in database client libraries
# that this application never uses.
foreach(_category platforms platforminputcontexts platformthemes
                  imageformats iconengines tls xcbglintegrations generic)
    if(IS_DIRECTORY "${QT6_RUNTIME_PLUGIN_DIR}/${_category}")
        install(DIRECTORY "${QT6_RUNTIME_PLUGIN_DIR}/${_category}"
                DESTINATION "${EDA_BUNDLE_PLUGIN_DIR}"
                PATTERN "*.so" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                                            GROUP_READ GROUP_EXECUTE
                                            WORLD_READ WORLD_EXECUTE)
    endif()
endforeach()
if(EXISTS "${QT6_RUNTIME_PLUGIN_DIR}/sqldrivers/libqsqlite.so")
    install(FILES "${QT6_RUNTIME_PLUGIN_DIR}/sqldrivers/libqsqlite.so"
            DESTINATION "${EDA_BUNDLE_PLUGIN_DIR}/sqldrivers")
endif()

# --- wrapper -----------------------------------------------------------------
configure_file("${CMAKE_CURRENT_LIST_DIR}/pcb-workbench.sh.in"
               "${CMAKE_CURRENT_BINARY_DIR}/pcb-workbench" @ONLY)
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/pcb-workbench" DESTINATION bin)

message(STATUS "Packaging: bundling the private Qt runtime from ${QT6_RUNTIME_LIB_DIR}")
