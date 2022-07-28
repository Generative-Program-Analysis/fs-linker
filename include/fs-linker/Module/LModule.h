//===-- LModule.h -----------------------------------------------*- C++ -*-===//
//
//                     File System Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LINKER_LMODULE_H
#define LINKER_LMODULE_H

#include "fs-linker/Config/Version.h"
#include "fs-linker/Module/LinkerDefines.h"

#include "llvm/ADT/ArrayRef.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

namespace llvm {
  class BasicBlock;
  class Constant;
  class Function;
  class Instruction;
  class Module;
  class DataLayout;
}

namespace linker {
  class Linker;

  class LModule {
  public:
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::DataLayout> targetData;

    // Our shadow versions of LLVM structures.
    std::vector<std::unique_ptr<llvm::Function>> functions;

    // Todo: record escapingFunctions here.

    std::vector<llvm::Constant*> constants;

    // Functions which are part of runtime
    std::set<const llvm::Function*> internalFunctions;

  public:
    LModule() = default;

    /// Optimise and prepare module such that it can be executed
    //
    void optimiseAndPrepare(const linker::ModuleOptions &opts,
                            llvm::ArrayRef<const char *>);

    /// Link the provided modules together as one module.
    ///
    /// If the entry point is empty, all modules are linked together.
    /// If the entry point is not empty, all modules are linked which resolve
    /// the dependencies of the module containing entryPoint
    ///
    /// @param modules list of modules to be linked together
    /// @param entryPoint name of the function which acts as the program's entry
    /// point
    /// @return true if at least one module has been linked in, false if nothing
    /// changed
    bool link(std::vector<std::unique_ptr<llvm::Module>> &modules,
              const std::string &entryPoint);

    void instrument(const linker::ModuleOptions &opts);

    /// Run passes that check if module is valid LLVM IR and if invariants
    /// expected by Linker hold.
    void checkModule();
  };
} // End linker namespace

#endif /* LINKER_LMODULE_H */