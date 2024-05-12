cmake_minimum_required(VERSION 3.22)

project(VulkanEngine LANGUAGES CXX)

function(imports name)
  get_cmake_property(_variableNames VARIABLES)
  foreach(_variableName ${_variableNames})
    if(_variableName MATCHES "^${name}")
      message(STATUS "${_variableName}")
      message(STATUS "\t${${_variableName}}")
    endif()
  endforeach()
endfunction()

set(CMAKE_CXX_STANDARD 20)

#need for mismatch resolving
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/libraries)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/libraries)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
include(FetchContent)

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

set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "Disable tinygltf loader example")
set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "Build header only tinygltf")
FetchContent_Declare(
        tinygltf
        GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
        GIT_TAG        84a83d39f55dcd12d722b77c5181349f4c840f5b
)

FetchContent_Declare(
        thread-pool
        GIT_REPOSITORY https://github.com/bshoshany/thread-pool.git
        GIT_TAG        cabb3df5876c9a6824b07fcb0ff73d4a0e506ca0
)

file(MAKE_DIRECTORY ${FETCHCONTENT_BASE_DIR}/mikktspace)
file(WRITE ${FETCHCONTENT_BASE_DIR}/mikktspace/CMakeLists.txt
  "cmake_minimum_required(VERSION 3.22)\n"
  "project(mikktspace LANGUAGES C)\n"
  "include_directories(${FETCHCONTENT_BASE_DIR}/mikktspace-src)\n"
  "add_library(mikktspace "${FETCHCONTENT_BASE_DIR}/mikktspace-src/mikktspace.c")"
)
FetchContent_Declare(
        mikktspace
        DOWNLOAD_COMMAND git clone https://github.com/mmikk/MikkTSpace.git ${FETCHCONTENT_BASE_DIR}/mikktspace-src &&
                         cd ${FETCHCONTENT_BASE_DIR}/mikktspace-src && git checkout 3e895b49d05ea07e4c2133156cfa94369e19e409 &&
                         ${CMAKE_COMMAND} -E copy ${FETCHCONTENT_BASE_DIR}/mikktspace/CMakeLists.txt ${FETCHCONTENT_BASE_DIR}/mikktspace-src/CMakeLists.txt

)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        aa6a2757a5152bfa0db667380fa93c64d5999d63
)

file(MAKE_DIRECTORY ${FETCHCONTENT_BASE_DIR}/imgui)
file(WRITE ${FETCHCONTENT_BASE_DIR}/imgui/CMakeLists.txt
  "cmake_minimum_required(VERSION 3.22)\n"
  "project(imgui LANGUAGES CXX)\n"
  "include_directories(${FETCHCONTENT_BASE_DIR}/imgui-src/)\n"
  "add_library(imgui "${FETCHCONTENT_BASE_DIR}/imgui-src/imgui.cpp"
                     "${FETCHCONTENT_BASE_DIR}/imgui-src/imgui_draw.cpp"
                     "${FETCHCONTENT_BASE_DIR}/imgui-src/imgui_tables.cpp"
                     "${FETCHCONTENT_BASE_DIR}/imgui-src/imgui_widgets.cpp"
                     "${FETCHCONTENT_BASE_DIR}/imgui-src/imgui_demo.cpp"
              )"
)

FetchContent_Declare(
  imgui
  DOWNLOAD_COMMAND git clone https://github.com/ocornut/imgui ${FETCHCONTENT_BASE_DIR}/imgui-src &&
                   cd ${FETCHCONTENT_BASE_DIR}/imgui-src && git checkout 70cca1eac07aa3809bdd3717253c3754a5b7cfcc &&
                   ${CMAKE_COMMAND} -E copy ${FETCHCONTENT_BASE_DIR}/imgui/CMakeLists.txt ${FETCHCONTENT_BASE_DIR}/imgui-src/CMakeLists.txt
)

FetchContent_Declare(
        nvtx
        GIT_REPOSITORY https://github.com/NVIDIA/NVTX.git
        GIT_TAG        release-v3
)

FetchContent_MakeAvailable(glm imgui tinygltf thread-pool mikktspace nvtx)

#affects all projects, add folder "include" to include section in VS
include_directories("include/Graphic")
include_directories("include/Primitive")
include_directories("include/Utility")
include_directories("include/Engine")
include_directories("include/Vulkan")

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
file(GLOB_RECURSE app_source_vulkan "src/Vulkan/*.c*")
source_group("Vulkan\\src" FILES ${app_source_vulkan})
file(GLOB_RECURSE app_header_vulkan "include/Vulkan/*.h*")
source_group("Vulkan\\include" FILES ${app_header_vulkan})
file(GLOB_RECURSE app_source_engine "src/Engine/*.c*")
source_group("Engine\\src" FILES ${app_source_engine})
file(GLOB_RECURSE app_header_engine "include/Engine/*.h*")
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

add_dependencies(${PROJECT_NAME} glfw)

file(COPY ${glm_SOURCE_DIR}/glm DESTINATION ${CMAKE_BINARY_DIR}/include)
file(COPY ${imgui_SOURCE_DIR}/imgui.h ${imgui_SOURCE_DIR}/imconfig.h DESTINATION ${CMAKE_BINARY_DIR}/include)
file(COPY ${tinygltf_SOURCE_DIR}/tiny_gltf.h ${tinygltf_SOURCE_DIR}/stb_image.h ${tinygltf_SOURCE_DIR}/stb_image_write.h ${tinygltf_SOURCE_DIR}/json.hpp DESTINATION ${CMAKE_BINARY_DIR}/include)
file(COPY ${thread-pool_SOURCE_DIR}/include/ DESTINATION ${CMAKE_BINARY_DIR}/include)
file(COPY ${mikktspace_SOURCE_DIR}/mikktspace.h DESTINATION ${CMAKE_BINARY_DIR}/include)
file(COPY ${nvtx_SOURCE_DIR}/c/include/ DESTINATION ${CMAKE_BINARY_DIR}/include)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/include)
#link glfw libraries
target_link_libraries(${PROJECT_NAME} ${CMAKE_BINARY_DIR}/libraries/glfw3dll.lib)

target_link_libraries(${PROJECT_NAME} imgui mikktspace)
