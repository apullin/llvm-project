//===--- TMS9900.cpp - Implement TMS9900 target feature support -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements TMS9900 TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "TMS9900.h"
#include "clang/Basic/MacroBuilder.h"

using namespace clang;
using namespace clang::targets;

const char *const TMS9900TargetInfo::GCCRegNames[] = {
    "r0", "r1", "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
};

ArrayRef<const char *> TMS9900TargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

void TMS9900TargetInfo::getTargetDefines(const LangOptions &Opts,
                                         MacroBuilder &Builder) const {
  Builder.defineMacro("TMS9900");
  Builder.defineMacro("__TMS9900__");
  Builder.defineMacro("__tms9900__");
}
