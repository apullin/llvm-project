//===-- I8085TargetTransformInfo.h - I8085 specific TTI ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the I8085 specific TargetTransformInfo implementation.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_I8085_I8085TARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_I8085_I8085TARGETTRANSFORMINFO_H

#include "I8085Subtarget.h"
#include "I8085TargetMachine.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/Support/MathExtras.h"

namespace llvm {

class I8085TTIImpl : public BasicTTIImplBase<I8085TTIImpl> {
  using BaseT = BasicTTIImplBase<I8085TTIImpl>;
  using TTI = TargetTransformInfo;
  friend BaseT;

  const I8085Subtarget *ST;
  const I8085TargetLowering *TLI;

  const I8085Subtarget *getST() const { return ST; }
  const I8085TargetLowering *getTLI() const { return TLI; }

public:
  explicit I8085TTIImpl(const I8085TargetMachine *TM, const Function &F)
      : BaseT(TM, F.getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  TypeSize getRegisterBitWidth(TTI::RegisterKind K) const {
    switch (K) {
    case TTI::RGK_Scalar:
      return TypeSize::getFixed(8);
    case TTI::RGK_FixedWidthVector:
      return TypeSize::getFixed(0);
    case TTI::RGK_ScalableVector:
      return TypeSize::getFixed(0);
    }
    llvm_unreachable("Unexpected register kind");
  }

  InstructionCost getIntImmCost(const APInt &Imm, Type *Ty,
                                TTI::TargetCostKind CostKind) {
    if (!Ty->isIntegerTy())
      return BaseT::getIntImmCost(Imm, Ty, CostKind);

    unsigned BitSize = Ty->getPrimitiveSizeInBits();
    if (BitSize == 0)
      return TTI::TCC_Free;
    if (BitSize <= 8)
      return TTI::TCC_Basic;
    if (BitSize <= 16)
      return 2 * TTI::TCC_Basic;
    if (BitSize <= 32)
      return 4 * TTI::TCC_Basic;

    return BaseT::getIntImmCost(Imm, Ty, CostKind);
  }

  InstructionCost getArithmeticInstrCost(
      unsigned Opcode, Type *Ty, TTI::TargetCostKind CostKind,
      TTI::OperandValueInfo Op1Info = {TTI::OK_AnyValue, TTI::OP_None},
      TTI::OperandValueInfo Op2Info = {TTI::OK_AnyValue, TTI::OP_None},
      ArrayRef<const Value *> Args = std::nullopt,
      const Instruction *CxtI = nullptr) {
    InstructionCost Base =
        BaseT::getArithmeticInstrCost(Opcode, Ty, CostKind, Op1Info, Op2Info,
                                      Args, CxtI);
    int ISD = TLI->InstructionOpcodeToISD(Opcode);

    switch (ISD) {
    case ISD::MUL:
    case ISD::SDIV:
    case ISD::UDIV:
    case ISD::SREM:
    case ISD::UREM:
      return 32 * Base;
    default:
      break;
    }

    if (Ty && Ty->isIntegerTy(32))
      return 8 * Base;

    return Base;
  }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_I8085_I8085TARGETTRANSFORMINFO_H
