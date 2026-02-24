//===-- TMS9900TargetMachine.h - Define TargetMachine for TMS9900 -*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the TMS9900 specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_TMS9900_TMS9900TARGETMACHINE_H
#define LLVM_LIB_TARGET_TMS9900_TMS9900TARGETMACHINE_H

#include "TMS9900Subtarget.h"
#include "llvm/Target/TargetMachine.h"
#include <optional>

namespace llvm {

class TMS9900TargetMachine : public LLVMTargetMachine {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  TMS9900Subtarget Subtarget;

public:
  TMS9900TargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                        StringRef FS, const TargetOptions &Options,
                        std::optional<Reloc::Model> RM,
                        std::optional<CodeModel::Model> CM,
                        CodeGenOptLevel OL, bool JIT);

  const TMS9900Subtarget *getSubtargetImpl(const Function &F) const override {
    return &Subtarget;
  }

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }

  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                            const TargetSubtargetInfo *STI) const override;

  // TMS9900 is big-endian
  bool isLittleEndian() const { return false; }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_TMS9900_TMS9900TARGETMACHINE_H
