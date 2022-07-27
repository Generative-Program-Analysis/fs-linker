#===------------------------------------------------------------------------===#
#
#                     File System Linker
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
linker_add_component(linkerSupport STATIC Utils.cpp)

set(LLVM_COMPONENTS
  support
)

linker_get_llvm_libs(LLVM_LIBS ${LLVM_COMPONENTS})
target_link_libraries(linkerSupport PUBLIC ${LLVM_LIBS})
