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

ExternalProject_Add(
  tinygltf
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tinygltf
  GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
  GIT_TAG 84a83d39f55dcd12d722b77c5181349f4c840f5b
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
)

ExternalProject_Add(
  tinyobjloader
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tinyobjloader
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
  GIT_TAG 54684096e4ab1fcff9e7571888489e48d018c7fb
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui)
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/CMakeLists.txt
  "cmake_minimum_required(VERSION 3.16)\n"
  "project(imgui LANGUAGES CXX)\n"
  "include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/src/imgui)\n"
  "add_library(imgui "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/src/imgui/imgui.cpp"
                     "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/src/imgui/imgui_draw.cpp"
                     "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/src/imgui/imgui_tables.cpp"
                     "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/src/imgui/imgui_widgets.cpp"
                     "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/src/imgui/imgui_demo.cpp"
                     )"
  )

ExternalProject_Add(
  imgui
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui
  GIT_TAG 70cca1eac07aa3809bdd3717253c3754a5b7cfcc
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/
  INSTALL_COMMAND ""
)

#affects all projects, add folder "include" to include section in VS
include_directories("include/Graphic")
include_directories("include/Primitive")
include_directories("include/Utility")
include_directories("include/Engine")

file(GLOB_RECURSE app_source_graphic "src/Graphic/*.c*")
source_group("Graphic\\src" FILES ${app_source_graphic})
file(GLOB_RECURSE app_header_graphic "include/Graphic/*.h*")
source_group("Graphic\\include" FILES ${app_header_graphic})
file(GLOB_RECURSE app_source_primitive "src/Primitive/*.c*")
source_group("Primitive\\src" FILES ${app_source_primitive})
file(GLOB_RECURSE app_header_primitive "include/Primitive/*.h*")
source_group("Primitive\\include" FILES ${app_header_primitive})
file(GLOB_RECURSE app_source_utility "src/Utility/*.c*")
source_group("Utility\\src" FILES ${app_source_utility})
file(GLOB_RECURSE app_header_utility "include/Utility/*.h*")
source_group("Utility\\include" FILES ${app_header_utility})
file(GLOB_RECURSE app_source_engine "src/Engine/*.c*")
source_group("Engine\\src" FILES ${app_source_engine})
file(GLOB_RECURSE app_header_engine "include/Engine/*.h*")
source_group("Engine\\include" FILES ${app_header_engine})
file(GLOB_RECURSE app_source_entry "src/Entry/*.c*")
source_group("Entry\\src" FILES ${app_source_entry})

###############################
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

#include headers to project (so they will be shown in include folder)
add_executable(${PROJECT_NAME} ${app_header_graphic} ${app_source_graphic}
                               ${app_header_primitive} ${app_source_primitive}
                               ${app_header_utility} ${app_source_utility}
                               ${app_header_engine} ${app_source_engine}
                               ${app_source_entry})
###############################

find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})

add_dependencies(${PROJECT_NAME} glfw glm imgui tinygltf tinyobjloader)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw/src/glfw/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glm/src/glm)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/src/imgui)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tinygltf/src/tinygltf)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tinyobjloader/src/tinyobjloader)
#link glfw libraries
target_link_libraries(${PROJECT_NAME} debug ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw/src/glfw-build/src/Debug/glfw3dll.lib)
target_link_libraries(${PROJECT_NAME} optimized ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw/src/glfw-build/src/Release/glfw3dll.lib)
#link imgui libraries
target_link_libraries(${PROJECT_NAME} debug ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/src/imgui-build/Debug/imgui.lib)
target_link_libraries(${PROJECT_NAME} optimized ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/src/imgui-build/Release/imgui.lib)
