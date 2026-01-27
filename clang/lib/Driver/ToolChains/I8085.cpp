//===--- I8085.cpp - I8085 ToolChain ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "I8085.h"
#include "CommonArgs.h"
#include "clang/Driver/Options.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/VirtualFileSystem.h"

using namespace clang;
using namespace clang::driver;
using namespace clang::driver::toolchains;
using tools::addPathIfExists;

static std::string computeI8085SysRoot(const Driver &D) {
  if (!D.SysRoot.empty())
    return D.SysRoot;
  SmallString<128> Dir(D.ResourceDir);
  llvm::sys::path::append(Dir, "i8085");
  return std::string(Dir);
}

I8085ToolChain::I8085ToolChain(const Driver &D, const llvm::Triple &Triple,
                               const llvm::opt::ArgList &Args)
    : Generic_ELF(D, Triple, Args) {
  DefaultSysRoot = computeI8085SysRoot(D);

  std::string Script = GetFilePath("i8085.ld");
  if (getVFS().exists(Script))
    DefaultLinkerScriptArg = "-T" + Script;

  SmallString<128> LibPath(DefaultSysRoot);
  llvm::sys::path::append(LibPath, "lib");
  addPathIfExists(D, LibPath, getFilePaths());
}

void I8085ToolChain::AddClangSystemIncludeArgs(
    const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc) ||
      DriverArgs.hasArg(options::OPT_nostdlibinc))
    return;

  SmallString<128> IncludePath(DefaultSysRoot);
  llvm::sys::path::append(IncludePath, "include");
  addSystemInclude(DriverArgs, CC1Args, IncludePath);
}

Tool *I8085ToolChain::buildLinker() const {
  return new tools::gnutools::Linker(*this);
}

void I8085ToolChain::addExtraOpts(llvm::opt::ArgStringList &CmdArgs) const {
  if (!DefaultLinkerScriptArg.empty())
    CmdArgs.push_back(DefaultLinkerScriptArg.c_str());
}
