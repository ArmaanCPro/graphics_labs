file(GLOB_RECURSE ENGER_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE ENGER_HEADERS CONFIGURE_DEPENDS src/*.h)

add_executable(enger ${ENGER_SOURCES} ${ENGER_HEADERS})

target_include_directories(enger PRIVATE src)

target_link_libraries(enger PRIVATE glfw glm::glm Vulkan::Headers GPUOpen::VulkanMemoryAllocator)

set_project_warnings(enger)
#enable_sanitizers(enger)