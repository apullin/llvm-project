//===-- TMS9900TargetInfo.cpp - TMS9900 Target Implementation -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/TMS9900TargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

Target &llvm::getTheTMS9900Target() {
  static Target TheTMS9900Target;
  return TheTMS9900Target;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeTMS9900TargetInfo() {
  RegisterTarget<Triple::tms9900, /*HasJIT=*/false> X(
      getTheTMS9900Target(), "tms9900", "TMS9900 [experimental]", "TMS9900");
}
