cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCES
    *.h
    *.cpp
)

project(vulkan_project)
add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic) #-Wno-zero-as-null-pointer-constant)

if(WIN32)

include_directories(C:/VulkanSDK/1.1.108.0/Include/vulkan)
target_link_libraries(${PROJECT_NAME} C:/VulkanSDK/1.1.108.0/Lib/vulkan-1.lib)

elseif(UNIX)

include_directories(/home/olka/vulkan/1.1.108.0/x86_64/include/vulkan)
target_link_libraries(${PROJECT_NAME} /home/olka/vulkan/1.1.108.0/x86_64/lib/libvulkan.so)

endif()
