//===-- Utils.cpp ----------------------------------------------------===//
//
//                     File System Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "fs-linker/Support/Utils.h"

#include "fs-linker/Config/Version.h"

#include "llvm/ADT/StringRef.h"


#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>

using namespace llvm;
using namespace linker;


void linker::linker_error(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  fprintf(stderr, "LINKER: ERROR: ");
  vfprintf(stderr, msg, ap);
  fprintf(stderr, "\n");
  fflush(stderr);
  va_end(ap);
  exit(1);
}

void linker::linker_message(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  fprintf(stderr, "LINKER: ");
  vfprintf(stderr, msg, ap);
  fprintf(stderr, "\n");
  fflush(stderr);
  va_end(ap);
}

/* Prints a warning once per message. */
void linker::linker_message_once(const void *id, const char *msg, ...) {
  static std::set<std::pair<const void *, const char *> > keys;
  std::pair<const void *, const char *> key;

  /* "calling external" messages contain the actual arguments with
     which we called the external function, so we need to ignore them
     when computing the key. */
  if (strncmp(msg, "calling external", strlen("calling external")) != 0)
    key = std::make_pair(id, msg);
  else
    key = std::make_pair(id, "calling external");

  if (!keys.count(key)) {
    keys.insert(key);
    va_list ap;
    va_start(ap, msg);
    fprintf(stderr, "LINKER: WARNING ONCE: ");
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(ap);
  }
}

void linker::HideOptions(llvm::cl::OptionCategory &Category) {
  StringMap<cl::Option *> &map = cl::getRegisteredOptions();

  for (auto &elem : map) {
#if LLVM_VERSION_CODE >= LLVM_VERSION(9, 0)
    for (auto &cat : elem.second->Categories) {
#else
    {
      auto &cat = elem.second->Category;
#endif
      if (cat == &Category) {
        elem.second->setHiddenFlag(cl::Hidden);
      }
    }
  }
}