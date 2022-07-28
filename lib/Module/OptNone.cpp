//===-- OptNone.cpp -------------------------------------------------------===//
//
//                     File System Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Passes.h"

#include "fs-linker/Config/Version.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"

namespace linker {

char OptNonePass::ID;

bool OptNonePass::runOnModule(llvm::Module &M) {
  // todo: modify this if we change external prefix
  // Find list of functions that start with `klee_` or `llsc_` or `make_symbolic`
  // and mark all functions that contain such call or invoke as optnone
  llvm::SmallPtrSet<llvm::Function *,16> CallingFunctions;
  for (auto &F : M) {
    if (!F.hasName() || !(F.getName().startswith("klee_") || F.getName().startswith("llsc_") || F.getName().startswith("make_symbolic")))
      continue;
    for (auto *U : F.users()) {
      // skip non-calls and non-invokes
      if (!llvm::isa<llvm::CallInst>(U) && !llvm::isa<llvm::InvokeInst>(U))
        continue;
      auto *Inst = llvm::cast<llvm::Instruction>(U);
      CallingFunctions.insert(Inst->getParent()->getParent());
    }
  }

  bool changed = false;
  for (auto F : CallingFunctions) {
    // Skip if already annotated
    if (F->hasFnAttribute(llvm::Attribute::OptimizeNone))
      continue;
    F->addFnAttr(llvm::Attribute::OptimizeNone);
    F->addFnAttr(llvm::Attribute::NoInline);
    changed = true;
  }

  return changed;
}
} // namespace linker
