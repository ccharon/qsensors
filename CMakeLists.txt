cmake_minimum_required(VERSION 4.2)
project(qsensors)

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
find_library(SENSORS_LIBRARY sensors REQUIRED)

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
target_include_directories(qsensors PRIVATE
        src/ui
        src/ui/panels
        src/ui/widgets
        src/sensors
)
target_link_libraries(qsensors
        Qt::Core
        Qt::Gui
        Qt::Widgets
        ${SENSORS_LIBRARY}
)

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
