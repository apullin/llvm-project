//===-- I8085SelectionDAGInfo.cpp - I8085 SelectionDAG Info ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the I8085SelectionDAGInfo class.
//
//===----------------------------------------------------------------------===//

#include "I8085SelectionDAGInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"

using namespace llvm;

static constexpr uint64_t kMaxInlineMemOpSize = 16;

SDValue I8085SelectionDAGInfo::EmitTargetCodeForMemcpy(
    SelectionDAG &DAG, const SDLoc &dl, SDValue Chain, SDValue Dst, SDValue Src,
    SDValue Size, Align Alignment, bool isVolatile, bool AlwaysInline,
    MachinePointerInfo DstPtrInfo, MachinePointerInfo SrcPtrInfo) const {
  auto *ConstantSize = dyn_cast<ConstantSDNode>(Size);
  if (!ConstantSize)
    return SDValue();

  uint64_t CopyLen = ConstantSize->getZExtValue();
  if (CopyLen == 0)
    return Chain;
  if (CopyLen > kMaxInlineMemOpSize)
    return SDValue();

  SDValue NewChain = Chain;
  for (uint64_t i = 0; i < CopyLen; ++i) {
    SDValue Off = DAG.getConstant(i, dl, Dst.getValueType());
    SDValue SrcAddr =
        (i == 0) ? Src : DAG.getNode(ISD::ADD, dl, Src.getValueType(), Src, Off);
    SDValue DstAddr =
        (i == 0) ? Dst : DAG.getNode(ISD::ADD, dl, Dst.getValueType(), Dst, Off);

    SDValue Ld = DAG.getLoad(MVT::i8, dl, NewChain, SrcAddr,
                             SrcPtrInfo.getWithOffset(i));
    NewChain = DAG.getStore(Ld.getValue(1), dl, Ld, DstAddr,
                            DstPtrInfo.getWithOffset(i));
  }

  return NewChain;
}

SDValue I8085SelectionDAGInfo::EmitTargetCodeForMemset(
    SelectionDAG &DAG, const SDLoc &dl, SDValue Chain, SDValue Dst,
    SDValue Value, SDValue Size, Align Alignment, bool isVolatile,
    bool AlwaysInline, MachinePointerInfo DstPtrInfo) const {
  auto *ConstantSize = dyn_cast<ConstantSDNode>(Size);
  if (!ConstantSize)
    return SDValue();

  uint64_t SetLen = ConstantSize->getZExtValue();
  if (SetLen == 0)
    return Chain;
  if (SetLen > kMaxInlineMemOpSize)
    return SDValue();

  SDValue Val8 = DAG.getZExtOrTrunc(Value, dl, MVT::i8);
  SDValue NewChain = Chain;
  for (uint64_t i = 0; i < SetLen; ++i) {
    SDValue Off = DAG.getConstant(i, dl, Dst.getValueType());
    SDValue DstAddr =
        (i == 0) ? Dst : DAG.getNode(ISD::ADD, dl, Dst.getValueType(), Dst, Off);
    NewChain = DAG.getStore(NewChain, dl, Val8, DstAddr,
                            DstPtrInfo.getWithOffset(i));
  }

  return NewChain;
}
