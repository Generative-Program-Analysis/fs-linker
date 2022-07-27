//===-- LinkerDefines.h ----------------------------------------------------------===//
//
//                     File System Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
#ifndef LINKER_LINKER_DEFINES_H
#define LINKER_LINKER_DEFINES_H

#include "llvm/ADT/Twine.h"
#include "llvm/Support/raw_ostream.h"

#include <string>

namespace linker {
struct ModuleOptions {
  std::string EntryPoint;
  bool Optimize;

  ModuleOptions(const std::string &_EntryPoint,
                bool _Optimize)
      : EntryPoint(_EntryPoint), Optimize(_Optimize) {}
};
} // End linker namespace

#endif /* LINKER_LINKER_DEFINES_H */