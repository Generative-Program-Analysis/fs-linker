#===------------------------------------------------------------------------===#
#
#                     File System Linker
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
set(LINKER_MODULE_COMPONENT_SRCS
  FunctionAlias.cpp
  ModuleUtil.cpp
  InstructionOperandTypeCheckPass.cpp
  IntrinsicCleaner.cpp
  Linker.cpp
  LModule.cpp
  LowerSwitch.cpp
  ModuleUtil.cpp
  Optimize.cpp
  OptNone.cpp
  OptNone.cpp
  PhiCleaner.cpp
  RaiseAsm.cpp
)

linker_add_component(linkerModule
  ${LINKER_MODULE_COMPONENT_SRCS}
)

set(LLVM_COMPONENTS
  bitreader
  bitwriter
  codegen
  ipo
  irreader
  linker
  support
)

linker_get_llvm_libs(LLVM_LIBS ${LLVM_COMPONENTS})
target_link_libraries(linkerModule PUBLIC ${LLVM_LIBS})
target_link_libraries(linkerModule PRIVATE
  linkerSupport
)
