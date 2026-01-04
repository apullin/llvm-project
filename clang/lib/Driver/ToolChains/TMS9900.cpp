//===--- TMS9900.cpp - TMS9900 Helpers for Tools ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TMS9900.h"
#include "CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Options.h"
#include "llvm/Option/ArgList.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang;
using namespace llvm::opt;

/// TMS9900 Toolchain
TMS9900ToolChain::TMS9900ToolChain(const Driver &D, const llvm::Triple &Triple,
                                   const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {
  // TMS9900 is a bare-metal target with no standard library paths by default
}

void TMS9900ToolChain::addClangTargetOptions(const ArgList &DriverArgs,
                                             ArgStringList &CC1Args,
                                             Action::OffloadKind) const {
  // TMS9900 cannot support tail call optimization safely because almost all
  // instructions (including MOV) set the status register. This causes issues
  // when LLVM's tail recursion elimination creates loops with phi-node copies
  // that get placed between compare and branch instructions, clobbering the
  // status flags. Disable tail call optimization unless the user explicitly
  // enables it.
  if (!DriverArgs.hasArg(options::OPT_foptimize_sibling_calls))
    CC1Args.push_back("-fno-optimize-sibling-calls");
}
