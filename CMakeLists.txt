cmake_minimum_required(VERSION 3.22)
project(GtnhLauncher VERSION 0.1.0 LANGUAGES CXX)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network)
find_package(Java 17 REQUIRED COMPONENTS Development)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(MSA_CLIENT_ID "" CACHE STRING "Microsoft public client application ID")
option(GTNH_ENABLE_INSTALL_DIAGNOSTICS "Enable the developer-only install probe" OFF)
set(GTNH_SERVER "gtnh.jsir.tech" CACHE STRING "Minecraft server hostname")
set(GTNH_SERVER_PORT 25565 CACHE STRING "Minecraft server port")
configure_file(src/config.h.in config.h @ONLY)

qt_add_executable(GtnhLauncher WIN32
    src/main.cpp src/http.cpp src/auth.cpp src/credentials.cpp src/game.cpp
)
qt_add_resources(GtnhLauncher pack PREFIX "/" BASE resources FILES resources/pack.json)
target_include_directories(GtnhLauncher PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(GtnhLauncher PRIVATE Qt6::Widgets Qt6::Network)
if(MSVC)
    target_compile_options(GtnhLauncher PRIVATE /utf-8)
endif()
if(WIN32)
    target_link_libraries(GtnhLauncher PRIVATE advapi32)
endif()

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Extract.class"
    COMMAND "${Java_JAVAC_EXECUTABLE}" --release 17 -d "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/Extract.java"
    DEPENDS tools/Extract.java VERBATIM)
add_custom_target(archive_helper ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/Extract.class")
add_dependencies(GtnhLauncher archive_helper)
install(TARGETS GtnhLauncher RUNTIME DESTINATION .)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/Extract.class" DESTINATION .)
