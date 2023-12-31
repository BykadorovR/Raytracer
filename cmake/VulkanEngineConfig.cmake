# FindVulkanEngine
# -------
#
# Finds the VulkanEngine library
#
# This will define the following variables:
#
#   VulkanEngine_FOUND        -- True if the system has the VulkanEngine library
#   VulkanEngine_INCLUDE_DIRS -- The include directories for VulkanEngine
#   VulkanEngine_LIBRARIES    -- Libraries to link against
#   VulkanEngine_DLL_PATH     -- DLLs required by VulkanEngine, valid on Windows platform only
#   VulkanEngine_SHADERS      -- Shaders folder
#   VulkanEngine_ASSETS       -- Folder with mandatory images and models
# and the following imported targets:
#   VulkanEngine

include(FindPackageHandleStandardArgs)

if(DEFINED ENV{VulkanEngine_INSTALL_PREFIX})
  message(STATUS "ENVIRONMENT")
  set(VulkanEngine_INSTALL_PREFIX $ENV{VulkanEngine_INSTALL_PREFIX})
else()
  # Assume we are in <install-prefix>/cmake/VulkanEngineConfig.cmake
  get_filename_component(CMAKE_CURRENT_LIST_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
  get_filename_component(VulkanEngine_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../" ABSOLUTE)
endif()

# Include shaders.
set(VulkanEngine_SHADERS ${VulkanEngine_INSTALL_PREFIX}/shaders/)

find_package(Vulkan REQUIRED)
# Include directories.
set(VulkanEngine_INCLUDE_DIRS ${VulkanEngine_INSTALL_PREFIX}/include/Engine
                              ${VulkanEngine_INSTALL_PREFIX}/include/Graphic
                              ${VulkanEngine_INSTALL_PREFIX}/include/Primitive
                              ${VulkanEngine_INSTALL_PREFIX}/include/Utility
                              ${VulkanEngine_INSTALL_PREFIX}/include/Vulkan
                              ${VulkanEngine_INSTALL_PREFIX}/build/include/
                              ${Vulkan_INCLUDE_DIRS})
# VulkanEngine library only
file(GLOB VulkanEngine_LIBRARIES ${VulkanEngine_INSTALL_PREFIX}/build/libraries/*.lib)
list(APPEND VulkanEngine_LIBRARIES ${Vulkan_LIBRARIES})
# VulkanEngine dll path
set(VulkanEngine_DLL_PATH ${VulkanEngine_INSTALL_PREFIX}/build/bin/)
set(VulkanEngine_ASSETS ${VulkanEngine_INSTALL_PREFIX}/assets)

find_package_handle_standard_args(VulkanEngine DEFAULT_MSG VulkanEngine_LIBRARIES
                                                           VulkanEngine_INCLUDE_DIRS
                                                           VulkanEngine_DLL_PATH
                                                           VulkanEngine_SHADERS
                                                           VulkanEngine_ASSETS)