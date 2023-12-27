cmake_minimum_required(VERSION 3.16)

project(Scene LANGUAGES CXX)

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

#affects all projects, add folder "include" to include section in VS
include_directories("include")

file(GLOB_RECURSE app_source "src/*.c*")
source_group("src" FILES ${app_source})
file(GLOB_RECURSE app_header "include/*.h*")
source_group("include" FILES ${app_header})

###############################
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

#include headers to project (so they will be shown in include folder)
add_executable(${PROJECT_NAME} ${app_header} ${app_source})

###############################
find_package(VulkanEngine REQUIRED)
include_directories(${VulkanEngine_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${VulkanEngine_LIBRARIES})

file(COPY ${VulkanEngine_DLL_PATH} DESTINATION ${CMAKE_BINARY_DIR} FILES_MATCHING PATTERN *.dll)
file(COPY ${VulkanEngine_SHADERS}  DESTINATION ${CMAKE_BINARY_DIR}/shaders FILES_MATCHING PATTERN *.spv)
file(COPY ${VulkanEngine_ASSETS}   DESTINATION ${CMAKE_BINARY_DIR})
