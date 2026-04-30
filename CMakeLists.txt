cmake_minimum_required(VERSION 3.22)
project(KLauncher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 components
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    WebEngineWidgets
    Concurrent
    Sql
    Xml
)

qt_standard_project_setup()

# Collect all sources
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.h
)

# Resources
set(RESOURCES resources/resources.qrc)

qt_add_executable(KLauncher
    ${SOURCES}
    ${RESOURCES}
    main.cpp
)

target_include_directories(KLauncher PRIVATE
    src
    src/ui
    src/auth
    src/minecraft
    src/modloaders
    src/mods
    src/java
    src/utils
)

target_link_libraries(KLauncher PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebEngineWidgets
    Qt6::Concurrent
    Qt6::Sql
    Qt6::Xml
)

# Version info
target_compile_definitions(KLauncher PRIVATE
    KLAUNCHER_VERSION="${PROJECT_VERSION}"
    KLAUNCHER_NAME="KLauncher"
)

# Install
install(TARGETS KLauncher
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# Desktop entry for Linux
if(UNIX AND NOT APPLE)
    install(FILES scripts/klauncher.desktop
        DESTINATION share/applications
    )
endif()
