//===-- Linker.cpp ------------------------------------------------------===//
//
//                     File System Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "Module/Linker.h"
#include "Module/LModule.h"
#include "Support/Utils.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#if LLVM_VERSION_CODE < LLVM_VERSION(8, 0)
#include "llvm/IR/CallSite.h"
#endif
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#if LLVM_VERSION_CODE >= LLVM_VERSION(10, 0)
#include "llvm/Support/TypeSize.h"
#else
typedef unsigned TypeSize;
#endif
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <cxxabi.h>
#include <fstream>
#include <iomanip>
#include <iosfwd>
#include <limits>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <vector>

using namespace llvm;
using namespace linker;

namespace linker {
struct ExternalInfo {
  const char *name;
  bool doesNotReturn; /// External terminates the process
  bool hasReturnValue; /// External has a return value
  bool doNotOverride; /// External should not be used if already defined
};

static ExternalInfo externalInfo[] = {
#define add(name, ret) { name, false, ret, false }
#define addDNR(name) { name, true, false, false }
  addDNR("__assert_rtn"),
  addDNR("__assert_fail"),
  addDNR("__assert"),
  addDNR("_assert"),
  addDNR("abort"),
  addDNR("_exit"),
  { "exit", true, false, true },
  add("calloc", true),
  add("free", false),
  add("__errno_location", true),
  add("malloc", true),
  add("memalign", true),
  add("realloc", true),

  // Run clang with -fsanitize=signed-integer-overflow and/or
  // -fsanitize=unsigned-integer-overflow
  add("__ubsan_handle_add_overflow", false),
  add("__ubsan_handle_sub_overflow", false),
  add("__ubsan_handle_mul_overflow", false),
  add("__ubsan_handle_divrem_overflow", false),

#undef addDNR
#undef add
};

static void prepareExternalFunc(std::unique_ptr<LModule> &lmodule, std::vector<const char *> &preservedFunctions) {
  unsigned N = sizeof(externalInfo)/sizeof(externalInfo[0]);

  for (unsigned i=0; i<N; ++i) {
    ExternalInfo &ei = externalInfo[i];
    Function *f = lmodule->module->getFunction(ei.name);

    // No need to create if the function doesn't exist, since it cannot
    // be called in that case.
    if (f && (!ei.doNotOverride || f->isDeclaration())) {
      preservedFunctions.push_back(ei.name);
      // Make sure NoReturn attribute is set, for optimization and
      // coverage counting.
      if (ei.doesNotReturn)
        f->addFnAttr(Attribute::NoReturn);

      // Change to a declaration since we handle internally (simplifies
      // module and allows deleting dead code).
      if (!f->isDeclaration())
        f->deleteBody();
    }
  }
}

llvm::Module *
Linker::setModule(std::vector<std::unique_ptr<llvm::Module>> &modules,
                  const ModuleOptions &opts) {

  assert(!lmodule && !modules.empty() &&
         "can only register one module"); // XXX gross

  lmodule = std::unique_ptr<LModule>(new LModule());

  // Preparing the final module happens in multiple stages

  // Todo: Link with KLEE/LLSC intrinsics library before running any optimizations

  // 1.) Link the modules together
  while (lmodule->link(modules, opts.EntryPoint)) {
    // 2.) Apply different instrumentation
    lmodule->instrument(opts);
  }

  // 3.) Optimise and prepare for Engine

  // Create a list of functions that should be preserved if used
  std::vector<const char *> preservedFunctions;
  prepareExternalFunc(lmodule, preservedFunctions);

  preservedFunctions.push_back(opts.EntryPoint.c_str());

  // Preserve the free-standing library calls
  preservedFunctions.push_back("memset");
  preservedFunctions.push_back("memcpy");
  preservedFunctions.push_back("memcmp");
  preservedFunctions.push_back("memmove");

  lmodule->optimiseAndPrepare(opts, preservedFunctions);
  lmodule->checkModule();

  return lmodule->module.get();
}

} // End linker namespace