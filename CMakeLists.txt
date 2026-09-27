cmake_minimum_required(VERSION 4.2)
project(qsensors VERSION 0.80.9)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        Test
        LinguistTools
        REQUIRED)
find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_check_modules(SENSORS QUIET IMPORTED_TARGET libsensors)
    if (NOT SENSORS_FOUND)
        pkg_check_modules(SENSORS QUIET IMPORTED_TARGET sensors)
    endif ()
endif ()

if (NOT SENSORS_FOUND)
    find_path(SENSORS_INCLUDE_DIR NAMES sensors/sensors.h REQUIRED)
    find_library(SENSORS_LIBRARY NAMES sensors REQUIRED)
endif ()

# Everything except the libsensors backend and the main window; shared by the
# app and the tests, so it needs no libsensors.
set(QSENSORS_CORE_SOURCES
        src/config/app_config_store.cpp
        src/config/runtime_config.cpp
        src/config/settings_schema.cpp
        src/sensors/sensor_format.cpp
        src/ui/main_window_state_store.cpp
        src/ui/theme/app_theme.cpp
        src/ui/panels/settings_panel.cpp
        src/ui/panels/sensors_panel.cpp
        src/ui/widgets/collapsible_section.cpp
        src/ui/widgets/lcd_segment_font.cpp
        src/ui/widgets/lcd_display_widget.cpp
        src/ui/widgets/sensor_value_widget.cpp
        src/ui/widgets/status_line.cpp
)
set(QSENSORS_APP_SOURCES
        src/main.cpp
        src/ui/main_window.cpp
        src/sensors/sensors_backend.cpp
)

add_library(qsensors_core STATIC ${QSENSORS_CORE_SOURCES})
target_include_directories(qsensors_core PUBLIC
        src/config
        src/ui
        src/ui/panels
        src/ui/widgets
        src/sensors
)
target_link_libraries(qsensors_core PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
)

add_executable(qsensors ${QSENSORS_APP_SOURCES} resources/resources.qrc)
target_compile_definitions(qsensors PRIVATE QSENSORS_VERSION="${PROJECT_VERSION}")
if (NOT SENSORS_FOUND)
    target_include_directories(qsensors PRIVATE ${SENSORS_INCLUDE_DIR})
endif ()
target_link_libraries(qsensors qsensors_core)
if (SENSORS_FOUND)
    target_link_libraries(qsensors PkgConfig::SENSORS)
else ()
    target_link_libraries(qsensors ${SENSORS_LIBRARY})
endif ()

include(GNUInstallDirs)
install(TARGETS qsensors RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# Desktop integration (XDG): menu entry plus bitmap and scalable app icon.
install(FILES resources/qsensors.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/icons/qsensors.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
install(FILES resources/icons/qsensors.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_QUIET TRUE)
if (SENSORS_FOUND)
    set(CMAKE_REQUIRED_LIBRARIES PkgConfig::SENSORS)
    set(CMAKE_REQUIRED_INCLUDES ${SENSORS_INCLUDE_DIRS})
else ()
    set(CMAKE_REQUIRED_LIBRARIES ${SENSORS_LIBRARY})
    set(CMAKE_REQUIRED_INCLUDES ${SENSORS_INCLUDE_DIR})
endif ()
check_cxx_source_compiles("
    #include <sensors/sensors.h>
    int main() {
        return sensors_init(nullptr);
    }
" QSENSORS_LIBSENSORS_COMPILE_OK)
if (NOT QSENSORS_LIBSENSORS_COMPILE_OK)
    message(FATAL_ERROR "libsensors development files are missing or unusable (headers/library mismatch)")
endif ()

set(TS_FILES
        i18n/qsensors_en.ts
        i18n/qsensors_de.ts
        i18n/qsensors_fr.ts
        i18n/qsensors_es.ts
        i18n/qsensors_pt.ts
)

# Explicit SOURCES: the translatable UI lives in qsensors_core, not in the app target.
qt_add_translations(qsensors
        TS_FILES ${TS_FILES}
        SOURCES ${QSENSORS_CORE_SOURCES} ${QSENSORS_APP_SOURCES}
        RESOURCE_PREFIX "/i18n"
        LUPDATE_OPTIONS -no-obsolete -locations none
)

include(CTest)
if (BUILD_TESTING)
    # Tests creating widgets run on the offscreen platform, so CI needs no display.
    function(add_qsensors_test target source)
        add_executable(${target} ${source})
        target_link_libraries(${target} qsensors_core Qt::Test)
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endfunction()

    add_qsensors_test(test_sensors_policy tests/test_sensors_policy.cpp)
    add_qsensors_test(test_lcd_logic tests/test_lcd_logic.cpp)
    add_qsensors_test(test_glyph_model tests/test_glyph_model.cpp)
    add_qsensors_test(test_settings_persistence tests/test_settings_persistence.cpp)
    add_qsensors_test(test_sensor_contract tests/test_sensor_contract.cpp)
    add_qsensors_test(test_sensor_identity tests/test_sensor_identity.cpp)
    add_qsensors_test(test_sensors_panel tests/test_sensors_panel.cpp)
    add_qsensors_test(test_theme_refresh tests/test_theme_refresh.cpp)
    add_qsensors_test(test_status_line tests/test_status_line.cpp)
    add_qsensors_test(test_window_sizing tests/test_window_sizing.cpp)
endif ()
