# Desktop application target. Included from the root CMakeLists.txt, which
# defines QRS_SRC / QRS_MAIN, so no path here names the source folder itself.

set(QRS_APP_SOURCES
    ${QRS_SRC}/app/Actions.cpp
    ${QRS_SRC}/app/AppSettings.cpp
    ${QRS_SRC}/app/BatchPage.cpp
    ${QRS_SRC}/app/FilesPage.cpp
    ${QRS_SRC}/app/FilesPageActions.cpp
    ${QRS_SRC}/app/FileTableModel.cpp
    ${QRS_SRC}/app/GeneratorPage.cpp
    ${QRS_SRC}/app/GeneratorPageActions.cpp
    ${QRS_SRC}/app/MainWindow.cpp
    ${QRS_SRC}/app/ScannerPage.cpp
    ${QRS_SRC}/app/SettingsPage.cpp
    ${QRS_SRC}/app/Theme.cpp
    ${QRS_SRC}/app/Widgets.cpp)

# The pages are a static library so the GUI tests can link them too.
add_library(qrs_app STATIC ${QRS_APP_SOURCES})
target_include_directories(qrs_app PUBLIC ${QRS_SRC}/app)
target_link_libraries(qrs_app PUBLIC qrs_core Qt6::Widgets Qt6::Concurrent)
target_compile_definitions(qrs_app PRIVATE
    QRS_TOOLS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tools")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(qrs_app PRIVATE -Wall -Wextra)
endif()

add_executable(QRStudio WIN32 ${QRS_MAIN}/main.cpp)
target_link_libraries(QRStudio PRIVATE qrs_app)
target_compile_definitions(QRStudio PRIVATE QRS_VERSION="${PROJECT_VERSION}")
