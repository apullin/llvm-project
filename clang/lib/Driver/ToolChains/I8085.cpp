//===--- I8085.cpp - I8085 ToolChain ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "I8085.h"
#include "llvm/Support/VirtualFileSystem.h"

using namespace clang;
using namespace clang::driver;
using namespace clang::driver::toolchains;

I8085ToolChain::I8085ToolChain(const Driver &D, const llvm::Triple &Triple,
                               const llvm::opt::ArgList &Args)
    : Generic_ELF(D, Triple, Args) {
  std::string Script = GetFilePath("i8085.ld");
  if (getVFS().exists(Script))
    DefaultLinkerScriptArg = "-T" + Script;
}

Tool *I8085ToolChain::buildLinker() const {
  return new tools::gnutools::Linker(*this);
}

void I8085ToolChain::addExtraOpts(llvm::opt::ArgStringList &CmdArgs) const {
  if (!DefaultLinkerScriptArg.empty())
    CmdArgs.push_back(DefaultLinkerScriptArg.c_str());
}
