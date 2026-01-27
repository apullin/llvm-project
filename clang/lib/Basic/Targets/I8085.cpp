//===--- I8085.cpp - Implement I8085 target feature support ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements I8085 TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "I8085.h"
#include "clang/Basic/MacroBuilder.h"

using namespace clang;
using namespace clang::targets;

const char *const I8085TargetInfo::GCCRegNames[] = {
    "a", "b",  "c",  "d",  "e",  "h",  "l",
    "bc", "de", "hl", "sp", "psw"};

ArrayRef<const char *> I8085TargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

void I8085TargetInfo::getTargetDefines(const LangOptions &Opts,
                                       MacroBuilder &Builder) const {
  Builder.defineMacro("__i8085__");
  Builder.defineMacro("__I8085__");
}

bool I8085TargetInfo::validateAsmConstraint(
    const char *&Name, TargetInfo::ConstraintInfo &Info) const {
  if (StringRef(Name).size() > 1)
    return false;

  switch (*Name) {
  case 'r':
    Info.setAllowsRegister();
    return true;
  case 'I':
    Info.setRequiresImmediate(0, 0xff);
    return true;
  default:
    return false;
  }
}
