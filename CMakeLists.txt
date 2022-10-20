cmake_minimum_required(VERSION 3.16)

project(Raytracer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

if (UNIX)
  cmake_policy(SET CMP0072 NEW)
endif()

#need for mismatch resolving
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

ExternalProject_Add(
  glfw
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 4afa227a056681d2628894b0893527bf69496a41
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF -DBUILD_SHARED_LIBS=ON -G "${CMAKE_GENERATOR}" ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw/src/glfw -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG} -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}
  INSTALL_COMMAND ""
)

ExternalProject_Add(
  glm
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG aa6a2757a5152bfa0db667380fa93c64d5999d63
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
)

#affects all projects, add folder "include" to include section in VS
include_directories("include/Graphic")
include_directories("include/Primitive")
include_directories("include/Common")
include_directories("dependencies")


file(GLOB_RECURSE app_source_graphic "src/Graphic/*.c*")
source_group("Graphic\\src" FILES ${app_source_graphic})
file(GLOB_RECURSE app_header_graphic "include/Graphic/*.h*")
source_group("Graphic\\include" FILES ${app_header_graphic})
file(GLOB_RECURSE app_source_primitive "src/Primitive/*.c*")
source_group("Primitive\\src" FILES ${app_source_primitive})
file(GLOB_RECURSE app_header_primitive "include/Primitive/*.h*")
source_group("Primitive\\include" FILES ${app_header_primitive})
file(GLOB_RECURSE app_source_common "src/Common/*.c*")
source_group("Common\\src" FILES ${app_source_common})
file(GLOB_RECURSE app_header_common "include/Common/*.h*")
source_group("Common\\include" FILES ${app_header_common})
file(GLOB_RECURSE app_source_entry "src/Entry/*.c*")
source_group("Entry\\src" FILES ${app_source_entry})

###############################
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

#include headers to project (so they will be shown in include folder)
add_executable(${PROJECT_NAME} ${app_header_graphic} ${app_source_graphic}
                               ${app_header_primitive} ${app_source_primitive}
                               ${app_header_common} ${app_source_common}
                               ${app_source_entry})
###############################

find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})

add_dependencies(${PROJECT_NAME} glfw glm)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw/src/glfw/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glm/src/glm)
#link glfw libraries
target_link_libraries(${PROJECT_NAME} debug ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw/src/glfw-build/src/Debug/glfw3dll.lib)
target_link_libraries(${PROJECT_NAME} optimized ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw/src/glfw-build/src/Release/glfw3dll.lib)