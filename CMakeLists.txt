cmake_minimum_required (VERSION 3.0.2)
# allows access to environment variables with the name 
project (VulkanFramework)

#set the build type if its not set
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE )
endif()

#if user didnt set install dir, override it and write it to the cache -> Type and description necessary,
#overwrite variable, not just write it to cache
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install" CACHE STRING "Install path prefix, prepended onto install directories." FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

# installation
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

#before adding libraries, define the output paths
# MSVC & Xcode automatically create the build-type folders
if(MSVC OR CMAKE_GENERATOR STREQUAL Xcode)
  SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
  SET(LIBRARY_OUTPUT_PATH	 ${PROJECT_BINARY_DIR})
else()
  SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
  SET(LIBRARY_OUTPUT_PATH	 ${PROJECT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
endif()

# Location where cmake first looks for modules.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
set(GLOBAL_EXT_DIR ${CMAKE_SOURCE_DIR}/external)
include(FindDependency)
include(AddSubLibrary)

#open mpi
find_package(MPI REQUIRED)

# vulkan SDK
find_package(Vulkan REQUIRED)
include_directories(${VULKAN_INCLUDE_DIR})

# include cmdline
include_directories(SYSTEM external/cmdline-e4cd007)

# include glm, as system header to suppress warnings
include_directories(SYSTEM external/glm-0.9.8.4)

# include stb_image, as system header to suppress warnings
include_directories(SYSTEM external/stb_image-2.0.6)

# include gli, as system header to suppress warnings
include_directories(SYSTEM external/gli-0.8.2.0)

# add tinyobjloader
include_directories(external/tinyobjloader-1.0.5)
# just one file so it can be directly compiled into main exe
file(GLOB TINYOBJLOADER_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/external/tinyobjloader-1.0.5/*.cc)

# add tinyobjloader
include_directories(external/jsoncpp-1.8.0)
# just one file so it can be directly compiled into main exe
file(GLOB JSONCPP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/external/jsoncpp-1.8.0/*.cpp)

# bvh loading files
add_sublibrary(bvh ${CMAKE_CURRENT_SOURCE_DIR}/external/bvh "")

# add spirv cross build system
add_subdirectory(external/SPIRV-Cross-8199986)

# glfw
# configure glfw building
option(GLFW_BUILD_DOCS     OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS    OFF)
option(GLFW_INSTALL        OFF)

# add glfw build system
add_subdirectory(external/glfw-3.2.1)

# create framework helper library 
file(GLOB_RECURSE FRAMEWORK_SOURCES framework/source/*.cpp)
add_library(framework STATIC ${FRAMEWORK_SOURCES} ${TINYOBJLOADER_SOURCES})
target_include_directories(framework PUBLIC framework/include)
target_link_libraries(framework glfw ${GLFW_LIBRARIES} ${VULKAN_LIBRARY} spirv-cross-core bvh)

# create framework helper library 
file(GLOB_RECURSE SCENEGRAPH_SOURCES scenegraph/source/*.cpp)
add_library(scenegraph STATIC ${SCENEGRAPH_SOURCES} ${JSONCPP_SOURCES})
target_include_directories(scenegraph PUBLIC scenegraph/include)
target_link_libraries(scenegraph ${VULKAN_LIBRARY} framework)

include_directories(application/include)

# output executables
add_executable(vulkan_test application/source/application_vulkan.cpp)
target_link_libraries(vulkan_test framework)
install(TARGETS vulkan_test DESTINATION .)

add_executable(vulkan_present application/source/application_present.cpp)
target_link_libraries(vulkan_present framework)
install(TARGETS vulkan_present DESTINATION .)

add_executable(vulkan_threaded application/source/application_threaded_simple.cpp)
target_link_libraries(vulkan_threaded framework)
install(TARGETS vulkan_threaded DESTINATION .)

add_executable(vulkan_threaded_min application/source/application_threaded_min.cpp)
target_link_libraries(vulkan_threaded_min framework)
install(TARGETS vulkan_threaded_min DESTINATION .)

add_executable(vulkan_lod application/source/application_lod.cpp)
target_link_libraries(vulkan_lod framework)
install(TARGETS vulkan_lod DESTINATION .)

add_executable(vulkan_lod_mpi application/source/application_lod_mpi.cpp)
target_link_libraries(vulkan_lod_mpi framework ${MPI_CXX_LIBRARIES})
target_include_directories(vulkan_lod_mpi SYSTEM PRIVATE ${MPI_CXX_INCLUDE_PATH})
install(TARGETS vulkan_lod_mpi DESTINATION .)

add_executable(vulkan_lod_single application/source/application_lod_single.cpp)
target_link_libraries(vulkan_lod_single framework)
install(TARGETS vulkan_lod_single DESTINATION .)

add_executable(vulkan_clustered application/source/application_clustered.cpp)
target_link_libraries(vulkan_clustered framework)
install(TARGETS vulkan_clustered DESTINATION .)

add_executable(vulkan_compute application/source/application_compute.cpp)
target_link_libraries(vulkan_compute framework)
install(TARGETS vulkan_compute DESTINATION .)

add_executable(vulkan_scenegraph application/source/application_scenegraph.cpp)
target_link_libraries(vulkan_scenegraph scenegraph)
install(TARGETS vulkan_scenegraph DESTINATION .)

add_executable(vulkan_scenegraph_clustered application/source/application_scenegraph_clustered.cpp)
target_link_libraries(vulkan_scenegraph_clustered scenegraph)
install(TARGETS vulkan_scenegraph_clustered DESTINATION .)

# set build type dependent flags
if(UNIX)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fstack-protector-all")
elseif(MSVC)
	set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2")
	set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi")
endif()

# activate C++ 11
if(NOT MSVC)
    add_definitions(-std=c++11)
    # show warnings but suppress those caused by glm
    add_definitions(-Wall -Wconversion)
    # force linking  with c++11 lib
    if(APPLE)
        set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++0x")
        add_definitions(-stdlib=libc++)
    endif()
else()
    # build in parallel, show warnings and suppress one caused by glbinding
    add_definitions(/MP /W3 /wd4251)
endif()

# remove external configuration vars from cmake gui
mark_as_advanced(OPTION_SELF_CONTAINED)
mark_as_advanced(GLFW_BUILD_DOCS GLFW_INSTALL GLFW_BUILD_EXAMPLES GLFW_BUILD_TESTS
 GLFW_VULKN_STATIC GLFW_DOCUMENT_INTERNALS GLFW_USE_EGL GLFW_USE_MIR 
 GLFW_USE_WAYLAND GLFW_LIBRARIES LIB_SUFFIX BUILD_SHARED_LIBS)

# compile shaders
# get list of shaders
file(GLOB SHADERS "resources/shaders/*")

# create folder for compiled shaders
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/resources/shaders)
# add shader compilation command sand target
foreach(_SHADER ${SHADERS})
  # compute output file name
  get_filename_component(_EXT ${_SHADER} EXT)
  get_filename_component(_NAME ${_SHADER} NAME_WE)
  string(SUBSTRING ${_EXT} 1 -1 _EXT)
  string(CONCAT _NAME_OUT ${_NAME}_ ${_EXT} ".spv")
  string(CONCAT _SHADER_OUT ${PROJECT_BINARY_DIR} "/resources/shaders/" ${_NAME_OUT})
  # add to output list
  list(APPEND SHADER_OUTPUTS ${_SHADER_OUT})
  add_custom_command(
    OUTPUT ${_SHADER_OUT}
    COMMAND "$ENV{VULKAN_SDK}/bin/glslangValidator" -V ${_SHADER} -o ${_SHADER_OUT}
    DEPENDS ${_SHADER}
    COMMENT "compiling ${_NAME}.${_EXT} to ${_NAME_OUT}" 
  )
endforeach()
# add target depending on shaders to compile when building
add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUTS})

# installation rules, copy compiled shaders
install(DIRECTORY ${PROJECT_BINARY_DIR}/resources
  DESTINATION ${CMAKE_INSTALL_PREFIX})