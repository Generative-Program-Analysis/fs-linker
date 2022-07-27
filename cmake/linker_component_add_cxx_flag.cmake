#===------------------------------------------------------------------------===#
#
#                     File System Linker
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
include(CheckCXXCompilerFlag)
include(CMakeParseArguments)

function(linker_component_add_cxx_flag flag)
  CMAKE_PARSE_ARGUMENTS(linker_component_add_cxx_flag "REQUIRED" "" "" ${ARGN})
  string(REPLACE "-" "_" SANITIZED_FLAG_NAME "${flag}")
  string(REPLACE "/" "_" SANITIZED_FLAG_NAME "${SANITIZED_FLAG_NAME}")
  string(REPLACE "=" "_" SANITIZED_FLAG_NAME "${SANITIZED_FLAG_NAME}")
  string(REPLACE " " "_" SANITIZED_FLAG_NAME "${SANITIZED_FLAG_NAME}")
  string(REPLACE "+" "_" SANITIZED_FLAG_NAME "${SANITIZED_FLAG_NAME}")
  unset(HAS_${SANITIZED_FLAG_NAME})
  CHECK_CXX_COMPILER_FLAG("${flag}" HAS_${SANITIZED_FLAG_NAME})
  if (linker_component_add_cxx_flag_REQUIRED AND NOT HAS_${SANITIZED_FLAG_NAME})
    message(FATAL_ERROR "The flag \"${flag}\" is required but your C++ compiler doesn't support it")
  endif()
  if (HAS_${SANITIZED_FLAG_NAME})
    message(STATUS "C++ compiler supports ${flag}")
    list(APPEND LINKER_COMPONENT_CXX_FLAGS "${flag}")
    set(LINKER_COMPONENT_CXX_FLAGS ${LINKER_COMPONENT_CXX_FLAGS} PARENT_SCOPE)
  else()
    message(STATUS "C++ compiler does not support ${flag}")
  endif()
endfunction()
