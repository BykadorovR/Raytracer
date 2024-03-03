cmake_minimum_required(VERSION 3.28)

project(VulkanEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

#need for mismatch resolving
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/libraries)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/libraries)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

ExternalProject_Add(
  glfw
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 4afa227a056681d2628894b0893527bf69496a41
  UPDATE_COMMAND ""
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF -DBUILD_SHARED_LIBS=ON -G "${CMAKE_GENERATOR}" ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw/src/glfw 
                                     -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}
                                     -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}
                                     -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG}
                                     -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE}
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw/src/glfw/include ${CMAKE_BINARY_DIR}/include
)

ExternalProject_Add(
  glm
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG aa6a2757a5152bfa0db667380fa93c64d5999d63
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  UPDATE_COMMAND ""
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glm/src/glm/glm ${CMAKE_BINARY_DIR}/include/glm
)

ExternalProject_Add(
  tinygltf
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tinygltf
  GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
  GIT_TAG 84a83d39f55dcd12d722b77c5181349f4c840f5b
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  UPDATE_COMMAND ""
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tinygltf/src/tinygltf/tiny_gltf.h ${CMAKE_BINARY_DIR}/include/tiny_gltf.h &&
                  ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tinygltf/src/tinygltf/stb_image.h ${CMAKE_BINARY_DIR}/include/stb_image.h &&
                  ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tinygltf/src/tinygltf/stb_image_write.h ${CMAKE_BINARY_DIR}/include/stb_image_write.h &&
                  ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tinygltf/src/tinygltf/json.hpp ${CMAKE_BINARY_DIR}/include/json.hpp
)

ExternalProject_Add(
  nvtx
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/nvtx
  GIT_REPOSITORY https://github.com/NVIDIA/NVTX.git
  GIT_TAG release-v3
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  UPDATE_COMMAND ""
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/nvtx/src/nvtx/c/include/ ${CMAKE_BINARY_DIR}/include
)

ExternalProject_Add(
  thread-pool
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/thread-pool
  GIT_REPOSITORY https://github.com/bshoshany/thread-pool.git
  GIT_TAG cabb3df5876c9a6824b07fcb0ff73d4a0e506ca0
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  UPDATE_COMMAND ""
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/thread-pool/src/thread-pool/include/ ${CMAKE_BINARY_DIR}/include
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/mikktspace)
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/mikktspace/CMakeLists.txt
  "cmake_minimum_required(VERSION 3.16)\n"
  "project(mikktspace LANGUAGES C)\n"
  "include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/mikktspace/src/mikktspace)\n"
  "add_library(mikktspace "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/mikktspace/src/mikktspace/mikktspace.c"
                          )"
  )

ExternalProject_Add(
  mikktspace
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/mikktspace
  GIT_REPOSITORY https://github.com/mmikk/MikkTSpace.git
  GIT_TAG 3e895b49d05ea07e4c2133156cfa94369e19e409
  UPDATE_COMMAND ""
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/mikktspace/
                                     -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}
                                     -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}
                                     -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG}
                                     -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE}
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/mikktspace/src/mikktspace/mikktspace.h ${CMAKE_BINARY_DIR}/include/mikktspace.h
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
  UPDATE_COMMAND ""
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/
                                     -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}
                                     -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}
                                     -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG}
                                     -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE}
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/src/imgui/imgui.h ${CMAKE_BINARY_DIR}/include/imgui.h &&
                  ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/src/imgui/imconfig.h ${CMAKE_BINARY_DIR}/include/imconfig.h
)

#affects all projects, add folder "include" to include section in VS
include_directories("include/Graphic")
include_directories("include/Primitive")
include_directories("include/Utility")
include_directories("include/Engine")
include_directories("include/Vulkan")

file(GLOB_RECURSE app_source_graphic "src/Graphic/*.c*")
source_group("Graphic\\src" FILES ${app_source_graphic})
file(GLOB_RECURSE app_header_graphic "include/Graphic/*.h*", "include/Graphic/*.ixx*")
source_group("Graphic\\include" FILES ${app_header_graphic})
file(GLOB_RECURSE app_source_primitive "src/Primitive/*.c*")
source_group("Primitive\\src" FILES ${app_source_primitive})
file(GLOB_RECURSE app_header_primitive "include/Primitive/*.h*", "include/Primitive/*.ixx*")
source_group("Primitive\\include" FILES ${app_header_primitive})
file(GLOB_RECURSE app_source_utility "src/Utility/*.c*")
source_group("Utility\\src" FILES ${app_source_utility})
file(GLOB_RECURSE app_header_utility "include/Utility/*.h*", "include/Utility/*.ixx*")
source_group("Utility\\include" FILES ${app_header_utility})
file(GLOB_RECURSE app_source_vulkan "src/Vulkan/*.c*")
source_group("Vulkan\\src" FILES ${app_source_vulkan})
file(GLOB_RECURSE app_header_vulkan "include/Vulkan/*.h*", "include/Vulkan/*.ixx*")
source_group("Vulkan\\include" FILES ${app_header_vulkan})
file(GLOB_RECURSE app_source_engine "src/Engine/*.c*")
source_group("Engine\\src" FILES ${app_source_engine})
file(GLOB_RECURSE app_header_engine "include/Engine/*.h*", "include/Engine/*.ixx*")
source_group("Engine\\include" FILES ${app_header_engine})

###############################
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

#include headers to project (so they will be shown in include folder)
add_library(${PROJECT_NAME} SHARED ${app_header_graphic}   ${app_source_graphic}
                                    ${app_header_primitive} ${app_source_primitive}
                                    ${app_header_utility}   ${app_source_utility}                                   
                                    ${app_header_vulkan}    ${app_source_vulkan}
                                    ${app_header_engine}    ${app_source_engine})
###############################

#enable multiple cores compilation for VS
if(MSVC)
 target_compile_options(${PROJECT_NAME} PRIVATE "/MP")
endif()

find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})

add_dependencies(${PROJECT_NAME} glfw glm imgui tinygltf nvtx thread-pool mikktspace)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/include)
#link glfw libraries
target_link_libraries(${PROJECT_NAME} ${CMAKE_BINARY_DIR}/libraries/glfw3dll.lib)
#link imgui libraries
target_link_libraries(${PROJECT_NAME} ${CMAKE_BINARY_DIR}/libraries/imgui.lib)
#link mikktspace library
target_link_libraries(${PROJECT_NAME} ${CMAKE_BINARY_DIR}/libraries/mikktspace.lib)