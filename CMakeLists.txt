cmake_minimum_required(VERSION 3.5)

project(RabbitShot VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# 源文件
set(SOURCES
        main.cpp
        mainwindow.cpp
    screenshotcapture.cpp
    selectionoverlay.cpp
    screenshotpreview.cpp
    globalhotkey.cpp
)

# 头文件
set(HEADERS
    mainwindow.h
    screenshotcapture.h
    selectionoverlay.h
    screenshotpreview.h
    globalhotkey.h
)

add_executable(RabbitShot
    ${SOURCES}
    ${HEADERS}
)

target_link_libraries(RabbitShot Qt6::Core Qt6::Widgets)

# macOS 特定设置
if(APPLE)
    find_library(CARBON_FRAMEWORK Carbon)
    if(CARBON_FRAMEWORK)
        target_link_libraries(RabbitShot ${CARBON_FRAMEWORK})
        message(STATUS "Carbon framework found: ${CARBON_FRAMEWORK}")
    else()
        message(WARNING "Carbon framework not found")
    endif()
endif()

# 设置应用程序属性
set_target_properties(RabbitShot PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER com.rabbitshorshell.RabbitShot
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        WIN32_EXECUTABLE TRUE
)
