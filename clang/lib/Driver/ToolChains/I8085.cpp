//===--- I8085.cpp - I8085 ToolChain ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "I8085.h"
#include "CommonArgs.h"
#include "clang/Driver/DriverDiagnostic.h"
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

void I8085ToolChain::addClangTargetOptions(
    const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args, Action::OffloadKind Kind) const {
  Generic_ELF::addClangTargetOptions(DriverArgs, CC1Args, Kind);

  // Use very conservative inlining threshold for this 8-bit target.
  // Default (225) causes massive code bloat due to aggressive inlining.
  // Functions with loops (like software multiply) should stay as calls.
  CC1Args.push_back("-mllvm");
  CC1Args.push_back("-inline-threshold=30");

  // Also limit unrolling to avoid code explosion.
  CC1Args.push_back("-mllvm");
  CC1Args.push_back("-unroll-threshold=50");

  const llvm::Triple &Triple = getTriple();
  if (const auto *A = DriverArgs.getLastArg(
          options::OPT_fexceptions, options::OPT_fcxx_exceptions,
          options::OPT_fobjc_exceptions, options::OPT_fsjlj_exceptions,
          options::OPT_fdwarf_exceptions, options::OPT_fwasm_exceptions,
          options::OPT_fseh_exceptions)) {
    getDriver().Diag(diag::err_drv_unsupported_opt_for_target)
        << A->getSpelling() << Triple.getTriple();
  }

  if (const auto *A = DriverArgs.getLastArg(
          options::OPT_funwind_tables, options::OPT_fasynchronous_unwind_tables)) {
    getDriver().Diag(diag::err_drv_unsupported_opt_for_target)
        << A->getSpelling() << Triple.getTriple();
  }
}

Tool *I8085ToolChain::buildLinker() const {
  return new tools::gnutools::Linker(*this);
}
