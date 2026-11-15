cmake_minimum_required(VERSION 4.2)
project(qsensors VERSION 0.80.11)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(QSENSORS_WARNINGS_AS_ERRORS "Treat compiler warnings in qsensors code as errors" OFF)
option(QSENSORS_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# Defines BUILD_TESTING, which decides whether Qt Test is required.
include(CTest)

set(QSENSORS_QT_COMPONENTS Core Gui Widgets LinguistTools)
if (BUILD_TESTING)
    list(APPEND QSENSORS_QT_COMPONENTS Test)
endif ()
find_package(Qt6 REQUIRED COMPONENTS ${QSENSORS_QT_COMPONENTS})

# Warnings and sanitizers apply to qsensors' own targets only (PRIVATE use of this
# interface library, so nothing leaks to consumers). Generated moc/rcc sources are
# compiled with the same flags and are checked to be warning-free.
add_library(qsensors_build_options INTERFACE)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(qsensors_build_options INTERFACE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wold-style-cast
            # Designated initializers omit members on purpose; they have default member initializers.
            -Wno-missing-field-initializers)
    if (QSENSORS_WARNINGS_AS_ERRORS)
        target_compile_options(qsensors_build_options INTERFACE -Werror)
    endif ()
endif ()
if (QSENSORS_SANITIZE)
    target_compile_options(qsensors_build_options INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
    target_link_options(qsensors_build_options INTERFACE -fsanitize=address,undefined)
endif ()
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

# Layering is enforced by the link graph: the model knows neither widgets nor
# libsensors, the UI knows no libsensors. All includes are relative to src/.
set(QSENSORS_MODEL_SOURCES
        src/config/app_config_store.cpp
        src/config/runtime_config.cpp
        src/config/settings_schema.cpp
        src/sensors/reading_pipeline.cpp
        src/sensors/sensor_format.cpp
        src/sensors/sensor_monitor.cpp
)
set(QSENSORS_UI_SOURCES
        src/ui/main_window.cpp
        src/ui/main_window_state_store.cpp
        src/ui/theme/app_theme.cpp
        src/ui/panels/card_grid_plan.cpp
        src/ui/panels/category_row_layout.cpp
        src/ui/panels/settings_panel.cpp
        src/ui/panels/sensors_panel.cpp
        src/ui/widgets/collapsible_section.cpp
        src/ui/widgets/lcd_segment_font.cpp
        src/ui/widgets/lcd_display_widget.cpp
        src/ui/widgets/sensor_value_widget.cpp
        src/ui/widgets/status_line.cpp
        src/ui/widgets/vertical_scroll_area.cpp
)
set(QSENSORS_APP_SOURCES
        src/main.cpp
        src/sensors/libsensors_source.cpp
)

add_library(qsensors_model STATIC ${QSENSORS_MODEL_SOURCES})
target_include_directories(qsensors_model PUBLIC src)
target_link_libraries(qsensors_model PUBLIC Qt::Core)
target_link_libraries(qsensors_model PRIVATE qsensors_build_options)

add_library(qsensors_ui STATIC ${QSENSORS_UI_SOURCES})
target_link_libraries(qsensors_ui PUBLIC
        qsensors_model
        Qt::Gui
        Qt::Widgets
)
target_link_libraries(qsensors_ui PRIVATE qsensors_build_options)

add_executable(qsensors ${QSENSORS_APP_SOURCES} resources/resources.qrc)
target_compile_definitions(qsensors PRIVATE QSENSORS_VERSION="${PROJECT_VERSION}")
if (NOT SENSORS_FOUND)
    target_include_directories(qsensors PRIVATE ${SENSORS_INCLUDE_DIR})
endif ()
target_link_libraries(qsensors qsensors_ui)
target_link_libraries(qsensors qsensors_build_options)
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

# Explicit SOURCES: the translatable UI lives in the libraries, not in the app target.
qt_add_translations(qsensors
        TS_FILES ${TS_FILES}
        SOURCES ${QSENSORS_MODEL_SOURCES} ${QSENSORS_UI_SOURCES} ${QSENSORS_APP_SOURCES}
        RESOURCE_PREFIX "/i18n"
        LUPDATE_OPTIONS -no-obsolete -locations none
)

if (BUILD_TESTING)
    # Tests creating widgets run on the offscreen platform, so CI needs no display.
    # Model tests link only the model, so they cannot pull in widgets by accident.
    function(add_qsensors_test target source library)
        add_executable(${target} ${source})
        target_link_libraries(${target} ${library} Qt::Test qsensors_build_options)
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endfunction()

    add_qsensors_test(test_sensors_policy tests/test_sensors_policy.cpp qsensors_model)
    add_qsensors_test(test_reading_pipeline tests/test_reading_pipeline.cpp qsensors_model)
    add_qsensors_test(test_sensor_monitor tests/test_sensor_monitor.cpp qsensors_model)
    add_qsensors_test(test_lcd_logic tests/test_lcd_logic.cpp qsensors_ui)
    add_qsensors_test(test_glyph_model tests/test_glyph_model.cpp qsensors_ui)
    add_qsensors_test(test_settings_persistence tests/test_settings_persistence.cpp qsensors_ui)
    add_qsensors_test(test_sensor_contract tests/test_sensor_contract.cpp qsensors_ui)
    add_qsensors_test(test_sensor_identity tests/test_sensor_identity.cpp qsensors_model)
    add_qsensors_test(test_sensors_panel tests/test_sensors_panel.cpp qsensors_ui)
    add_qsensors_test(test_theme_refresh tests/test_theme_refresh.cpp qsensors_ui)
    add_qsensors_test(test_status_line tests/test_status_line.cpp qsensors_ui)
    add_qsensors_test(test_window_sizing tests/test_window_sizing.cpp qsensors_ui)
    add_qsensors_test(test_main_window tests/test_main_window.cpp qsensors_ui)
endif ()
