/* -*- mode: c++; c-basic-offset: 2; -*- */

//===-- main.cpp ------------------------------------------------*- C++ -*-===//
//
//                     File System Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "fs-linker/Config/Version.h"
#include "fs-linker/Support/Utils.h"
#include "fs-linker/Module/LinkerModule.h"

#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Errno.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Signals.h"


#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <cerrno>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>


using namespace llvm;
using namespace linker;

namespace {
  cl::opt<std::string>
  InputFile(cl::desc("<input bytecode>"), cl::Positional, cl::init("-"));

  /*** Startup options ***/

  cl::OptionCategory StartCat("Startup options",
                              "These options affect how execution is started.");

  cl::opt<std::string>
  EntryPoint("entry-point",
             cl::desc("Function in which to start execution (default=main)"),
             cl::init("main"),
             cl::cat(StartCat));

  cl::opt<std::string>
  OutputDir("output-dir",
            cl::desc("Directory in which to write results (default=${PWD})"),
            cl::init(""),
            cl::cat(StartCat));

  cl::opt<std::string>
  OutputFilename("o",
                 cl::desc("Specify output filename"),
                 cl::value_desc("filename"),
                 cl::init(""),
                 cl::cat(StartCat));

  cl::opt<bool>
  OptimizeModule("optimize",
                 cl::desc("Optimize the code before execution (default=false)."),
		 cl::init(false),
                 cl::cat(StartCat));


  /*** Linking options ***/

  cl::OptionCategory LinkCat("Linking options",
                             "These options control the libraries being linked.");

  cl::opt<std::string> PosixPath("posix-path",
           cl::desc("Link the specified POSIX runtime to the input program."),
           cl::value_desc("path to your POSIX runtime archive"),
           cl::init(""),
           cl::cat(LinkCat));

  cl::opt<std::string> UclibcPath("uclibc-path",
           cl::desc("Link the specified uClibc library to the input program."),
           cl::value_desc("path to your uClibc library archive"),
           cl::init(""),
           cl::cat(LinkCat));

  cl::list<std::string>
      LinkLibraries("link-llvm-lib",
                    cl::desc("Link the given bitcode library, "
                             "e.g. .bca, .bc, .a. Can be used multiple times."),
                    cl::value_desc("bitcode library file"), cl::cat(LinkCat));

}

/***/

class OutputMgr {
private:
  SmallString<128> m_outputDirectory;
  std::unique_ptr<llvm::raw_fd_ostream> open_file(const std::string &path, std::string &error);

public:
  OutputMgr();
  ~OutputMgr() {}

  std::string getOutputFilename(const std::string &filename);
  std::unique_ptr<llvm::raw_fd_ostream> openOutputFile(const std::string &filename);
  std::unique_ptr<llvm::raw_fd_ostream> openOutputIR();
};

std::unique_ptr<llvm::raw_fd_ostream>
OutputMgr::open_file(const std::string &path, std::string &error) {
  error.clear();
  std::error_code ec;

#if LLVM_VERSION_CODE >= LLVM_VERSION(7, 0)
  auto f = std::make_unique<llvm::raw_fd_ostream>(path.c_str(), ec,
                                                  llvm::sys::fs::OF_None);
#else
  auto f = std::make_unique<llvm::raw_fd_ostream>(path.c_str(), ec,
                                                  llvm::sys::fs::F_None);
#endif

  if (ec)
    error = ec.message();
  if (!error.empty()) {
    f.reset(nullptr);
  }
  return f;
}

OutputMgr::OutputMgr()
    : m_outputDirectory() {

  // create output directory (OutputDir or "${PWD}")
  bool dir_given = OutputDir != "";
  SmallString<128> directory;
  SmallString<128> current_dir;
  if (auto ec = sys::fs::current_path	(current_dir))
    linker_error("unable to get current path: %s", ec.message().c_str());

  if (dir_given)
    directory = OutputDir;
  else
    directory = current_dir;

  if (auto ec = sys::fs::make_absolute(directory)) {
    linker_error("unable to determine absolute path: %s", ec.message().c_str());
  }

  m_outputDirectory = directory;

  if (dir_given) {
    // OutputDir
    if (mkdir(m_outputDirectory.c_str(), 0775) < 0)
      linker_error("cannot create \"%s\": %s", directory.c_str(), strerror(errno));
  }

  linker_message("output directory is \"%s\"", m_outputDirectory.c_str());
}

std::string OutputMgr::getOutputFilename(const std::string &filename) {
  SmallString<128> path = m_outputDirectory;
  sys::path::append(path,filename);
  return path.c_str();
}

std::unique_ptr<llvm::raw_fd_ostream>
OutputMgr::openOutputFile(const std::string &filename) {
  std::string Error;
  std::string path = getOutputFilename(filename);
  auto f = open_file(path, Error);
  if (!f) {
    linker_message("error opening file \"%s\".  Linker may have run out of file "
                   "descriptors: try to increase the maximum number of open file "
                   "descriptors by using ulimit (%s).",
                   path.c_str(), Error.c_str());
    return nullptr;
  }
  return f;
}

std::unique_ptr<llvm::raw_fd_ostream>
OutputMgr::openOutputIR() {
  bool file_given = OutputFilename != "";
  llvm::StringRef output_ir = file_given ? sys::path::filename(OutputFilename) : "assembly.ll";
  return openOutputFile(std::string(output_ir));
}

//===----------------------------------------------------------------------===//
// main Driver function
//

static void parseArguments(int argc, char **argv) {
  // This version always reads response files
  cl::ParseCommandLineOptions(argc, argv, " linker\n");
}

static void
preparePOSIX(std::vector<std::unique_ptr<llvm::Module>> &loadedModules,
             llvm::StringRef libCPrefix) {
  // Get the main function from the main module and rename it such that it can
  // be called after the POSIX setup
  Function *mainFn = nullptr;
  for (auto &module : loadedModules) {
    mainFn = module->getFunction(EntryPoint);
    if (mainFn)
      break;
  }

  if (!mainFn)
    linker_error("Entry function '%s' not found in module.", EntryPoint.c_str());
  mainFn->setName("__klee_posix_wrapped_main");

  // Add a definition of the entry function if needed. This is the case if we
  // link against a libc implementation. Preparing for libc linking (i.e.
  // linking with uClibc will expect a main function and rename it to
  // _user_main. We just provide the definition here.
  if (!libCPrefix.empty() && !mainFn->getParent()->getFunction(EntryPoint))
    llvm::Function::Create(mainFn->getFunctionType(),
                           llvm::Function::ExternalLinkage, EntryPoint,
                           mainFn->getParent());

  llvm::Function *wrapper = nullptr;
  for (auto &module : loadedModules) {
    wrapper = module->getFunction("__klee_posix_wrapper");
    if (wrapper)
      break;
  }
  assert(wrapper && "klee_posix_wrapper not found");

  // Rename the POSIX wrapper to prefixed entrypoint, e.g. _user_main as uClibc
  // would expect it or main otherwise
  wrapper->setName(libCPrefix + EntryPoint);
}

// Symbols we explicitly support
static const char *modelledExternals[] = {
  "__assert_rtn",
  "__assert_fail",
  "__assert",
  "_assert",
  "abort",
  "_exit",
  "exit",
  "calloc",
  "free",
  "__errno_location",
  "malloc",
  "memalign",
  "realloc",
  "__ubsan_handle_add_overflow",
  "__ubsan_handle_sub_overflow",
  "__ubsan_handle_mul_overflow",
  "__ubsan_handle_divrem_overflow",
};

// Symbols we consider unsafe
static const char *unsafeExternals[] = {
  "fork", // oh lord
  "exec", // heaven help us
  "error", // calls _exit
  "raise", // yeah
  "kill", // mmmhmmm
};

#define NELEMS(array) (sizeof(array)/sizeof(array[0]))
void externalsAndGlobalsCheck(const llvm::Module *m) {
  std::map<std::string, bool> externals;
  std::set<std::string> modelled(modelledExternals,
                                 modelledExternals+NELEMS(modelledExternals));
  std::set<std::string> unsafe(unsafeExternals,
                               unsafeExternals+NELEMS(unsafeExternals));

  for (Module::const_iterator fnIt = m->begin(), fn_ie = m->end();
       fnIt != fn_ie; ++fnIt) {
    if (fnIt->isDeclaration() && !fnIt->use_empty())
      externals.insert(std::make_pair(fnIt->getName(), false));
    for (Function::const_iterator bbIt = fnIt->begin(), bb_ie = fnIt->end();
         bbIt != bb_ie; ++bbIt) {
      for (BasicBlock::const_iterator it = bbIt->begin(), ie = bbIt->end();
           it != ie; ++it) {
        if (const CallInst *ci = dyn_cast<CallInst>(it)) {
#if LLVM_VERSION_CODE >= LLVM_VERSION(8, 0)
          if (isa<InlineAsm>(ci->getCalledOperand())) {
#else
          if (isa<InlineAsm>(ci->getCalledValue())) {
#endif
            linker_message_once(&*fnIt, "function \"%s\" has inline asm", fnIt->getName().data());
          }
        }
      }
    }
  }

  for (Module::const_global_iterator
         it = m->global_begin(), ie = m->global_end();
       it != ie; ++it)
    if (it->isDeclaration() && !it->use_empty())
      externals.insert(std::make_pair(it->getName(), true));
  // and remove aliases (they define the symbol after global
  // initialization)
  for (Module::const_alias_iterator
         it = m->alias_begin(), ie = m->alias_end();
       it != ie; ++it) {
    std::map<std::string, bool>::iterator it2 =
        externals.find(it->getName().str());
    if (it2!=externals.end())
      externals.erase(it2);
  }

  std::map<std::string, bool> foundUnsafe;
  for (std::map<std::string, bool>::iterator
         it = externals.begin(), ie = externals.end();
       it != ie; ++it) {
    const std::string &ext = it->first;
    if (!modelled.count(ext) && !((ext.rfind("klee_", 0) == 0) || (ext.rfind("gs_", 0) == 0) || (ext.rfind("make_symbolic", 0) == 0))) {
      if (ext.compare(0, 5, "llvm.") != 0) { // not an LLVM reserved name
        if (unsafe.count(ext)) {
          foundUnsafe.insert(*it);
        } else {
          linker_message("undefined reference to %s: %s",
                       it->second ? "variable" : "function",
                       ext.c_str());
        }
      }
    }
  }

  for (std::map<std::string, bool>::iterator
         it = foundUnsafe.begin(), ie = foundUnsafe.end();
       it != ie; ++it) {
    const std::string &ext = it->first;
    linker_message("undefined reference to %s: %s (UNSAFE)!",
                 it->second ? "variable" : "function",
                 ext.c_str());
  }
}

static void replaceOrRenameFunction(llvm::Module *module,
		const char *old_name, const char *new_name)
{
  Function *new_function, *old_function;
  new_function = module->getFunction(new_name);
  old_function = module->getFunction(old_name);
  if (old_function) {
    if (new_function) {
      old_function->replaceAllUsesWith(new_function);
      old_function->eraseFromParent();
    } else {
      old_function->setName(new_name);
      assert(old_function->getName() == new_name);
    }
  }
}

static void
createLibCWrapper(std::vector<std::unique_ptr<llvm::Module>> &modules,
                  llvm::StringRef intendedFunction,
                  llvm::StringRef libcMainFunction) {
  // XXX we need to rearchitect so this can also be used with
  // programs externally linked with libc implementation.

  // We now need to swap things so that libcMainFunction is the entry
  // point, in such a way that the arguments are passed to
  // libcMainFunction correctly. We do this by renaming the user main
  // and generating a stub function to call intendedFunction. There is
  // also an implicit cooperation in that runFunctionAsMain sets up
  // the environment arguments to what a libc expects (following
  // argv), since it does not explicitly take an envp argument.
  auto &ctx = modules[0]->getContext();
  Function *userMainFn = modules[0]->getFunction(intendedFunction);
  assert(userMainFn && "unable to get user main");
  // Rename entry point using a prefix
  userMainFn->setName("__user_" + intendedFunction);

  // force import of libcMainFunction
  llvm::Function *libcMainFn = nullptr;
  for (auto &module : modules) {
    if ((libcMainFn = module->getFunction(libcMainFunction)))
      break;
  }
  if (!libcMainFn)
    linker_error("Could not add %s wrapper", libcMainFunction.str().c_str());

  auto inModuleReference = libcMainFn->getParent()->getOrInsertFunction(
      userMainFn->getName(), userMainFn->getFunctionType());

  const auto ft = libcMainFn->getFunctionType();

  if (ft->getNumParams() != 7)
    linker_error("Imported %s wrapper does not have the correct "
               "number of arguments",
               libcMainFunction.str().c_str());

  std::vector<Type *> fArgs;
  fArgs.push_back(ft->getParamType(1)); // argc
  fArgs.push_back(ft->getParamType(2)); // argv
  Function *stub =
      Function::Create(FunctionType::get(Type::getInt32Ty(ctx), fArgs, false),
                       GlobalVariable::ExternalLinkage, intendedFunction,
                       libcMainFn->getParent());
  BasicBlock *bb = BasicBlock::Create(ctx, "entry", stub);
  llvm::IRBuilder<> Builder(bb);

  std::vector<llvm::Value*> args;
  args.push_back(llvm::ConstantExpr::getBitCast(
#if LLVM_VERSION_CODE >= LLVM_VERSION(9, 0)
      cast<llvm::Constant>(inModuleReference.getCallee()),
#else
      inModuleReference,
#endif
      ft->getParamType(0)));
  args.push_back(&*(stub->arg_begin())); // argc
  auto arg_it = stub->arg_begin();
  arg_it->setName("argc");
  args.push_back(&*(++arg_it)); // argv
  arg_it->setName("argv");
  args.push_back(Constant::getNullValue(ft->getParamType(3))); // app_init
  args.push_back(Constant::getNullValue(ft->getParamType(4))); // app_fini
  args.push_back(Constant::getNullValue(ft->getParamType(5))); // rtld_fini
  args.push_back(Constant::getNullValue(ft->getParamType(6))); // stack_end
  Builder.CreateCall(libcMainFn, args);
  Builder.CreateUnreachable();
}

static void
linkWithUclibc(StringRef libPath, std::vector<std::unique_ptr<llvm::Module>> &modules) {
  LLVMContext &ctx = modules[0]->getContext();

  size_t newModules = modules.size();

  // Ensure that klee-uclibc exists
  SmallString<128> uclibcBCA(libPath);
  std::string errorMsg;
  if (!linker::loadFile(uclibcBCA.c_str(), ctx, modules, errorMsg))
    linker_error("Cannot find uclibc '%s': %s", uclibcBCA.c_str(),
               errorMsg.c_str());

  for (auto i = newModules, j = modules.size(); i < j; ++i) {
    replaceOrRenameFunction(modules[i].get(), "__libc_open", "open");
    replaceOrRenameFunction(modules[i].get(), "__libc_fcntl", "fcntl");
  }

  createLibCWrapper(modules, EntryPoint, "__uClibc_main");
  linker_message("NOTE: Using uclibc : %s", uclibcBCA.c_str());

  // Todo: Link the fortified library
}

int main(int argc, char **argv, char **envp) {
  atexit(llvm_shutdown);  // Call llvm_shutdown() on exit.

#if LLVM_VERSION_CODE >= LLVM_VERSION(13, 0)
  linker::HideOptions(llvm::cl::getGeneralCategory());
#else
  linker::HideOptions(llvm::cl::GeneralCategory);
#endif

  llvm::InitializeNativeTarget();

  parseArguments(argc, argv);
  sys::PrintStackTraceOnErrorSignal(argv[0]);

  // Load the bytecode...
  std::string errorMsg;
  LLVMContext ctx;
  std::vector<std::unique_ptr<llvm::Module>> loadedModules;
  if (!linker::loadFile(InputFile, ctx, loadedModules, errorMsg)) {
    linker_error("error loading program '%s': %s", InputFile.c_str(),
               errorMsg.c_str());
  }
  // Load and link the whole files content. The assumption is that this is the
  // application under test.
  // Nothing gets removed in the first place.
  std::unique_ptr<llvm::Module> M(linker::linkModules(
      loadedModules, "" /* link all modules together */, errorMsg));
  if (!M) {
    linker_error("error loading program '%s': %s", InputFile.c_str(),
               errorMsg.c_str());
  }

  llvm::Module *mainModule = M.get();

  const std::string &module_triple = mainModule->getTargetTriple();
  std::string host_triple = llvm::sys::getDefaultTargetTriple();

  if (module_triple != host_triple)
    linker_message("Module and host target triples do not match: '%s' != '%s'\n"
                 "This may cause unexpected crashes or assertion violations.",
                 module_triple.c_str(), host_triple.c_str());

  // Detect architecture
  // Only support 64bit target
  if (module_triple.find("i686") != std::string::npos ||
      module_triple.find("i586") != std::string::npos ||
      module_triple.find("i486") != std::string::npos ||
      module_triple.find("i386") != std::string::npos) {
    linker_error("only accept 64bit architecture but module is %s", module_triple.c_str());
  }

  // Push the module as the first entry
  loadedModules.emplace_back(std::move(M));

  // Todo: get runtime library path
  linker::ModuleOptions Opts(EntryPoint, /*Optimize=*/OptimizeModule);

  bool link_with_uclibc = (UclibcPath != "");
  // if (!link_with_uclibc)
  //   linker_error("must link with uClibc library");

  if (PosixPath != "") {
    SmallString<128> Path(PosixPath);
    linker_message("NOTE: Using POSIX model: %s", Path.c_str());
    if (!linker::loadFile(Path.c_str(), mainModule->getContext(), loadedModules,
                        errorMsg))
      linker_error("error loading POSIX support '%s': %s", Path.c_str(),
                 errorMsg.c_str());

    std::string libcPrefix = (link_with_uclibc ? "__user_" : "");
    preparePOSIX(loadedModules, libcPrefix);
  }


  if (link_with_uclibc)
    linkWithUclibc(UclibcPath, loadedModules);

  for (const auto &library : LinkLibraries) {
    if (!linker::loadFile(library, mainModule->getContext(), loadedModules,
                        errorMsg))
      linker_error("error loading bitcode library '%s': %s", library.c_str(),
                 errorMsg.c_str());
  }

  OutputMgr *outputmgr = new OutputMgr();
  Linker *m_linker = new Linker();
  assert(m_linker);

  // Get the desired main function.  user's main initializes uClibc
  // locale and other data and then calls main.

  auto finalModule = m_linker->setModule(loadedModules, Opts);
  Function *mainFn = finalModule->getFunction(EntryPoint);
  if (!mainFn) {
    linker_error("Entry function '%s' not found in module.", EntryPoint.c_str());
  }

  externalsAndGlobalsCheck(finalModule);

  // Output IR code
  std::unique_ptr<llvm::raw_fd_ostream> output_ll(outputmgr->openOutputIR());
  assert(output_ll && !output_ll->has_error() && "unable to open source output");
  *output_ll << *finalModule;

  delete outputmgr;
  delete m_linker;

  return 0;
}
