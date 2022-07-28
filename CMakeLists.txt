#===------------------------------------------------------------------------===#
#
#                     File System Linker
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
cmake_minimum_required(VERSION 3.9.0)
if (POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

if (POLICY CMP0042)
  # Enable `MACOSX_RPATH` by default.
  cmake_policy(SET CMP0042 NEW)
endif()

if (POLICY CMP0037)
  # Disallow reserved target names
  cmake_policy(SET CMP0037 NEW)
endif()

# This overrides the default flags for the different CMAKE_BUILD_TYPEs
set(CMAKE_USER_MAKE_RULES_OVERRIDE_C
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/c_flags_override.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cxx_flags_override.cmake")

project(FS-LINKER CXX C)

include(CheckFunctionExists)
include(CheckLibraryExists)

################################################################################
# Sanity check - Disallow building in source.
################################################################################
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "In source builds are not allowed. You should invoke "
          "CMake from a different directory.")
endif()

################################################################################
# Build type
################################################################################
message(STATUS "CMake generator: ${CMAKE_GENERATOR}")
if (DEFINED CMAKE_CONFIGURATION_TYPES)
  # Multi-configuration build (e.g. Xcode). Here
  # CMAKE_BUILD_TYPE doesn't matter
  message(STATUS "Available configurations: ${CMAKE_CONFIGURATION_TYPES}")
else()
  # Single configuration generator (e.g. Unix Makefiles, Ninja)
  set(available_build_types Debug Release RelWithDebInfo MinSizeRel)
  if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE is not set. Setting default")
    message(STATUS "The available build types are: ${available_build_types}")
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
        "Options are ${available_build_types}"
        FORCE)
    # Provide drop down menu options in cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${available_build_types})
  endif()
  message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

  # Check the selected build type is valid
  list(FIND available_build_types "${CMAKE_BUILD_TYPE}" _build_type_index)
  if ("${_build_type_index}" EQUAL "-1")
    message(FATAL_ERROR "\"${CMAKE_BUILD_TYPE}\" is an invalid build type.\n"
      "Use one of the following build types ${available_build_types}")
  endif()
endif()

# Reference specific library paths used during linking for install
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

################################################################################
# Compiler flags for LINKER components
# Subsequent commands will append to these. These are used instead of
# directly modifying CMAKE_CXX_FLAGS so that other code can be easily built with
# different flags.
################################################################################
set(LINKER_COMPONENT_EXTRA_INCLUDE_DIRS "")
set(LINKER_COMPONENT_CXX_DEFINES "")
set(LINKER_COMPONENT_CXX_FLAGS "")
set(LINKER_COMPONENT_EXTRA_LIBRARIES "")

################################################################################
# Include useful CMake functions
################################################################################
include(GNUInstallDirs)
include(CheckCXXSymbolExists)
include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckPrototypeDefinition)
include(CMakePushCheckState)
include("${CMAKE_SOURCE_DIR}/cmake/string_to_list.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/linker_component_add_cxx_flag.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/add_global_flag.cmake")


################################################################################
# Find LLVM
################################################################################
include(${CMAKE_SOURCE_DIR}/cmake/find_llvm.cmake)
set(NEEDED_LLVM_VARS
  LLVM_PACKAGE_VERSION
  LLVM_VERSION_MAJOR
  LLVM_VERSION_MINOR
  LLVM_VERSION_PATCH
  LLVM_DEFINITIONS
  LLVM_ENABLE_ASSERTIONS
  LLVM_ENABLE_EH
  LLVM_ENABLE_RTTI
  LLVM_INCLUDE_DIRS
  LLVM_LIBRARY_DIRS
  LLVM_TOOLS_BINARY_DIR
  LLVM_ENABLE_VISIBILITY_INLINES_HIDDEN
  TARGET_TRIPLE
)

if (${LLVM_VERSION_MAJOR} LESS 9)
  message(FATAL_ERROR "only support for LLVM >= 9.0.0")
endif()

foreach (vname ${NEEDED_LLVM_VARS})
  message(STATUS "${vname}: \"${${vname}}\"")
  if (NOT (DEFINED "${vname}"))
    message(FATAL_ERROR "${vname} was not defined")
  endif()
endforeach()

set(OPTIONAL_LLVM_VARS
  LLVM_BUILD_MAIN_SRC_DIR
)
foreach (vname ${OPTIONAL_LLVM_VARS})
  if (${vname})
    message(STATUS "${vname}: \"${${vname}}\"")
  endif()
endforeach()

if (LLVM_ENABLE_ASSERTIONS)
  # Certain LLVM debugging macros only work when LLVM was built with asserts
  set(ENABLE_LINKER_DEBUG 1) # for config.h
else()
  unset(ENABLE_LINKER_DEBUG) # for config.h
endif()


if (LLVM_ENABLE_VISIBILITY_INLINES_HIDDEN)
  list(APPEND LINKER_COMPONENT_CXX_FLAGS "-fvisibility-inlines-hidden")
endif()


list(APPEND LINKER_COMPONENT_CXX_DEFINES ${LLVM_DEFINITIONS})
list(APPEND LINKER_COMPONENT_EXTRA_INCLUDE_DIRS ${LLVM_INCLUDE_DIRS})

# Find llvm-link
set(LLVM_LINK "${LLVM_TOOLS_BINARY_DIR}/llvm-link")
if (NOT EXISTS "${LLVM_LINK}")
  message(FATAL_ERROR "Failed to find llvm-link at \"${LLVM_LINK}\"")
endif()

# Find llvm-ar
set(LLVM_AR "${LLVM_TOOLS_BINARY_DIR}/llvm-ar")
if (NOT EXISTS "${LLVM_AR}")
  message(FATAL_ERROR "Failed to find llvm-ar at \"${LLVM_AR}\"")
endif()

# Find llvm-as
set(LLVM_AS "${LLVM_TOOLS_BINARY_DIR}/llvm-as")
if (NOT EXISTS "${LLVM_AS}")
  message(FATAL_ERROR "Failed to find llvm-as at \"${LLVM_AS}\"")
endif()

################################################################################
# C++ version
################################################################################
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

################################################################################
# Warnings
################################################################################
include(${CMAKE_SOURCE_DIR}/cmake/compiler_warnings.cmake)

###############################################################################
# Exception handling
###############################################################################
if (NOT LLVM_ENABLE_EH)
  linker_component_add_cxx_flag("-fno-exceptions" REQUIRED)
endif()

###############################################################################
# RTTI
###############################################################################
if (NOT LLVM_ENABLE_RTTI)
  linker_component_add_cxx_flag("-fno-rtti" REQUIRED)
endif()

################################################################################
# Generate `config.h`
################################################################################
configure_file(${CMAKE_SOURCE_DIR}/include/fs-linker/Config/config.h.cmin
  ${CMAKE_BINARY_DIR}/include/fs-linker/Config/config.h)

################################################################################
# Global include directories
################################################################################
include_directories("${CMAKE_BINARY_DIR}/include")
include_directories("${CMAKE_SOURCE_DIR}/include")

################################################################################
# Set default location for targets in the build directory
################################################################################
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

################################################################################
# Report the value of various variables to aid debugging
################################################################################
message(STATUS "LINKER_COMPONENT_EXTRA_INCLUDE_DIRS: '${LINKER_COMPONENT_EXTRA_INCLUDE_DIRS}'")
message(STATUS "LINKER_COMPONENT_CXX_DEFINES: '${LINKER_COMPONENT_CXX_DEFINES}'")
message(STATUS "LINKER_COMPONENT_CXX_FLAGS: '${LINKER_COMPONENT_CXX_FLAGS}'")
message(STATUS "LINKER_COMPONENT_EXTRA_LIBRARIES: '${LINKER_COMPONENT_EXTRA_LIBRARIES}'")

################################################################################
# LINKER components
################################################################################
include("${CMAKE_SOURCE_DIR}/cmake/linker_add_component.cmake")

add_subdirectory(lib)

################################################################################
# FS-LINKER tools
################################################################################

add_executable(fs-linker
  main.cpp
)

set(LLVM_COMPONENTS
  core
  executionengine
  mcjit
  native
  support
)

linker_get_llvm_libs(LLVM_LIBS ${LLVM_COMPONENTS})
target_link_libraries(fs-linker PUBLIC ${LLVM_LIBS})
set(LINKER_LIBS
  linkerSupport
  linkerModule
)
target_link_libraries(fs-linker PUBLIC ${LINKER_LIBS})

install(TARGETS fs-linker RUNTIME DESTINATION bin)
