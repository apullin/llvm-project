//===-- TMS9900TargetTransformInfo.h - TMS9900 specific TTI -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_TMS9900_TMS9900TARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_TMS9900_TMS9900TARGETTRANSFORMINFO_H

#include "TMS9900.h"
#include "TMS9900Subtarget.h"
#include "TMS9900TargetMachine.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/BasicTTIImpl.h"

namespace llvm {

class TMS9900TTIImpl : public BasicTTIImplBase<TMS9900TTIImpl> {
  using BaseT = BasicTTIImplBase<TMS9900TTIImpl>;
  using TTI = TargetTransformInfo;
  friend BaseT;

  const TMS9900Subtarget *ST;
  const TMS9900TargetLowering *TLI;

  const TMS9900Subtarget *getST() const { return ST; }
  const TMS9900TargetLowering *getTLI() const { return TLI; }

public:
  explicit TMS9900TTIImpl(const TMS9900TargetMachine *TM, const Function &F)
      : BaseT(TM, F.getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  unsigned getNumberOfRegisters(unsigned ClassID) const {
    bool Vector = (ClassID == 1);
    if (Vector)
      return 0;
    return ST->reserveCRU() ? 13 : 14;
  }

  TypeSize getRegisterBitWidth(TTI::RegisterKind K) const {
    switch (K) {
    case TTI::RGK_Scalar:
      return TypeSize::getFixed(16);
    case TTI::RGK_FixedWidthVector:
    case TTI::RGK_ScalableVector:
      return TypeSize::getFixed(0);
    }
    llvm_unreachable("Unsupported register kind");
  }

  TTI::AddressingModeKind
  getPreferredAddressingMode(const Loop *L, ScalarEvolution *SE) const {
    return TTI::AMK_PostIndexed;
  }

  void getUnrollingPreferences(Loop *L, ScalarEvolution &SE,
                               TTI::UnrollingPreferences &UP,
                               OptimizationRemarkEmitter *ORE) {
    BaseT::getUnrollingPreferences(L, SE, UP, ORE);
    // TMS9900 is a non-pipelined 16-bit CPU with a 64KB address space and zero
    // branch-misprediction penalty. Aggressive unrolling rapidly inflates ROM
    // size and exhausts the 13-14 workspace registers.
    UP.Partial = false;
    UP.Runtime = false;
    const Function *F = L->getHeader()->getParent();
    if (F->hasOptSize()) {
      UP.Threshold = 0;
      UP.MaxIterationsCountToAnalyze = 0;
      return;
    }
    if (TLI->getTargetMachine().getOptLevel() >= CodeGenOptLevel::Aggressive)
      UP.Threshold = 72;
    else
      UP.Threshold = 36;
  }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_TMS9900_TMS9900TARGETTRANSFORMINFO_H
