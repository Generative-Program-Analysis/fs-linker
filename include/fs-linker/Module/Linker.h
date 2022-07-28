//===-- Linker.h ----------------------------------------------------------===//
//
//                     File System Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//

#ifndef LINKER_LINKER_H
#define LINKER_LINKER_H

#include "fs-linker/Module/LModule.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace linker {
class Linker {
private:
  std::unique_ptr<LModule> lmodule;
public:
  Linker() {}
  ~Linker() {}
  llvm::Module *setModule(std::vector<std::unique_ptr<llvm::Module>> &modules,
                          const ModuleOptions &opts);
};
} // End linker namespace

#endif /* LINKER_LINKER_H */
