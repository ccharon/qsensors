cmake_minimum_required(VERSION 4.2)
project(qsensors VERSION 0.80.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
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

add_executable(qsensors
        src/main.cpp
        resources/resources.qrc
        src/ui/main_window.cpp
        src/ui/panels/settings_panel.cpp
        src/ui/panels/sensors_panel.cpp
        src/ui/widgets/lcd_display_widget.cpp
        src/ui/widgets/sensor_value_widget.cpp
        src/sensors/sensors_backend.cpp
)
target_compile_definitions(qsensors PRIVATE QSENSORS_VERSION="${PROJECT_VERSION}")
target_include_directories(qsensors PRIVATE
        src/ui
        src/ui/panels
        src/ui/widgets
        src/sensors
)
if (NOT SENSORS_FOUND)
    target_include_directories(qsensors PRIVATE ${SENSORS_INCLUDE_DIR})
endif ()
target_link_libraries(qsensors
        Qt::Core
        Qt::Gui
        Qt::Widgets
)
if (SENSORS_FOUND)
    target_link_libraries(qsensors PkgConfig::SENSORS)
else ()
    target_link_libraries(qsensors ${SENSORS_LIBRARY})
endif ()

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
)

qt_add_translations(qsensors
        TS_FILES ${TS_FILES}
        RESOURCE_PREFIX "/i18n"
)
