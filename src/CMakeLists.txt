# Application sources.
#
# eda_core   - model, persistence, logging, importers, diagnostics, AI, database
#              (no dependency on QtWidgets, so it can be unit tested head less)
# eda_ui     - widgets, dialogs, application services
# pcb-workbench - the executable
add_library(eda_core STATIC
    core/Logging.cpp
    core/Paths.cpp
    core/ProjectModel.cpp
    core/Project.cpp
    core/SExpression.cpp
    core/KicadImport.cpp
    core/Database.cpp
    core/Measurements.cpp
)
target_include_directories(eda_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/../generated
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/core
)
target_link_libraries(eda_core PUBLIC Qt6::Core Qt6::Gui Qt6::Sql)
target_compile_options(eda_core PRIVATE -Wall -Wextra)

add_library(eda_ui STATIC
    application/Application.cpp
    application/AppIdentity.cpp
    application/Settings.cpp
    ui/AssistantPanel.cpp
    ui/BoardView.cpp
    ui/ComponentDetails.cpp
    ui/ComponentExplorer.cpp
    ui/LogPanel.cpp
    ui/MainWindow.cpp
    ui/SettingsDialog.cpp
    ui/TestPointPanel.cpp
    ui/Theme.cpp
)
target_include_directories(eda_ui
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ui ${CMAKE_CURRENT_SOURCE_DIR}/application
)
target_link_libraries(eda_ui PUBLIC eda_core Qt6::Widgets Qt6::Network Qt6::Sql)
target_compile_options(eda_ui PRIVATE -Wall -Wextra)

add_executable(pcb-workbench WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(pcb-workbench PRIVATE eda_ui)
target_compile_options(pcb-workbench PRIVATE -Wall -Wextra)

set_target_properties(pcb-workbench PROPERTIES OUTPUT_NAME "pcb-workbench")

# With a private Qt runtime the binary lives next to its libraries and is
# started through the wrapper in bin/; with the system Qt it goes to bin/
# directly, which is what a distribution package expects.
if(EDA_BUNDLE_PRIVATE_QT)
    set_target_properties(pcb-workbench PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
    install(TARGETS pcb-workbench RUNTIME DESTINATION "${EDA_BUNDLE_BIN_DIR}")
else()
    install(TARGETS pcb-workbench RUNTIME DESTINATION bin)
endif()
