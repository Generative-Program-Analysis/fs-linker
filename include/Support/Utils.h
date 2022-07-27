//===-- Utils.h --------------------------------------------*- C++ -*-===//
//
//                     File System Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LINKER_UTILS_H
#define LINKER_UTILS_H

#include "Config/Version.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#ifdef ENABLE_LINKER_DEBUG
#define LINKER_DEBUG_WITH_TYPE(TYPE, X) DEBUG_WITH_TYPE(TYPE, X)
#else
#define LINKER_DEBUG_WITH_TYPE(TYPE, X) do { } while (0)
#endif
#define LINKER_DEBUG(X) LINKER_DEBUG_WITH_TYPE(DEBUG_TYPE, X)

namespace linker {

/// Print "LINKER: ERROR: " followed by the msg in printf format and a
/// newline on stderr and to warnings.txt, then exit with an error.
void linker_error(const char *msg, ...)
    __attribute__((format(printf, 1, 2), noreturn));

/// Print "LINKER: " followed by the msg in printf format and a
/// newline on stderr.
void linker_message(const char *msg, ...) __attribute__((format(printf, 1, 2)));

/// Print "LINKER: " followed by the msg in printf format and a
/// newline on stderr. However, the warning is only
/// printed once for each unique (id, msg) pair (as pointers).
void linker_message_once(const void *id, const char *msg, ...)
    __attribute__((format(printf, 2, 3)));

/// Hide all options in the specified category
void HideOptions(llvm::cl::OptionCategory &Category);

extern llvm::cl::OptionCategory ModuleCat;
} // End linker namespace

#endif /* LINKER_UTILS_H */
