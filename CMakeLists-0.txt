cmake_minimum_required(VERSION 3.16)
project(GisDynamicClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# 🌟 引入 Qt5 基础模块及 WebSockets
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Xml Svg WebSockets)

find_path(QGIS_INCLUDE_DIR qgsapplication.h PATHS /usr/include/qgis /usr/include)
find_library(QGIS_CORE_LIBRARY NAMES qgis_core PATHS /usr/lib /usr/lib/x86_64-linux-gnu)
find_library(QGIS_GUI_LIBRARY NAMES qgis_gui PATHS /usr/lib /usr/lib/x86_64-linux-gnu)
find_package(GDAL REQUIRED)

include_directories(
    ${QGIS_INCLUDE_DIR}
    ${GDAL_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 🌟 将 gisapiserver 源码加入编译
add_executable(GisDynamicClient
    main.cpp
    mainwindow.h
    mainwindow.cpp
    gisapiserver.h
    gisapiserver.cpp
)

target_link_libraries(GisDynamicClient PRIVATE
    Qt5::Core 
    Qt5::Gui 
    Qt5::Widgets 
    Qt5::Xml 
    Qt5::Svg 
    Qt5::WebSockets
    ${QGIS_CORE_LIBRARY}
    ${QGIS_GUI_LIBRARY}
    ${GDAL_LIBRARIES}
)