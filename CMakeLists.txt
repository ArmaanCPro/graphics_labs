cmake_minimum_required(VERSION 4.0)

# set toolchain file in command line
#set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_OVERLAY_PORTS "${CMAKE_CURRENT_SOURCE_DIR}/overlay-ports")

project(
        graphics_labs
        VERSION 1.0
        LANGUAGES CXX
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/Slang.cmake)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(nfd CONFIG REQUIRED)

add_subdirectory(labs)
