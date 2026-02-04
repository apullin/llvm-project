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
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/MathExtras.h"
#include <algorithm>

namespace llvm {

class Loop;
class OptimizationRemarkEmitter;
class ScalarEvolution;

class I8085TTIImpl : public BasicTTIImplBase<I8085TTIImpl> {
  using BaseT = BasicTTIImplBase<I8085TTIImpl>;
  using TTI = TargetTransformInfo;
  friend BaseT;

  const I8085Subtarget *ST;
  const I8085TargetLowering *TLI;
  const Function *Func;

  const I8085Subtarget *getST() const { return ST; }
  const I8085TargetLowering *getTLI() const { return TLI; }

  unsigned getTypeScale(Type *Ty) const {
    if (!Ty)
      return 1;
    if (Ty->isVectorTy())
      return 16;
    if (Ty->isPointerTy())
      return 2;
    if (!Ty->isSized())
      return 1;
    uint64_t Bytes = getDataLayout().getTypeStoreSize(Ty);
    if (Bytes <= 1)
      return 1;
    if (Bytes <= 2)
      return 2;
    if (Bytes <= 4)
      return 4;
    if (Bytes <= 8)
      return 8;
    return 16;
  }

public:
  explicit I8085TTIImpl(const I8085TargetMachine *TM, const Function &F)
      : BaseT(TM, F.getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()), Func(&F) {}

  unsigned getNumberOfRegisters(unsigned ClassID) const {
    bool Vector = (ClassID == 1);
    if (Vector)
      return 0;
    return 5;
  }

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

    unsigned Scale = getTypeScale(Ty);
    // For types larger than 16 bits, shifts are implemented as loops
    // that iterate N times where N is the shift amount. This makes
    // variable shifts extremely expensive on the i8085.
    bool IsLargeType = Ty && Ty->isIntegerTy() &&
                       Ty->getIntegerBitWidth() > 16;
    switch (ISD) {
    case ISD::MUL:
      return 16 * Scale * Base;
    case ISD::SDIV:
    case ISD::UDIV:
    case ISD::SREM:
    case ISD::UREM:
      return 32 * Scale * Base;
    case ISD::SHL:
    case ISD::SRL:
    case ISD::SRA:
      // Variable shifts on i32/i64 use loops - very expensive
      if (IsLargeType)
        return 32 * Scale * Base;
      return 2 * Scale * Base;
    default:
      break;
    }

    if (!Ty || !Ty->isIntegerTy())
      return Base;

    return Base * getTypeScale(Ty);
  }

  InstructionCost getCastInstrCost(unsigned Opcode, Type *Dst, Type *Src,
                                   TTI::CastContextHint CCH,
                                   TTI::TargetCostKind CostKind,
                                   const Instruction *I = nullptr) {
    InstructionCost Base =
        BaseT::getCastInstrCost(Opcode, Dst, Src, CCH, CostKind, I);
    InstructionCost Cost = Base * std::max(getTypeScale(Dst), getTypeScale(Src));

    // Widening from 16-bit to 32-bit or larger is very expensive on the i8085
    // because it triggers expensive loop-based operations for subsequent
    // arithmetic. Strongly discourage the optimizer from widening types.
    if (Dst && Src && Dst->isIntegerTy() && Src->isIntegerTy()) {
      unsigned DstBits = Dst->getIntegerBitWidth();
      unsigned SrcBits = Src->getIntegerBitWidth();
      if (DstBits > 16 && SrcBits <= 16) {
        // Widening to i32 or larger is very expensive - discourage it
        Cost = Cost * 8;
      }
    }
    return Cost;
  }

  InstructionCost getCmpSelInstrCost(unsigned Opcode, Type *ValTy, Type *CondTy,
                                     CmpInst::Predicate VecPred,
                                     TTI::TargetCostKind CostKind,
                                     const Instruction *I = nullptr) {
    InstructionCost Base =
        BaseT::getCmpSelInstrCost(Opcode, ValTy, CondTy, VecPred, CostKind, I);
    return Base * getTypeScale(ValTy);
  }

  InstructionCost
  getMemoryOpCost(unsigned Opcode, Type *Src, MaybeAlign Alignment,
                  unsigned AddressSpace, TTI::TargetCostKind CostKind,
                  TTI::OperandValueInfo OpInfo = {TTI::OK_AnyValue, TTI::OP_None},
                  const Instruction *I = nullptr) {
    InstructionCost Base = BaseT::getMemoryOpCost(Opcode, Src, Alignment,
                                                  AddressSpace, CostKind, OpInfo, I);
    return Base * getTypeScale(Src);
  }

  InstructionCost getIntrinsicInstrCost(const IntrinsicCostAttributes &ICA,
                                        TTI::TargetCostKind CostKind) {
    // The i8085 has no native count-leading-zeros, count-trailing-zeros, or
    // popcount instructions. These must be emulated via loops or library calls.
    // Report them as expensive to prevent loop idiom recognition from
    // transforming simple shift loops into these intrinsics.
    //
    // Also report abs as expensive since the optimizer may introduce it
    // and it has no direct hardware support.
    switch (ICA.getID()) {
    case Intrinsic::ctlz:
    case Intrinsic::cttz:
    case Intrinsic::ctpop:
    case Intrinsic::abs:
      return TTI::TCC_Expensive;
    case Intrinsic::bswap:
      // bswap is cheap on i8085 - just byte reordering
      // i16: ~4 instructions, i32: ~16 instructions
      if (ICA.getReturnType()->isIntegerTy(16))
        return 4;
      if (ICA.getReturnType()->isIntegerTy(32))
        return 16;
      return TTI::TCC_Expensive;
    case Intrinsic::smin:
    case Intrinsic::smax:
    case Intrinsic::umin:
    case Intrinsic::umax:
      // min/max expand to compare + select
      // Cost depends on operand size
      if (ICA.getReturnType()->isIntegerTy(8))
        return 6;
      if (ICA.getReturnType()->isIntegerTy(16))
        return 10;
      if (ICA.getReturnType()->isIntegerTy(32))
        return 20;
      return TTI::TCC_Expensive;
    case Intrinsic::fshl:
    case Intrinsic::fshr:
      // Funnel shifts are expensive - no hardware support
      return TTI::TCC_Expensive;
    default:
      break;
    }
    return BaseT::getIntrinsicInstrCost(ICA, CostKind);
  }

  void getUnrollingPreferences(Loop *L, ScalarEvolution &SE,
                               TTI::UnrollingPreferences &UP,
                               OptimizationRemarkEmitter *ORE) {
    BaseT::getUnrollingPreferences(L, SE, UP, ORE);
    if (!Func)
      return;

    unsigned MaxCount = 4;
    unsigned Threshold = 100;
    if (Func->hasMinSize()) {
      MaxCount = 2;
      Threshold = 40;
      UP.Partial = false;
      UP.UnrollRemainder = false;
    } else if (Func->hasOptSize()) {
      MaxCount = 3;
      Threshold = 60;
      UP.Partial = false;
    }

    UP.Threshold = std::min(UP.Threshold, Threshold);
    UP.MaxCount = std::min(UP.MaxCount, MaxCount);
  }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_I8085_I8085TARGETTRANSFORMINFO_H
