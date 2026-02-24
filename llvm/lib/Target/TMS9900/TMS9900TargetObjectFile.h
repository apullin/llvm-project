//===-- TMS9900TargetObjectFile.h - TMS9900 Object Info ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_TMS9900_TMS9900TARGETOBJECTFILE_H
#define LLVM_LIB_TARGET_TMS9900_TMS9900TARGETOBJECTFILE_H

#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"

namespace llvm {

class TMS9900TargetObjectFile : public TargetLoweringObjectFileELF {
public:
  TMS9900TargetObjectFile() = default;

  void Initialize(MCContext &Ctx, const TargetMachine &TM) override;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_TMS9900_TMS9900TARGETOBJECTFILE_H
