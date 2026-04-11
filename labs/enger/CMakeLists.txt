file(GLOB_RECURSE ENGER_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE ENGER_HEADERS CONFIGURE_DEPENDS src/*.h)

add_executable(enger ${ENGER_SOURCES} ${ENGER_HEADERS})

target_include_directories(enger PRIVATE src)

target_link_libraries(enger PRIVATE glfw glm::glm Vulkan::Headers GPUOpen::VulkanMemoryAllocator imgui::imgui)

target_compile_definitions(enger PRIVATE
        GLFW_INCLUDE_NONE
        VK_NO_PROTOTYPES
        VULKAN_HPP_NO_EXCEPTIONS
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VMA_STATIC_VULKAN_FUNCTIONS=0
        VMA_DYNAMIC_VULKAN_FUNCTIONS=1

        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
)

set_project_warnings(enger)
#enable_sanitizers(enger)