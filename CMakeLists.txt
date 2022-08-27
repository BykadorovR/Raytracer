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
include_directories("include/")

file(GLOB_RECURSE app_source "src/*.c*")
source_group("src" FILES ${app_source})
file(GLOB_RECURSE app_header "include/*.h*")
source_group("include" FILES ${app_header})

###############################
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

#include headers to project (so they will be shown in include folder)
add_executable(${PROJECT_NAME} ${app_header} ${app_source})
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