//===-- I8085ISelLowering.cpp - I8085 DAG Lowering Implementation -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that I8085 uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#include "I8085ISelLowering.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>

#include "I8085.h"
#include "I8085MachineFunctionInfo.h"
#include "I8085Subtarget.h"
#include "I8085TargetMachine.h"
#include "MCTargetDesc/I8085MCTargetDesc.h"

namespace llvm {

static void fail(const SDLoc &DL, SelectionDAG &DAG, const Twine &Msg,
                 SDValue Val = {}) {
  std::string Str;
  if (Val) {
    raw_string_ostream OS(Str);
    Val->print(OS);
    OS << ' ';
  }
  MachineFunction &MF = DAG.getMachineFunction();
  DAG.getContext()->diagnose(DiagnosticInfoUnsupported(
      MF.getFunction(), Twine(Str).concat(Msg), DL.getDebugLoc()));
}

static void splitI64Value(SDValue Val, SDValue &Lo, SDValue &Hi,
                          SelectionDAG &DAG, const SDLoc &DL) {
  if (Val.getOpcode() == ISD::BUILD_PAIR) {
    Lo = Val.getOperand(0);
    Hi = Val.getOperand(1);
    return;
  }

  auto Parts = DAG.SplitScalar(Val, DL, MVT::i32, MVT::i32);
  Lo = Parts.first;
  Hi = Parts.second;
}

static SDValue buildI64Value(SDValue Lo, SDValue Hi,
                             SelectionDAG &DAG, const SDLoc &DL) {
  return DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i64, Lo, Hi);
}

static bool getPowerOf2ShiftAmount(const ConstantSDNode *C,
                                   unsigned &ShiftAmt) {
  if (!C)
    return false;
  const APInt &Val = C->getAPIntValue();
  if (!Val.isPowerOf2())
    return false;
  ShiftAmt = Val.logBase2();
  return true;
}

static bool isIntegerVT(EVT VT) {
  return VT.isInteger() && VT != MVT::i1;
}

static SDValue lowerI64Load(SDValue Op, SelectionDAG &DAG) {
  SDLoc DL(Op);
  auto *LD = cast<LoadSDNode>(Op.getNode());
  EVT MemVT = LD->getMemoryVT();
  SDValue Base = LD->getBasePtr();
  EVT PtrVT = Base.getValueType();
  Align Align = LD->getOriginalAlign();
  auto Flags = LD->getMemOperand()->getFlags();
  auto AAInfo = LD->getAAInfo();

  if (MemVT != MVT::i64) {
    SDValue Tmp = DAG.getLoad(MemVT, DL, LD->getChain(), Base,
                              LD->getPointerInfo(), Align, Flags, AAInfo);
    SDValue Ext;
    switch (LD->getExtensionType()) {
    case ISD::SEXTLOAD:
      Ext = DAG.getNode(ISD::SIGN_EXTEND, DL, MVT::i64, Tmp);
      break;
    case ISD::ZEXTLOAD:
      Ext = DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i64, Tmp);
      break;
    case ISD::EXTLOAD:
      Ext = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, Tmp);
      break;
    default:
      Ext = DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i64, Tmp);
      break;
    }
    return DAG.getMergeValues({Ext, Tmp.getValue(1)}, DL);
  }

  SDValue Lo = DAG.getLoad(MVT::i32, DL, LD->getChain(), Base,
                           LD->getPointerInfo(), Align, Flags, AAInfo);
  SDValue HiPtr = DAG.getNode(ISD::ADD, DL, PtrVT, Base,
                              DAG.getConstant(4, DL, PtrVT));
  SDValue Hi = DAG.getLoad(MVT::i32, DL, Lo.getValue(1), HiPtr,
                           LD->getPointerInfo().getWithOffset(4), Align,
                           Flags, AAInfo);

  SDValue Val = buildI64Value(Lo, Hi, DAG, DL);
  return DAG.getMergeValues({Val, Hi.getValue(1)}, DL);
}

static SDValue lowerI64Store(SDValue Op, SelectionDAG &DAG) {
  SDLoc DL(Op);
  auto *ST = cast<StoreSDNode>(Op.getNode());
  EVT MemVT = ST->getMemoryVT();
  SDValue Base = ST->getBasePtr();
  EVT PtrVT = Base.getValueType();
  Align Align = ST->getOriginalAlign();
  SDValue Val = ST->getValue();
  auto Flags = ST->getMemOperand()->getFlags();
  auto AAInfo = ST->getAAInfo();

  if (MemVT != MVT::i64) {
    SDValue Trunc = DAG.getNode(ISD::TRUNCATE, DL, MemVT, Val);
    return DAG.getStore(ST->getChain(), DL, Trunc, Base,
                        ST->getPointerInfo(), Align, Flags, AAInfo);
  }

  SDValue Lo, Hi;
  splitI64Value(Val, Lo, Hi, DAG, DL);

  SDValue StoreLo = DAG.getStore(ST->getChain(), DL, Lo, Base,
                                 ST->getPointerInfo(), Align, Flags, AAInfo);
  SDValue HiPtr = DAG.getNode(ISD::ADD, DL, PtrVT, Base,
                              DAG.getConstant(4, DL, PtrVT));
  SDValue StoreHi = DAG.getStore(StoreLo, DL, Hi, HiPtr,
                                 ST->getPointerInfo().getWithOffset(4), Align,
                                 Flags, AAInfo);
  return StoreHi;
}

I8085TargetLowering::I8085TargetLowering(const I8085TargetMachine &TM,
                                     const I8085Subtarget &STI)
    : TargetLowering(TM), Subtarget(STI) {
  // Set up the register classes.
  addRegisterClass(MVT::i8, &I8085::GR8RegClass);
  addRegisterClass(MVT::i16, &I8085::GR16RegClass);
  addRegisterClass(MVT::i32, &I8085::GR32RegClass);

  // Compute derived properties from the register classes.
  computeRegisterProperties(Subtarget.getRegisterInfo());

  setBooleanContents(ZeroOrOneBooleanContent);
  setBooleanVectorContents(ZeroOrOneBooleanContent);
  setSchedulingPreference(Sched::RegPressure);
  setStackPointerRegisterToSaveRestore(I8085::SP);
  setSupportsUnalignedAtomics(false);
  // Atomics are not supported; make them Custom so we can emit diagnostics.
  for (MVT VT : {MVT::i1, MVT::i8, MVT::i16, MVT::i32, MVT::i64}) {
    for (unsigned Op :
         {ISD::ATOMIC_LOAD, ISD::ATOMIC_STORE, ISD::ATOMIC_CMP_SWAP,
          ISD::ATOMIC_CMP_SWAP_WITH_SUCCESS, ISD::ATOMIC_SWAP,
          ISD::ATOMIC_LOAD_ADD, ISD::ATOMIC_LOAD_SUB, ISD::ATOMIC_LOAD_AND,
          ISD::ATOMIC_LOAD_CLR, ISD::ATOMIC_LOAD_OR, ISD::ATOMIC_LOAD_XOR,
          ISD::ATOMIC_LOAD_NAND, ISD::ATOMIC_LOAD_MIN, ISD::ATOMIC_LOAD_MAX,
          ISD::ATOMIC_LOAD_UMIN, ISD::ATOMIC_LOAD_UMAX,
          ISD::ATOMIC_LOAD_FADD, ISD::ATOMIC_LOAD_FSUB,
          ISD::ATOMIC_LOAD_FMAX, ISD::ATOMIC_LOAD_FMIN,
          ISD::ATOMIC_LOAD_UINC_WRAP, ISD::ATOMIC_LOAD_UDEC_WRAP}) {
      setOperationAction(Op, VT, Custom);
    }
  }
  setOperationAction(ISD::ATOMIC_FENCE, MVT::Other, Custom);
  // Force custom memcpy/memset expansion in SelectionDAGInfo.
  MaxStoresPerMemcpy = 0;
  MaxStoresPerMemcpyOptSize = 0;
  MaxStoresPerMemmove = 0;
  MaxStoresPerMemmoveOptSize = 0;
  MaxStoresPerMemset = 0;
  MaxStoresPerMemsetOptSize = 0;

  setTruncStoreAction(MVT::i16, MVT::i8, Expand);
  setTruncStoreAction(MVT::i32, MVT::i8, Expand);
  setTruncStoreAction(MVT::i32, MVT::i16, Expand);
  setOperationAction(ISD::LOAD, MVT::i64, Custom);
  setOperationAction(ISD::STORE, MVT::i64, Custom);
  setOperationAction(ISD::ZERO_EXTEND, MVT::i64, Custom);
  setOperationAction(ISD::SIGN_EXTEND, MVT::i64, Custom);
  setOperationAction(ISD::ANY_EXTEND, MVT::i64, Custom);
  setOperationAction(ISD::TRUNCATE, MVT::i32, Custom);

  setOperationAction(ISD::FADD, MVT::f32, LibCall);
  setOperationAction(ISD::FSUB, MVT::f32, LibCall);
  setOperationAction(ISD::FMUL, MVT::f32, LibCall);
  setOperationAction(ISD::FDIV, MVT::f32, LibCall);
  setOperationAction(ISD::SETCC, MVT::f32, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::f32, Expand);
  setOperationAction(ISD::SELECT, MVT::f32, Expand);
  setOperationAction(ISD::FP_TO_SINT, MVT::i32, LibCall);
  setOperationAction(ISD::FP_TO_UINT, MVT::i32, LibCall);
  setOperationAction(ISD::SINT_TO_FP, MVT::i32, LibCall);
  setOperationAction(ISD::UINT_TO_FP, MVT::i32, LibCall);

  for (MVT VT : {MVT::i1, MVT::i8, MVT::i16, MVT::i32})
    setOperationAction(ISD::SIGN_EXTEND_INREG, VT, Custom);

  setTargetDAGCombine(ISD::ADD);
  setTargetDAGCombine(ISD::SUB);
  setTargetDAGCombine(ISD::AND);
  setTargetDAGCombine(ISD::OR);
  setTargetDAGCombine(ISD::XOR);
  setTargetDAGCombine(ISD::SHL);
  setTargetDAGCombine(ISD::SRL);
  setTargetDAGCombine(ISD::SRA);
  setTargetDAGCombine(ISD::MUL);
  setTargetDAGCombine(ISD::UDIV);
  setTargetDAGCombine(ISD::UREM);

  for (MVT VT : MVT::integer_valuetypes()) {
    for (auto N : {ISD::EXTLOAD, ISD::SEXTLOAD, ISD::ZEXTLOAD}) {
      setLoadExtAction(N, VT, MVT::i1, Promote);
      setLoadExtAction(N, VT, MVT::i8, Expand);
      setLoadExtAction(N, VT, MVT::i16, Expand);
      setLoadExtAction(N, VT, MVT::i32, Expand);
    }
  }

  setOperationAction(ISD::MUL, MVT::i8, LibCall);
  setOperationAction(ISD::MUL, MVT::i16, LibCall);
  setOperationAction(ISD::MUL, MVT::i32, LibCall);
  setOperationAction(ISD::MUL, MVT::i64, Custom);
  for (MVT VT : {MVT::i8, MVT::i16, MVT::i32, MVT::i64}) {
    setOperationAction(ISD::MULHS, VT, Expand);
    setOperationAction(ISD::MULHU, VT, Expand);
    setOperationAction(ISD::SMUL_LOHI, VT, Expand);
    setOperationAction(ISD::UMUL_LOHI, VT, Expand);
  }

  setOperationAction(ISD::SDIV, MVT::i8, LibCall);
  setOperationAction(ISD::SDIV, MVT::i16, LibCall);
  setOperationAction(ISD::SDIV, MVT::i32, LibCall);
  setOperationAction(ISD::SDIV, MVT::i64, Custom);

  setOperationAction(ISD::SREM, MVT::i8, LibCall);
  setOperationAction(ISD::SREM, MVT::i16, LibCall);
  setOperationAction(ISD::SREM, MVT::i32, LibCall);
  setOperationAction(ISD::SREM, MVT::i64, Custom);

  setOperationAction(ISD::UDIV, MVT::i8, LibCall);
  setOperationAction(ISD::UDIV, MVT::i16, LibCall);
  setOperationAction(ISD::UDIV, MVT::i32, LibCall);
  setOperationAction(ISD::UDIV, MVT::i64, Custom);

  setOperationAction(ISD::UREM, MVT::i8, LibCall);
  setOperationAction(ISD::UREM, MVT::i16, LibCall);
  setOperationAction(ISD::UREM, MVT::i32, LibCall);
  setOperationAction(ISD::UREM, MVT::i64, Custom);

  setOperationAction(ISD::SHL, MVT::i64, Custom);
  setOperationAction(ISD::SRA, MVT::i64, Custom);
  setOperationAction(ISD::SRL, MVT::i64, Custom);
  setOperationAction(ISD::SHL_PARTS, MVT::i32, Expand);
  setOperationAction(ISD::SRA_PARTS, MVT::i32, Expand);
  setOperationAction(ISD::SRL_PARTS, MVT::i32, Expand);
  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VAEND, MVT::Other, Custom);
  setOperationAction(ISD::VACOPY, MVT::Other, Custom);
  setOperationAction(ISD::VAARG, MVT::Other, Expand);
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i8, Expand);
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i16, Expand);

  for (MVT VT : {MVT::i8, MVT::i16}) {
    setOperationAction(ISD::CTLZ, VT, Expand);
    setOperationAction(ISD::CTLZ_ZERO_UNDEF, VT, Expand);
    setOperationAction(ISD::CTTZ, VT, Expand);
    setOperationAction(ISD::CTTZ_ZERO_UNDEF, VT, Expand);
    setOperationAction(ISD::CTPOP, VT, Expand);
  }
  setOperationAction(ISD::BITREVERSE, MVT::i8, Expand);
  setOperationAction(ISD::BITREVERSE, MVT::i16, Expand);
  setOperationAction(ISD::BITREVERSE, MVT::i32, Expand);
  setOperationAction(ISD::BITREVERSE, MVT::i64, Expand);
  setOperationAction(ISD::BSWAP, MVT::i16, Expand);
  setOperationAction(ISD::BSWAP, MVT::i32, Expand);
  setOperationAction(ISD::BSWAP, MVT::i64, Expand);
  setOperationAction(ISD::ROTL, MVT::i8, Expand);
  setOperationAction(ISD::ROTR, MVT::i8, Expand);
  setOperationAction(ISD::ROTL, MVT::i16, Expand);
  setOperationAction(ISD::ROTR, MVT::i16, Expand);
  setOperationAction(ISD::ROTL, MVT::i32, Expand);
  setOperationAction(ISD::ROTR, MVT::i32, Expand);
  setOperationAction(ISD::ROTL, MVT::i64, Expand);
  setOperationAction(ISD::ROTR, MVT::i64, Expand);
  setOperationAction(ISD::CTLZ, MVT::i32, Custom);
  setOperationAction(ISD::CTLZ_ZERO_UNDEF, MVT::i32, Custom);
  setOperationAction(ISD::CTLZ, MVT::i64, Custom);
  setOperationAction(ISD::CTLZ_ZERO_UNDEF, MVT::i64, Custom);
  setOperationAction(ISD::CTTZ, MVT::i32, Custom);
  setOperationAction(ISD::CTTZ_ZERO_UNDEF, MVT::i32, Custom);
  setOperationAction(ISD::CTTZ, MVT::i64, Custom);
  setOperationAction(ISD::CTTZ_ZERO_UNDEF, MVT::i64, Custom);
  setOperationAction(ISD::CTPOP, MVT::i32, Expand);
  setOperationAction(ISD::CTPOP, MVT::i64, Expand);

  for (MVT VT : {MVT::i8, MVT::i16, MVT::i32}) {
    setOperationAction(ISD::SELECT, VT, Legal);
    setOperationAction(ISD::SELECT_CC, VT, Custom);
  }
  setOperationAction(ISD::SELECT, MVT::i64, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i64, Expand);

  setLibcallName(RTLIB::MUL_I8, "__mul8");
  setLibcallName(RTLIB::MUL_I16, "__mul16");
  setLibcallName(RTLIB::MUL_I32, "__mul32");
  setLibcallName(RTLIB::MUL_I64, "__muldi3");

  setLibcallName(RTLIB::SDIV_I8, "__sdiv8");
  setLibcallName(RTLIB::SDIV_I16, "__sdiv16");
  setLibcallName(RTLIB::SDIV_I32, "__sdiv32");
  setLibcallName(RTLIB::SDIV_I64, "__divdi3");

  setLibcallName(RTLIB::SREM_I8, "__srem8");
  setLibcallName(RTLIB::SREM_I16, "__srem16");
  setLibcallName(RTLIB::SREM_I32, "__srem32");
  setLibcallName(RTLIB::SREM_I64, "__moddi3");

  setLibcallName(RTLIB::UDIV_I8, "__udiv8");
  setLibcallName(RTLIB::UDIV_I16, "__udiv16");
  setLibcallName(RTLIB::UDIV_I32, "__udiv32");
  setLibcallName(RTLIB::UDIV_I64, "__udivdi3");

  setLibcallName(RTLIB::UREM_I8, "__urem8");
  setLibcallName(RTLIB::UREM_I16, "__urem16");
  setLibcallName(RTLIB::UREM_I32, "__urem32");
  setLibcallName(RTLIB::UREM_I64, "__umoddi3");

  setLibcallName(RTLIB::ADD_F32, "__addsf3");
  setLibcallName(RTLIB::SUB_F32, "__subsf3");
  setLibcallName(RTLIB::MUL_F32, "__mulsf3");
  setLibcallName(RTLIB::DIV_F32, "__divsf3");
  setLibcallName(RTLIB::SINTTOFP_I32_F32, "__floatsisf");
  setLibcallName(RTLIB::UINTTOFP_I32_F32, "__floatunsisf");
  setLibcallName(RTLIB::FPTOSINT_F32_I32, "__fixsfsi");
  setLibcallName(RTLIB::FPTOUINT_F32_I32, "__fixunssfsi");
  setLibcallName(RTLIB::OEQ_F32, "__eqsf2");
  setLibcallName(RTLIB::UNE_F32, "__nesf2");
  setLibcallName(RTLIB::OLT_F32, "__ltsf2");
  setLibcallName(RTLIB::OLE_F32, "__lesf2");
  setLibcallName(RTLIB::OGT_F32, "__gtsf2");
  setLibcallName(RTLIB::OGE_F32, "__gesf2");
  setLibcallName(RTLIB::UO_F32, "__unordsf2");

  setLibcallName(RTLIB::CTLZ_I32, "__clzsi2");
  setLibcallName(RTLIB::CTLZ_I64, "__clzdi2");
  setLibcallName(RTLIB::CTTZ_I32, "__ctzsi2");
  setLibcallName(RTLIB::CTTZ_I64, "__ctzdi2");

  setLibcallName(RTLIB::SHL_I64, "__ashldi3");
  setLibcallName(RTLIB::SRL_I64, "__lshrdi3");
  setLibcallName(RTLIB::SRA_I64, "__ashrdi3");

  setOperationAction(ISD::GlobalAddress, MVT::i16, Custom);
  setOperationAction(ISD::BlockAddress, MVT::i16, Custom);
  setOperationAction(ISD::ConstantPool, MVT::i16, Custom);

  setMinFunctionAlignment(Align(2));
  setMinimumJumpTableEntries(UINT_MAX);
  setMaxAtomicSizeInBitsSupported(0);
}

const char *I8085TargetLowering::getTargetNodeName(unsigned Opcode) const {
#define NODE(name)                                                             \
  case I8085ISD::name:                                                           \
    return #name

  switch (Opcode) {
  default:
    return nullptr;
    NODE(RET_FLAG);
    NODE(RETI_FLAG);
    NODE(CALL);
    NODE(TC_RETURN);
    NODE(WRAPPER);
    NODE(LSL);
    NODE(LSR);
    NODE(ROL);
    NODE(ROR);
    NODE(ASR);
    NODE(BRCOND);
    NODE(CMP);
    NODE(CMPC);
    NODE(TST);
    NODE(SELECT_CC);
#undef NODE
  }
}

TargetLowering::ConstraintType
I8085TargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
      return C_RegisterClass;
    case 'I':
      return C_Immediate;
    default:
      break;
    }
  }
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
I8085TargetLowering::getRegForInlineAsmConstraint(
    const TargetRegisterInfo *TRI, StringRef Constraint, MVT VT) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
      if (VT == MVT::i8)
        return std::make_pair(0U, &I8085::GR8RegClass);
      if (VT == MVT::i16)
        return std::make_pair(0U, &I8085::GR16RegClass);
      if (VT == MVT::i32)
        return std::make_pair(0U, &I8085::GR32RegClass);
      break;
    default:
      break;
    }
  }

  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

void I8085TargetLowering::LowerAsmOperandForConstraint(
    SDValue Op, StringRef Constraint, std::vector<SDValue> &Ops,
    SelectionDAG &DAG) const {
  if (Constraint.size() != 1)
    return;

  switch (Constraint[0]) {
  default:
    break;
  case 'I': {
    const ConstantSDNode *C = dyn_cast<ConstantSDNode>(Op);
    if (!C)
      return;

    uint64_t Val = C->getZExtValue();
    if (!isUInt<8>(Val))
      return;

    Ops.push_back(DAG.getTargetConstant(Val, SDLoc(Op), Op.getValueType()));
    return;
  }
  }

  TargetLowering::LowerAsmOperandForConstraint(Op, Constraint, Ops, DAG);
}

EVT I8085TargetLowering::getSetCCResultType(const DataLayout &DL, LLVMContext &,
                                          EVT VT) const {
  assert(!VT.isVector() && "No I8085 SetCC type for vectors!");
  return MVT::i8;
}

void I8085TargetLowering::computeKnownBitsForFrameIndex(
    int FrameIdx, KnownBits &Known, const MachineFunction &MF) const {
  // Clamp object alignment to the ABI stack alignment so we don't assume
  // stronger alignment than the runtime provides.
  Align ObjAlign = MF.getFrameInfo().getObjectAlign(FrameIdx);
  Align StackAlign = MF.getSubtarget<I8085Subtarget>()
                         .getFrameLowering()
                         ->getStackAlign();
  Align KnownAlign = std::min(ObjAlign, StackAlign);
  if (KnownAlign > Align(1))
    Known.Zero.setLowBits(Log2(KnownAlign));
}

SDValue I8085TargetLowering::LowerGlobalAddress(SDValue Op,
                                              SelectionDAG &DAG) const {
  auto DL = DAG.getDataLayout();

  const GlobalValue *GV = cast<GlobalAddressSDNode>(Op)->getGlobal();
  int64_t Offset = cast<GlobalAddressSDNode>(Op)->getOffset();

  // Create the TargetGlobalAddress node, folding in the constant offset.
  SDValue Result =
      DAG.getTargetGlobalAddress(GV, SDLoc(Op), getPointerTy(DL), Offset);
  return DAG.getNode(I8085ISD::WRAPPER, SDLoc(Op), getPointerTy(DL), Result);
}

SDValue I8085TargetLowering::LowerBlockAddress(SDValue Op,
                                             SelectionDAG &DAG) const {
  auto DL = DAG.getDataLayout();
  const BlockAddress *BA = cast<BlockAddressSDNode>(Op)->getBlockAddress();

  SDValue Result = DAG.getTargetBlockAddress(BA, getPointerTy(DL));

  return DAG.getNode(I8085ISD::WRAPPER, SDLoc(Op), getPointerTy(DL), Result);
}

SDValue I8085TargetLowering::LowerConstantPool(SDValue Op,
                                              SelectionDAG &DAG) const {
  auto DL = DAG.getDataLayout();
  const ConstantPoolSDNode *CP = cast<ConstantPoolSDNode>(Op);

  SDValue Result =
      DAG.getTargetConstantPool(CP->getConstVal(), getPointerTy(DL),
                                CP->getAlign(), CP->getOffset());

  return DAG.getNode(I8085ISD::WRAPPER, SDLoc(Op), getPointerTy(DL), Result);
}

/// Returns appropriate CP/CPI/CPC nodes code for the given 8/16-bit operands.
SDValue I8085TargetLowering::getI8085Cmp(SDValue LHS, SDValue RHS,
                                     SelectionDAG &DAG, SDLoc DL) const {
  assert((LHS.getSimpleValueType() == RHS.getSimpleValueType()) &&
         "LHS and RHS have different types");
  assert(((LHS.getSimpleValueType() == MVT::i16) ||
          (LHS.getSimpleValueType() == MVT::i8)) &&
         "invalid comparison type");

  SDValue Cmp;

  if (LHS.getSimpleValueType() == MVT::i16 && isa<ConstantSDNode>(RHS)) {
    // Generate a CPI/CPC pair if RHS is a 16-bit constant.
    SDValue LHSlo = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i8, LHS,
                                DAG.getIntPtrConstant(0, DL));
    SDValue LHShi = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i8, LHS,
                                DAG.getIntPtrConstant(1, DL));
    SDValue RHSlo = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i8, RHS,
                                DAG.getIntPtrConstant(0, DL));
    SDValue RHShi = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i8, RHS,
                                DAG.getIntPtrConstant(1, DL));
    Cmp = DAG.getNode(I8085ISD::CMP, DL, MVT::Glue, LHSlo, RHSlo);
    Cmp = DAG.getNode(I8085ISD::CMPC, DL, MVT::Glue, LHShi, RHShi, Cmp);
  } else {
    // Generate ordinary 16-bit comparison.
    Cmp = DAG.getNode(I8085ISD::CMP, DL, MVT::Glue, LHS, RHS);
  }

  return Cmp;
}

SDValue I8085TargetLowering::LowerShiftI64(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Val = Op.getOperand(0);
  SDValue Amt = Op.getOperand(1);

  if (Amt.getValueType() != MVT::i32)
    Amt = DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i32, Amt);

  SDValue Mask6 = DAG.getConstant(63, DL, MVT::i32);
  SDValue Mask5 = DAG.getConstant(31, DL, MVT::i32);
  SDValue Zero = DAG.getConstant(0, DL, MVT::i32);
  SDValue Const32 = DAG.getConstant(32, DL, MVT::i32);
  SDValue Const31 = DAG.getConstant(31, DL, MVT::i32);

  SDValue AmtMasked = DAG.getNode(ISD::AND, DL, MVT::i32, Amt, Mask6);
  SDValue AmtLo = DAG.getNode(ISD::AND, DL, MVT::i32, AmtMasked, Mask5);
  SDValue InvAmt = DAG.getNode(ISD::SUB, DL, MVT::i32, Const32, AmtLo);

  SDValue Lo;
  SDValue Hi;
  splitI64Value(Val, Lo, Hi, DAG, DL);

  SDValue Lo1;
  SDValue Hi1;
  SDValue Lo2;
  SDValue Hi2;

  switch (Op.getOpcode()) {
  case ISD::SHL:
    Lo1 = DAG.getNode(ISD::SHL, DL, MVT::i32, Lo, AmtLo);
    Hi1 = DAG.getNode(ISD::OR, DL, MVT::i32,
                      DAG.getNode(ISD::SHL, DL, MVT::i32, Hi, AmtLo),
                      DAG.getNode(ISD::SRL, DL, MVT::i32, Lo, InvAmt));
    Lo2 = Zero;
    Hi2 = DAG.getNode(ISD::SHL, DL, MVT::i32, Lo, AmtLo);
    break;
  case ISD::SRL:
    Lo1 = DAG.getNode(ISD::OR, DL, MVT::i32,
                      DAG.getNode(ISD::SRL, DL, MVT::i32, Lo, AmtLo),
                      DAG.getNode(ISD::SHL, DL, MVT::i32, Hi, InvAmt));
    Hi1 = DAG.getNode(ISD::SRL, DL, MVT::i32, Hi, AmtLo);
    Lo2 = DAG.getNode(ISD::SRL, DL, MVT::i32, Hi, AmtLo);
    Hi2 = Zero;
    break;
  case ISD::SRA:
    Lo1 = DAG.getNode(ISD::OR, DL, MVT::i32,
                      DAG.getNode(ISD::SRL, DL, MVT::i32, Lo, AmtLo),
                      DAG.getNode(ISD::SHL, DL, MVT::i32, Hi, InvAmt));
    Hi1 = DAG.getNode(ISD::SRA, DL, MVT::i32, Hi, AmtLo);
    Lo2 = DAG.getNode(ISD::SRA, DL, MVT::i32, Hi, AmtLo);
    Hi2 = DAG.getNode(ISD::SRA, DL, MVT::i32, Hi, Const31);
    break;
  default:
    llvm_unreachable("Unexpected shift opcode");
  }

  SDValue Ge32 = DAG.getSetCC(DL, MVT::i1, AmtMasked, Const32, ISD::SETUGE);
  SDValue ZeroAmt = DAG.getSetCC(DL, MVT::i1, AmtMasked, Zero, ISD::SETEQ);

  SDValue LoSel = DAG.getSelect(DL, MVT::i32, Ge32, Lo2, Lo1);
  SDValue HiSel = DAG.getSelect(DL, MVT::i32, Ge32, Hi2, Hi1);
  SDValue LoRes = DAG.getSelect(DL, MVT::i32, ZeroAmt, Lo, LoSel);
  SDValue HiRes = DAG.getSelect(DL, MVT::i32, ZeroAmt, Hi, HiSel);

  return buildI64Value(LoRes, HiRes, DAG, DL);
}

SDValue I8085TargetLowering::LowerOperation(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT VT = Op.getValueType();

  auto lowerI64LibCall = [&](RTLIB::Libcall LC, SDValue LHS,
                             SDValue RHS) -> SDValue {
    ArgListTy Args;
    SDValue Chain = DAG.getEntryNode();
    auto PtrVT = getPointerTy(DAG.getDataLayout());
    Type *RetTy = VT.getTypeForEVT(*DAG.getContext());
    Type *RetTyABI = RetTy;

    // Use an sret temp to match clang's ABI for >32-bit integer returns.
    MachineFrameInfo &MFI = DAG.getMachineFunction().getFrameInfo();
    int RetFI = MFI.CreateStackObject(8, Align(1), false);
    SDValue RetPtr = DAG.getFrameIndex(RetFI, PtrVT);

    ArgListEntry RetEntry;
    RetEntry.Node = RetPtr;
    RetEntry.Ty = PointerType::getUnqual(RetTy);
    RetEntry.IsSRet = true;
    RetEntry.IndirectType = RetTy;
    Args.push_back(RetEntry);
    RetTyABI = Type::getVoidTy(*DAG.getContext());

    ArgListEntry A1;
    A1.Node = LHS;
    A1.Ty = LHS.getValueType().getTypeForEVT(*DAG.getContext());
    Args.push_back(A1);

    ArgListEntry A2;
    A2.Node = RHS;
    A2.Ty = RHS.getValueType().getTypeForEVT(*DAG.getContext());
    Args.push_back(A2);

    SDValue Callee = DAG.getExternalSymbol(getLibcallName(LC), PtrVT);
    TargetLowering::CallLoweringInfo CLI(DAG);
    CLI.setDebugLoc(DL)
        .setChain(Chain)
        .setLibCallee(getLibcallCallingConv(LC), RetTyABI, Callee,
                      std::move(Args));
    std::pair<SDValue, SDValue> CallInfo = LowerCallTo(CLI);

    SDValue Load =
        DAG.getLoad(VT, DL, CallInfo.second, RetPtr, MachinePointerInfo());
    return Load;
  };

  switch (Op.getOpcode()) {
  default:
    llvm_unreachable("Don't know how to custom lower this!");
  case ISD::ATOMIC_FENCE:
  case ISD::ATOMIC_LOAD:
  case ISD::ATOMIC_STORE:
  case ISD::ATOMIC_CMP_SWAP:
  case ISD::ATOMIC_CMP_SWAP_WITH_SUCCESS:
  case ISD::ATOMIC_SWAP:
  case ISD::ATOMIC_LOAD_ADD:
  case ISD::ATOMIC_LOAD_SUB:
  case ISD::ATOMIC_LOAD_AND:
  case ISD::ATOMIC_LOAD_CLR:
  case ISD::ATOMIC_LOAD_OR:
  case ISD::ATOMIC_LOAD_XOR:
  case ISD::ATOMIC_LOAD_NAND:
  case ISD::ATOMIC_LOAD_MIN:
  case ISD::ATOMIC_LOAD_MAX:
  case ISD::ATOMIC_LOAD_UMIN:
  case ISD::ATOMIC_LOAD_UMAX:
  case ISD::ATOMIC_LOAD_FADD:
  case ISD::ATOMIC_LOAD_FSUB:
  case ISD::ATOMIC_LOAD_FMAX:
  case ISD::ATOMIC_LOAD_FMIN:
  case ISD::ATOMIC_LOAD_UINC_WRAP:
  case ISD::ATOMIC_LOAD_UDEC_WRAP:
    fail(DL, DAG, "i8085 does not support atomic operations");
    return SDValue();
  case ISD::VASTART:
    return LowerVASTART(Op, DAG);
  case ISD::VAEND:
    return LowerVAEND(Op, DAG);
  case ISD::VACOPY:
    return LowerVACOPY(Op, DAG);
  case ISD::MUL:
    if (VT == MVT::i64)
      return lowerI64LibCall(RTLIB::MUL_I64, Op.getOperand(0),
                             Op.getOperand(1));
    break;
  case ISD::SDIV:
    if (VT == MVT::i64)
      return lowerI64LibCall(RTLIB::SDIV_I64, Op.getOperand(0),
                             Op.getOperand(1));
    break;
  case ISD::UDIV:
    if (VT == MVT::i64)
      return lowerI64LibCall(RTLIB::UDIV_I64, Op.getOperand(0),
                             Op.getOperand(1));
    break;
  case ISD::SREM:
    if (VT == MVT::i64)
      return lowerI64LibCall(RTLIB::SREM_I64, Op.getOperand(0),
                             Op.getOperand(1));
    break;
  case ISD::UREM:
    if (VT == MVT::i64)
      return lowerI64LibCall(RTLIB::UREM_I64, Op.getOperand(0),
                             Op.getOperand(1));
    break;
  case ISD::ZERO_EXTEND:
  case ISD::ANY_EXTEND:
    if (VT == MVT::i64) {
      SDValue In = Op.getOperand(0);
      EVT InVT = In.getValueType();
      SDValue Lo = (InVT == MVT::i32)
                       ? In
                       : DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i32, In);
      SDValue Hi = DAG.getConstant(0, DL, MVT::i32);
      return buildI64Value(Lo, Hi, DAG, DL);
    }
    break;
  case ISD::SIGN_EXTEND:
    if (VT == MVT::i64) {
      SDValue In = Op.getOperand(0);
      EVT InVT = In.getValueType();
      SDValue Lo = (InVT == MVT::i32)
                       ? In
                       : DAG.getNode(ISD::SIGN_EXTEND, DL, MVT::i32, In);
      SDValue Zero = DAG.getConstant(0, DL, MVT::i32);
      SDValue NegOne = DAG.getConstant(-1, DL, MVT::i32);
      SDValue Sign = DAG.getSetCC(DL, MVT::i1, Lo, Zero, ISD::SETLT);
      SDValue Hi = DAG.getSelect(DL, MVT::i32, Sign, NegOne, Zero);
      return buildI64Value(Lo, Hi, DAG, DL);
    }
    break;
  case ISD::TRUNCATE:
    if (VT == MVT::i32 && Op.getOperand(0).getValueType() == MVT::i64) {
      SDValue Lo, Hi;
      splitI64Value(Op.getOperand(0), Lo, Hi, DAG, DL);
      return Lo;
    }
    break;
  case ISD::CTLZ:
  case ISD::CTLZ_ZERO_UNDEF:
    if (VT == MVT::i32) {
      MakeLibCallOptions CallOptions;
      SDValue Result;
      SDValue Chain;
      std::tie(Result, Chain) = makeLibCall(DAG, RTLIB::CTLZ_I32, VT,
                                            {Op.getOperand(0)}, CallOptions,
                                            DL);
      return Result;
    }
    if (VT == MVT::i64) {
      MakeLibCallOptions CallOptions;
      SDValue Result32;
      SDValue Chain;
      std::tie(Result32, Chain) =
          makeLibCall(DAG, RTLIB::CTLZ_I64, MVT::i32, {Op.getOperand(0)},
                      CallOptions, DL);
      return DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i64, Result32);
    }
    break;
  case ISD::CTTZ:
  case ISD::CTTZ_ZERO_UNDEF:
    if (VT == MVT::i32) {
      MakeLibCallOptions CallOptions;
      SDValue Result;
      SDValue Chain;
      std::tie(Result, Chain) = makeLibCall(DAG, RTLIB::CTTZ_I32, VT,
                                            {Op.getOperand(0)}, CallOptions,
                                            DL);
      return Result;
    }
    if (VT == MVT::i64) {
      MakeLibCallOptions CallOptions;
      SDValue Result32;
      SDValue Chain;
      std::tie(Result32, Chain) =
          makeLibCall(DAG, RTLIB::CTTZ_I64, MVT::i32, {Op.getOperand(0)},
                      CallOptions, DL);
      return DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i64, Result32);
    }
    break;
  case ISD::LOAD:
    if (auto Res = lowerI64Load(Op, DAG))
      return Res;
    break;
  case ISD::STORE:
    if (auto Res = lowerI64Store(Op, DAG))
      return Res;
    break;
  case ISD::SHL:
  case ISD::SRL:
  case ISD::SRA:
    if (VT == MVT::i64)
      return LowerShiftI64(Op, DAG);
    break;
  case ISD::SIGN_EXTEND_INREG: {
    SDValue Val = Op.getOperand(0);
    EVT FromVT = cast<VTSDNode>(Op.getOperand(1))->getVT();
    unsigned FromBits = FromVT.getScalarSizeInBits();
    unsigned ToBits = VT.getScalarSizeInBits();
    if (FromBits >= ToBits)
      return Val;
    SDValue ShiftAmt = DAG.getConstant(ToBits - FromBits, DL, VT);
    SDValue Shl = DAG.getNode(ISD::SHL, DL, VT, Val, ShiftAmt);
    return DAG.getNode(ISD::SRA, DL, VT, Shl, ShiftAmt);
  }
  case ISD::SELECT_CC: {
    SDValue LHS = Op.getOperand(0);
    SDValue RHS = Op.getOperand(1);
    SDValue TrueV = Op.getOperand(2);
    SDValue FalseV = Op.getOperand(3);
    SDValue CC = Op.getOperand(4);
    EVT CmpVT = LHS.getValueType();
    EVT CCVT = getSetCCResultType(DAG.getDataLayout(), *DAG.getContext(), CmpVT);
    SDValue Cond = DAG.getNode(ISD::SETCC, DL, CCVT, LHS, RHS, CC, Op->getFlags());
    return DAG.getSelect(DL, VT, Cond, TrueV, FalseV, Op->getFlags());
  }
  case ISD::GlobalAddress:
    return LowerGlobalAddress(Op, DAG);
  case ISD::BlockAddress:
    return LowerBlockAddress(Op, DAG);
  case ISD::ConstantPool:
    return LowerConstantPool(Op, DAG);
  }

  return SDValue();
}

SDValue I8085TargetLowering::LowerVASTART(SDValue Op,
                                       SelectionDAG &DAG) const {
  const MachineFunction &MF = DAG.getMachineFunction();
  const I8085MachineFunctionInfo *AFI = MF.getInfo<I8085MachineFunctionInfo>();
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  auto DL = DAG.getDataLayout();
  SDLoc dl(Op);

  // Store the address of the first vararg argument into the va_list.
  SDValue FI =
      DAG.getFrameIndex(AFI->getVarArgsFrameIndex(), getPointerTy(DL));

  return DAG.getStore(Op.getOperand(0), dl, FI, Op.getOperand(1),
                      MachinePointerInfo(SV));
}

SDValue I8085TargetLowering::LowerVAEND(SDValue Op, SelectionDAG &DAG) const {
  return Op.getOperand(0);
}

SDValue I8085TargetLowering::LowerVACOPY(SDValue Op, SelectionDAG &DAG) const {
  // va_list is a simple pointer on i8085, so va_copy is just a pointer copy.
  SDLoc dl(Op);
  auto DL = DAG.getDataLayout();
  EVT PtrVT = getPointerTy(DL);

  SDValue Chain = Op.getOperand(0);
  SDValue DstPtr = Op.getOperand(1);
  SDValue SrcPtr = Op.getOperand(2);
  const Value *DstSV = cast<SrcValueSDNode>(Op.getOperand(3))->getValue();
  const Value *SrcSV = cast<SrcValueSDNode>(Op.getOperand(4))->getValue();

  // Load the va_list pointer from the source.
  SDValue SrcVal =
      DAG.getLoad(PtrVT, dl, Chain, SrcPtr, MachinePointerInfo(SrcSV));
  // Store it to the destination.
  return DAG.getStore(SrcVal.getValue(1), dl, SrcVal, DstPtr,
                      MachinePointerInfo(DstSV));
}

/// Replace a node with an illegal result type
/// with a new node built out of custom code.
void I8085TargetLowering::ReplaceNodeResults(SDNode *N,
                                           SmallVectorImpl<SDValue> &Results,
                                           SelectionDAG &DAG) const {
  SDLoc DL(N);

  switch (N->getOpcode()) {
  case ISD::ADD: {
    // Convert add (x, imm) into sub (x, -imm).
    if (const ConstantSDNode *C = dyn_cast<ConstantSDNode>(N->getOperand(1))) {
      SDValue Sub = DAG.getNode(
          ISD::SUB, DL, N->getValueType(0), N->getOperand(0),
          DAG.getConstant(-C->getAPIntValue(), DL, C->getValueType(0)));
      Results.push_back(Sub);
    }
    break;
  }
  default: {
    SDValue Res = LowerOperation(SDValue(N, 0), DAG);

    for (unsigned I = 0, E = Res->getNumValues(); I != E; ++I)
      Results.push_back(Res.getValue(I));

    break;
  }
  }
}

SDValue I8085TargetLowering::PerformDAGCombine(SDNode *N,
                                             DAGCombinerInfo &DCI) const {
  switch (N->getOpcode()) {
  case ISD::ADD:
    return performAddSubCombine(N, DCI);
  case ISD::SUB: {
    if (SDValue V = performAddSubCombine(N, DCI))
      return V;
    return performSubCombine(N, DCI);
  }
  case ISD::AND:
  case ISD::OR:
  case ISD::XOR:
    return performLogicCombine(N, DCI);
  case ISD::SHL:
  case ISD::SRL:
  case ISD::SRA:
    return performShiftCombine(N, DCI);
  case ISD::MUL:
    return performMulCombine(N, DCI);
  case ISD::UDIV:
    return performUDivCombine(N, DCI);
  case ISD::UREM:
    return performURemCombine(N, DCI);
  default:
    break;
  }
  return SDValue();
}

SDValue I8085TargetLowering::performSubCombine(SDNode *N,
                                             DAGCombinerInfo &DCI) const {
  EVT VT = N->getValueType(0);
  if (VT != MVT::i8 && VT != MVT::i16)
    return SDValue();

  auto *CLHS = dyn_cast<ConstantSDNode>(N->getOperand(0));
  if (!CLHS)
    return SDValue();
  if (CLHS->isZero())
    return SDValue();

  if (isa<ConstantSDNode>(N->getOperand(1)))
    return SDValue();

  SelectionDAG &DAG = DCI.DAG;
  SDLoc DL(N);
  SDValue RHS = N->getOperand(1);
  SDValue AllOnes = DAG.getAllOnesConstant(DL, VT);
  SDValue NotRHS = DAG.getNode(ISD::XOR, DL, VT, RHS, AllOnes);
  APInt C = CLHS->getAPIntValue();
  SDValue CPlusOne = DAG.getConstant(C + 1, DL, VT);
  return DAG.getNode(ISD::ADD, DL, VT, NotRHS, CPlusOne);
}

SDValue I8085TargetLowering::performMulCombine(SDNode *N,
                                             DAGCombinerInfo &DCI) const {
  EVT VT = N->getValueType(0);
  if (!isIntegerVT(VT))
    return SDValue();

  SDValue LHS = N->getOperand(0);
  SDValue RHS = N->getOperand(1);
  const ConstantSDNode *C = dyn_cast<ConstantSDNode>(LHS);
  SDValue Other = RHS;
  if (!C) {
    C = dyn_cast<ConstantSDNode>(RHS);
    Other = LHS;
  }

  unsigned ShiftAmt = 0;
  if (!getPowerOf2ShiftAmount(C, ShiftAmt))
    ShiftAmt = 0;

  SelectionDAG &DAG = DCI.DAG;
  SDLoc DL(N);
  if (!C)
    return SDValue();
  if (C->isZero())
    return DAG.getConstant(0, DL, VT);
  if (C->isOne())
    return Other;
  if (!ShiftAmt)
    return SDValue();

  EVT ShiftVT = getShiftAmountTy(VT, DAG.getDataLayout());
  SDValue Amt = DAG.getConstant(ShiftAmt, DL, ShiftVT);
  return DAG.getNode(ISD::SHL, DL, VT, Other, Amt);
}

SDValue I8085TargetLowering::performUDivCombine(SDNode *N,
                                              DAGCombinerInfo &DCI) const {
  EVT VT = N->getValueType(0);
  if (!isIntegerVT(VT))
    return SDValue();

  const ConstantSDNode *C = dyn_cast<ConstantSDNode>(N->getOperand(1));
  unsigned ShiftAmt = 0;
  if (!C)
    return SDValue();
  if (C->isOne())
    return N->getOperand(0);
  if (!getPowerOf2ShiftAmount(C, ShiftAmt))
    return SDValue();

  SelectionDAG &DAG = DCI.DAG;
  SDLoc DL(N);
  EVT ShiftVT = getShiftAmountTy(VT, DAG.getDataLayout());
  SDValue Amt = DAG.getConstant(ShiftAmt, DL, ShiftVT);
  return DAG.getNode(ISD::SRL, DL, VT, N->getOperand(0), Amt);
}

SDValue I8085TargetLowering::performURemCombine(SDNode *N,
                                              DAGCombinerInfo &DCI) const {
  EVT VT = N->getValueType(0);
  if (!isIntegerVT(VT))
    return SDValue();

  const ConstantSDNode *C = dyn_cast<ConstantSDNode>(N->getOperand(1));
  unsigned ShiftAmt = 0;
  if (!C)
    return SDValue();

  SelectionDAG &DAG = DCI.DAG;
  SDLoc DL(N);
  if (C->isOne())
    return DAG.getConstant(0, DL, VT);
  if (!getPowerOf2ShiftAmount(C, ShiftAmt))
    return SDValue();

  APInt Mask = C->getAPIntValue() - 1;
  SDValue MaskVal = DAG.getConstant(Mask, DL, VT);
  return DAG.getNode(ISD::AND, DL, VT, N->getOperand(0), MaskVal);
}

SDValue I8085TargetLowering::performAddSubCombine(SDNode *N,
                                                DAGCombinerInfo &DCI) const {
  EVT VT = N->getValueType(0);
  if (!isIntegerVT(VT))
    return SDValue();

  SDValue LHS = N->getOperand(0);
  SDValue RHS = N->getOperand(1);
  const ConstantSDNode *C = dyn_cast<ConstantSDNode>(RHS);
  if (!C)
    return SDValue();
  if (!C->isZero())
    return SDValue();

  return LHS;
}

SDValue I8085TargetLowering::performLogicCombine(SDNode *N,
                                               DAGCombinerInfo &DCI) const {
  EVT VT = N->getValueType(0);
  if (!isIntegerVT(VT))
    return SDValue();

  SDValue LHS = N->getOperand(0);
  SDValue RHS = N->getOperand(1);
  const ConstantSDNode *C = dyn_cast<ConstantSDNode>(RHS);
  if (!C)
    return SDValue();

  SelectionDAG &DAG = DCI.DAG;
  SDLoc DL(N);
  APInt Val = C->getAPIntValue();
  switch (N->getOpcode()) {
  case ISD::AND:
    if (Val.isAllOnes())
      return LHS;
    if (Val.isZero())
      return DAG.getConstant(0, DL, VT);
    break;
  case ISD::OR:
    if (Val.isZero())
      return LHS;
    if (Val.isAllOnes())
      return DAG.getConstant(Val, DL, VT);
    break;
  case ISD::XOR:
    if (Val.isZero())
      return LHS;
    break;
  default:
    break;
  }
  return SDValue();
}

SDValue I8085TargetLowering::performShiftCombine(SDNode *N,
                                               DAGCombinerInfo &DCI) const {
  EVT VT = N->getValueType(0);
  if (!isIntegerVT(VT))
    return SDValue();

  const ConstantSDNode *C = dyn_cast<ConstantSDNode>(N->getOperand(1));
  if (!C || !C->isZero())
    return SDValue();

  return N->getOperand(0);
}

/// Return true if the addressing mode represented
/// by AM is legal for this target, for a load/store of the specified type.
bool I8085TargetLowering::isLegalAddressingMode(const DataLayout &DL,
                                              const AddrMode &AM, Type *Ty,
                                              unsigned AS,
                                              Instruction *I) const {
  int64_t Offs = AM.BaseOffs;

  // Allow absolute addresses.
  if (AM.BaseGV && !AM.HasBaseReg && AM.Scale == 0 && Offs == 0) {
    return true;
  }

  // Flash memory instructions only allow zero offsets.
  if (isa<PointerType>(Ty) && AS == I8085::ProgramMemory) {
    return false;
  }

  // Allow reg+<6bit> offset.
  if (Offs < 0)
    Offs = -Offs;
  if (AM.BaseGV == nullptr && AM.HasBaseReg && AM.Scale == 0 &&
      isUInt<6>(Offs)) {
    return true;
  }

  return false;
}

/// Returns true by value, base pointer and
/// offset pointer and addressing mode by reference if the node's address
/// can be legally represented as pre-indexed load / store address.
bool I8085TargetLowering::getPreIndexedAddressParts(SDNode *N, SDValue &Base,
                                                  SDValue &Offset,
                                                  ISD::MemIndexedMode &AM,
                                                  SelectionDAG &DAG) const {
  EVT VT;
  const SDNode *Op;
  SDLoc DL(N);

  if (const LoadSDNode *LD = dyn_cast<LoadSDNode>(N)) {
    VT = LD->getMemoryVT();
    Op = LD->getBasePtr().getNode();
    if (LD->getExtensionType() != ISD::NON_EXTLOAD)
      return false;
    if (I8085::isProgramMemoryAccess(LD)) {
      return false;
    }
  } else if (const StoreSDNode *ST = dyn_cast<StoreSDNode>(N)) {
    VT = ST->getMemoryVT();
    Op = ST->getBasePtr().getNode();
    if (I8085::isProgramMemoryAccess(ST)) {
      return false;
    }
  } else {
    return false;
  }

  if (VT != MVT::i8 && VT != MVT::i16) {
    return false;
  }

  if (Op->getOpcode() != ISD::ADD && Op->getOpcode() != ISD::SUB) {
    return false;
  }

  if (const ConstantSDNode *RHS = dyn_cast<ConstantSDNode>(Op->getOperand(1))) {
    int RHSC = RHS->getSExtValue();
    if (Op->getOpcode() == ISD::SUB)
      RHSC = -RHSC;

    if ((VT == MVT::i16 && RHSC != -2) || (VT == MVT::i8 && RHSC != -1)) {
      return false;
    }

    Base = Op->getOperand(0);
    Offset = DAG.getConstant(RHSC, DL, MVT::i8);
    AM = ISD::PRE_DEC;

    return true;
  }

  return false;
}

/// Returns true by value, base pointer and
/// offset pointer and addressing mode by reference if this node can be
/// combined with a load / store to form a post-indexed load / store.
bool I8085TargetLowering::getPostIndexedAddressParts(SDNode *N, SDNode *Op,
                                                   SDValue &Base,
                                                   SDValue &Offset,
                                                   ISD::MemIndexedMode &AM,
                                                   SelectionDAG &DAG) const {
  EVT VT;
  SDLoc DL(N);

  if (const LoadSDNode *LD = dyn_cast<LoadSDNode>(N)) {
    VT = LD->getMemoryVT();
    if (LD->getExtensionType() != ISD::NON_EXTLOAD)
      return false;
  } else if (const StoreSDNode *ST = dyn_cast<StoreSDNode>(N)) {
    VT = ST->getMemoryVT();
    if (I8085::isProgramMemoryAccess(ST)) {
      return false;
    }
  } else {
    return false;
  }

  if (VT != MVT::i8 && VT != MVT::i16) {
    return false;
  }

  if (Op->getOpcode() != ISD::ADD && Op->getOpcode() != ISD::SUB) {
    return false;
  }

  if (const ConstantSDNode *RHS = dyn_cast<ConstantSDNode>(Op->getOperand(1))) {
    int RHSC = RHS->getSExtValue();
    if (Op->getOpcode() == ISD::SUB)
      RHSC = -RHSC;
    if ((VT == MVT::i16 && RHSC != 2) || (VT == MVT::i8 && RHSC != 1)) {
      return false;
    }

    Base = Op->getOperand(0);
    Offset = DAG.getConstant(RHSC, DL, MVT::i8);
    AM = ISD::POST_INC;

    return true;
  }

  return false;
}

bool I8085TargetLowering::isOffsetFoldingLegal(
    const GlobalAddressSDNode *GA) const {
  return true;
}

//===----------------------------------------------------------------------===//
//             Formal Arguments Calling Convention Implementation
//===----------------------------------------------------------------------===//

#include "I8085GenCallingConv.inc"

/// Registers for calling conventions, ordered in reverse as required by ABI.
/// Both arrays must be of the same length.
static const MCPhysReg LLVM_ATTRIBUTE_UNUSED RegList8I8085[] = {
    I8085::B, I8085::C, I8085::D, I8085::E};

static const MCPhysReg LLVM_ATTRIBUTE_UNUSED RegList16I8085[] = {
    I8085::BC, I8085::DE};





/// Analyze incoming and outgoing function arguments. We need custom C++ code
/// to handle special constraints in the ABI.
/// In addition, all pieces of a certain argument have to be passed either
/// using registers or the stack but never mixing both.
template <typename ArgT>
static void analyzeArguments(TargetLowering::CallLoweringInfo *CLI,
                             const Function *F, const DataLayout *TD,
                             const SmallVectorImpl<ArgT> &Args,
                             SmallVectorImpl<CCValAssign> &ArgLocs,
                             CCState &CCInfo, bool Tiny) {
  // Choose the proper register list for argument passing according to the ABI.
  ArrayRef<MCPhysReg> RegList8;
  ArrayRef<MCPhysReg> RegList16;

    RegList8 = ArrayRef(RegList8I8085, std::size(RegList8I8085));
    RegList16 = ArrayRef(RegList16I8085, std::size(RegList16I8085));


  unsigned NumArgs = Args.size();
  // This is the index of the last used register, in RegList*.
  // -1 means R26 (R26 is never actually used in CC).
  int RegLastIdx = -1;
  // Once a value is passed to the stack it will always be used
  bool UseStack = false;
  for (unsigned i = 0; i != NumArgs;) {
    MVT VT = Args[i].VT;
    // We have to count the number of bytes for each function argument, that is
    // those Args with the same OrigArgIndex. This is important in case the
    // function takes an aggregate type.
    // Current argument will be between [i..j).
    unsigned ArgIndex = Args[i].OrigArgIndex;
    unsigned TotalBytes = VT.getStoreSize();
    unsigned j = i + 1;
    for (; j != NumArgs; ++j) {
      if (Args[j].OrigArgIndex != ArgIndex)
        break;
      TotalBytes += Args[j].VT.getStoreSize();
    }
    // Round up to even number of bytes.
    TotalBytes = alignTo(TotalBytes, 2);
    // Skip zero sized arguments
    if (TotalBytes == 0)
      continue;
    // The index of the first register to be used
    unsigned RegIdx = RegLastIdx + TotalBytes;
    RegLastIdx = RegIdx;
    // If there are not enough registers, use the stack
    if (RegIdx >= RegList8.size()) {
      UseStack = true;
    }
    for (; i != j; ++i) {
      MVT VT = Args[i].VT;

      if (UseStack) {
        auto evt = EVT(VT).getTypeForEVT(CCInfo.getContext());
        unsigned Offset = CCInfo.AllocateStack(TD->getTypeAllocSize(evt),
                                               TD->getABITypeAlign(evt));
        CCInfo.addLoc(
            CCValAssign::getMem(i, VT, Offset, VT, CCValAssign::Full));
      } else {
        unsigned Reg;
        if (VT == MVT::i8) {
          Reg = CCInfo.AllocateReg(RegList8[RegIdx]);
        } else if (VT == MVT::i16) {
          Reg = CCInfo.AllocateReg(RegList16[RegIdx]);
        } else {
          llvm_unreachable(
              "calling convention can only manage i8 and i16 types");
        }
        assert(Reg && "register not available in calling convention");
        CCInfo.addLoc(CCValAssign::getReg(i, VT, Reg, VT, CCValAssign::Full));
        // Registers inside a particular argument are sorted in increasing order
        // (remember the array is reversed).
        RegIdx -= VT.getStoreSize();
      }
    }
  }
}

/// Count the total number of bytes needed to pass or return these arguments.
template <typename ArgT>
static unsigned
getTotalArgumentsSizeInBytes(const SmallVectorImpl<ArgT> &Args) {
  unsigned TotalBytes = 0;

  for (const ArgT &Arg : Args) {
    TotalBytes += Arg.VT.getStoreSize();
  }
  return TotalBytes;
}

/// Analyze incoming and outgoing value of returning from a function.
/// The algorithm is similar to analyzeArguments, but there can only be
/// one value, possibly an aggregate, and it is limited to 8 bytes.
template <typename ArgT>
static void analyzeReturnValues(const SmallVectorImpl<ArgT> &Args,
                                CCState &CCInfo, bool Tiny) {
  unsigned NumArgs = Args.size();
  unsigned TotalBytes = getTotalArgumentsSizeInBytes(Args);
  // CanLowerReturn() guarantees this assertion.
  assert(TotalBytes <= 8 &&
         "return values greater than 8 bytes cannot be lowered");

  // Choose the proper register list for argument passing according to the ABI.
  ArrayRef<MCPhysReg> RegList8;
  ArrayRef<MCPhysReg> RegList16;

    RegList8 = ArrayRef(RegList8I8085, std::size(RegList8I8085));
    RegList16 = ArrayRef(RegList16I8085, std::size(RegList16I8085));


  // GCC-ABI says that the size is rounded up to the next even number,
  // but actually once it is more than 4 it will always round up to 8.
  if (TotalBytes > 4) {
    TotalBytes = 8;
  } else {
    TotalBytes = alignTo(TotalBytes, 2);
  }

  // The index of the first register to use.
  int RegIdx = TotalBytes - 1;
  for (unsigned i = 0; i != NumArgs; ++i) {
    MVT VT = Args[i].VT;
    unsigned Reg;
    if (VT == MVT::i8) {
      Reg = CCInfo.AllocateReg(RegList8[RegIdx]);
    } else if (VT == MVT::i16) {
      Reg = CCInfo.AllocateReg(RegList16[RegIdx]);
    } else {
      llvm_unreachable("calling convention can only manage i8 and i16 types");
    }
    assert(Reg && "register not available in calling convention");
    CCInfo.addLoc(CCValAssign::getReg(i, VT, Reg, VT, CCValAssign::Full));
    // Registers sort in increasing order
    RegIdx -= VT.getStoreSize();
  }
}

SDValue I8085TargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &dl,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  auto DL = DAG.getDataLayout();

  // Assign locations to all of the incoming arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());

  // Variadic functions do not need all the analysis below.

  CCInfo.AnalyzeFormalArguments(Ins, ArgCC_I8085_Vararg);


  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    const ISD::InputArg &Arg = Ins[i];

    // Arguments stored on registers.
    if (VA.isRegLoc()) {
      llvm_unreachable("Args should be passed via Stack!");
    } else {
      // Only arguments passed on the stack should make it here.
      assert(VA.isMemLoc());

      EVT LocVT = VA.getLocVT();

      int32_t StackOffset = VA.getLocMemOffset();

      if (Arg.Flags.isByVal() && Arg.Flags.getByValSize()) {
        unsigned Size = Arg.Flags.getByValSize();
        Align Alignment =
            std::max(Align(1), Arg.Flags.getNonZeroByValAlign());
        int FI = MFI.CreateFixedObject(Size, StackOffset, true);
        MFI.setObjectAlignment(FI, Alignment);
        SDValue FIN = DAG.getFrameIndex(FI, getPointerTy(DL));
        InVals.push_back(FIN);
        continue;
      }

      // Create the frame index object for this incoming parameter.
      int FI = MFI.CreateFixedObject(LocVT.getSizeInBits() / 8,
                                     StackOffset, true);

      // Create the SelectionDAG nodes corresponding to a load
      // from this parameter.
      SDValue FIN = DAG.getFrameIndex(FI, getPointerTy(DL));

      SDValue load = DAG.getLoad(LocVT, dl, Chain, FIN,
                                 MachinePointerInfo::getFixedStack(MF, FI));

      InVals.push_back(load);
    }
  }

  // If the function takes variable number of arguments, make a frame index for
  // the start of the first vararg value... for expansion of llvm.va_start.
  if (isVarArg) {
    unsigned StackSize = CCInfo.getStackSize();
    I8085MachineFunctionInfo *AFI = MF.getInfo<I8085MachineFunctionInfo>();

    AFI->setVarArgsFrameIndex(MFI.CreateFixedObject(2, StackSize, true));
  }

  return Chain;
}

uint8_t twos_complement(uint8_t val) { return -(unsigned int)val;}

//===----------------------------------------------------------------------===//
//                  Call Calling Convention Implementation
//===----------------------------------------------------------------------===//

static bool isEligibleForTailCallOptimization(
    TargetLowering::CallLoweringInfo &CLI, CCState &CCInfo,
    MachineFunction &MF) {
  if (!CLI.IsTailCall)
    return false;

  const Function &Caller = MF.getFunction();
  if (Caller.getFnAttribute("disable-tail-calls").getValueAsString() == "true")
    return false;

  if (CLI.IsVarArg)
    return false;

  if (CCInfo.getStackSize() != 0)
    return false;

  if (MF.getFrameInfo().hasVarSizedObjects())
    return false;

  for (const ISD::OutputArg &Out : CLI.Outs) {
    if (Out.Flags.isByVal() || Out.Flags.isSRet() || Out.Flags.isInReg() ||
        Out.Flags.isNest())
      return false;
  }

  return true;
}


SDValue I8085TargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                     SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  bool &isTailCall = CLI.IsTailCall;
  CallingConv::ID CallConv = CLI.CallConv;
  bool isVarArg = CLI.IsVarArg;

  MachineFunction &MF = DAG.getMachineFunction();

  // Analyze operands of the call, assigning locations to each operand.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());


  CCInfo.AnalyzeCallOperands(Outs, ArgCC_I8085_Vararg);

  isTailCall = isEligibleForTailCallOptimization(CLI, CCInfo, MF);

  // If the callee is a GlobalAddress/ExternalSymbol node (quite common, every
  // direct call is) turn it into a TargetGlobalAddress/TargetExternalSymbol
  // node so that legalize doesn't hack it.
  if (const GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    const GlobalValue *GV = G->getGlobal();
    Callee =
        DAG.getTargetGlobalAddress(GV, DL, getPointerTy(DAG.getDataLayout()));
  } else if (const ExternalSymbolSDNode *ES =
                 dyn_cast<ExternalSymbolSDNode>(Callee)) {
    Callee = DAG.getTargetExternalSymbol(ES->getSymbol(),
                                         getPointerTy(DAG.getDataLayout()));
  }

  // Get a count of how many bytes are to be pushed on the stack.
  unsigned NumBytes = CCInfo.getStackSize();


  SmallVector<std::pair<unsigned, SDValue>, 8> RegsToPass;

  // First, walk the register assignments, inserting copies.
  unsigned AI, AE;
  bool HasStackArgs = false;
  for (AI = 0, AE = ArgLocs.size(); AI != AE; ++AI) {
    CCValAssign &VA = ArgLocs[AI];
    EVT RegVT = VA.getLocVT();
    SDValue Arg = OutVals[AI];
    
    if (VA.isMemLoc()) {
      HasStackArgs = true;
      break;
    }

    // Arguments that can be passed on registers must be kept in the RegsToPass
    // vector.
    RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
  }

  if (!isTailCall)
    Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, DL);

  // Second, stack arguments have to walked.
  // Previously this code created chained stores but those chained stores appear
  // to be unchained in the legalization phase. Therefore, do not attempt to
  // chain them here. In fact, chaining them here somehow causes the first and
  // second store to be reversed which is the exact opposite of the intended
  // effect.
  if (HasStackArgs) {
    SmallVector<SDValue, 8> MemOpChains;
    for (; AI != AE; AI++) {

      CCValAssign &VA = ArgLocs[AI];
      SDValue Arg = OutVals[AI];
      ISD::ArgFlagsTy Flags = Outs[AI].Flags;

      assert(VA.isMemLoc());

      SDValue PtrOff = DAG.getNode(
          ISD::ADD, DL, getPointerTy(DAG.getDataLayout()),
          DAG.getRegister(I8085::SP, getPointerTy(DAG.getDataLayout())),
          DAG.getIntPtrConstant(VA.getLocMemOffset(), DL));

      if (Flags.isByVal() && Flags.getByValSize()) {
        unsigned Size = Flags.getByValSize();
        Align Alignment =
            std::max(Align(1), Flags.getNonZeroByValAlign());
        SDValue SizeVal = DAG.getConstant(Size, DL, getPointerTy(DAG.getDataLayout()));
        MemOpChains.push_back(DAG.getMemcpy(
            Chain, DL, PtrOff, Arg, SizeVal, Alignment,
            /*isVolatile=*/false, /*AlwaysInline=*/false, /*CI=*/nullptr,
            std::nullopt, MachinePointerInfo(), MachinePointerInfo()));
      } else {
        MemOpChains.push_back(DAG.getStore(
            Chain, DL, Arg, PtrOff,
            MachinePointerInfo::getStack(MF, VA.getLocMemOffset())));
      }

    }

    if (!MemOpChains.empty())
      Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);
  }

  // Build a sequence of copy-to-reg nodes chained together with token chain and
  // flag operands which copy the outgoing args into registers.  The InFlag in
  // necessary since all emited instructions must be stuck together.
  SDValue InFlag;
  for (auto Reg : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg.first, Reg.second, InFlag);
    InFlag = Chain.getValue(1);
  }

  // Returns a chain & a flag for retval copy to use.
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);

  // Add argument registers to the end of the list so that they are known live
  // into the call.
  for (auto Reg : RegsToPass) {
    Ops.push_back(DAG.getRegister(Reg.first, Reg.second.getValueType()));
  }

  // Add a register mask operand representing the call-preserved registers.
  const TargetRegisterInfo *TRI = Subtarget.getRegisterInfo();
  const uint32_t *Mask =
      TRI->getCallPreservedMask(DAG.getMachineFunction(), CallConv);
  assert(Mask && "Missing call preserved mask for calling convention");
  Ops.push_back(DAG.getRegisterMask(Mask));

  if (InFlag.getNode())
    Ops.push_back(InFlag);

  if (isTailCall) {
    SmallVector<SDValue, 8> TailOps;
    TailOps.push_back(Chain);
    TailOps.push_back(Callee);
    TailOps.push_back(DAG.getConstant(0, DL, MVT::i16));

    for (auto Reg : RegsToPass)
      TailOps.push_back(
          DAG.getRegister(Reg.first, Reg.second.getValueType()));

    TailOps.push_back(DAG.getRegisterMask(Mask));

    if (InFlag.getNode())
      TailOps.push_back(InFlag);

    MF.getFrameInfo().setHasTailCall();
    return DAG.getNode(I8085ISD::TC_RETURN, DL, NodeTys, TailOps);
  }

  Chain = DAG.getNode(I8085ISD::CALL, DL, NodeTys, Ops);
  InFlag = Chain.getValue(1);

  // Create the CALLSEQ_END node.
  Chain = DAG.getCALLSEQ_END(Chain, NumBytes, 0, InFlag, DL);

  if (!Ins.empty()) {
    InFlag = Chain.getValue(1);
  }

  // Handle result values, copying them out of physregs into vregs that we
  // return.
  return LowerCallResult(Chain, InFlag, CallConv, isVarArg, Ins, DL, DAG,
                         InVals);
}

/// Lower the result values of a call into the
/// appropriate copies out of appropriate physical registers.
///
SDValue I8085TargetLowering::LowerCallResult(
    SDValue Chain, SDValue InFlag, CallingConv::ID CallConv, bool isVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &dl,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {

  if (Ins.size() == 1 && Ins[0].VT == MVT::i8) {
    SDValue V = DAG.getCopyFromReg(Chain, dl, I8085::A, MVT::i8, InFlag);
    Chain = V.getValue(1);
    InFlag = V.getValue(2);
    InVals.push_back(V.getValue(0));
    return Chain;
  }

  bool IsI64 =
      (!Ins.empty() && Ins[0].VT == MVT::i64) ||
      (Ins.size() == 2 && Ins[0].VT == MVT::i32 && Ins[1].VT == MVT::i32 &&
       (Ins[0].Flags.isSplit() || Ins[1].Flags.isSplit())) ||
      (Ins.size() == 2 &&
       Ins[0].VT == MVT::i32 && Ins[1].VT == MVT::i32);

  bool IsF32 =
      (Ins.size() == 1 && Ins[0].VT == MVT::f32);
  bool IsI32 =
      (Ins.size() == 1 && Ins[0].VT == MVT::i32) || IsF32;

  if (IsI64) {
    SDValue Lo = DAG.getCopyFromReg(Chain, dl, I8085::IAX, MVT::i32, InFlag);
    Chain = Lo.getValue(1);
    InFlag = Lo.getValue(2);
    SDValue Hi = DAG.getCopyFromReg(Chain, dl, I8085::IBX, MVT::i32, InFlag);
    Chain = Hi.getValue(1);
    InFlag = Hi.getValue(2);

    InVals.push_back(Lo.getValue(0));
    InVals.push_back(Hi.getValue(0));
    return Chain;
  }

  if (IsI32) {
    SDValue Lo = DAG.getCopyFromReg(Chain, dl, I8085::BC, MVT::i16, InFlag);
    Chain = Lo.getValue(1);
    InFlag = Lo.getValue(2);
    SDValue Hi = DAG.getCopyFromReg(Chain, dl, I8085::DE, MVT::i16, InFlag);
    Chain = Hi.getValue(1);
    InFlag = Hi.getValue(2);

    SDValue LoZ = DAG.getNode(ISD::ZERO_EXTEND, dl, MVT::i32, Lo);
    SDValue HiZ = DAG.getNode(ISD::ZERO_EXTEND, dl, MVT::i32, Hi);
    SDValue HiShift = DAG.getNode(ISD::SHL, dl, MVT::i32, HiZ,
                                  DAG.getConstant(16, dl, MVT::i32));
    SDValue Val = DAG.getNode(ISD::OR, dl, MVT::i32, LoZ, HiShift);

    if (IsF32)
      Val = DAG.getNode(ISD::BITCAST, dl, MVT::f32, Val);
    InVals.push_back(Val);
    return Chain;
  }

  // Assign locations to each value returned by this call.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());

  // Handle runtime calling convs.

  CCInfo.AnalyzeCallResult(Ins, RetCC_I8085_BUILTIN);

  SmallVector<std::pair<int, unsigned>, 4> ResultMemLocs;

  // Copy all of the result registers out of their specified physreg.
  for (CCValAssign const &RVLoc : RVLocs) {
    if(RVLoc.isRegLoc()){
    SDValue Copy = DAG.getCopyFromReg(Chain, dl, RVLoc.getLocReg(),
                                      RVLoc.getValVT(), InFlag);
    Chain = Copy.getValue(1);
    InFlag = Copy.getValue(2);
    InVals.push_back(Copy.getValue(0));
    }
    else{
      assert(RVLoc.isMemLoc() && "Must be memory location.");
      ResultMemLocs.push_back(
          std::make_pair(RVLoc.getLocMemOffset(), InVals.size()));
      // Reserve space for this result.
      InVals.push_back(SDValue());
    }
  }

    // Copy results out of memory.
  SmallVector<SDValue, 4> MemOpChains;
  for (unsigned i = 0, e = ResultMemLocs.size(); i != e; ++i) {
    int Offset = ResultMemLocs[i].first;
    unsigned Index = ResultMemLocs[i].second;
    
    SDValue ptrConstant = DAG.getConstant(Offset,dl,MVT::i16);
    SDValue Load = DAG.getLoad(MVT::i32, dl, Chain, ptrConstant, MachinePointerInfo());
    InVals[Index] = Load;
    MemOpChains.push_back(Load.getValue(1));
  }

  // Transform all loads nodes into one single node because
  // all load nodes are independent of each other.
  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, dl, MVT::Other, MemOpChains);

  return Chain;
}

//===----------------------------------------------------------------------===//
//               Return Value Calling Convention Implementation
//===----------------------------------------------------------------------===//

bool I8085TargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool isVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context) const {
  if (CallConv == CallingConv::I8085_BUILTIN) {
    for (const auto &Out : Outs) {
      if (Out.VT == MVT::f32)
        return true;
    }
    for (const auto &Out : Outs) {
      if (Out.VT == MVT::i64 || Out.Flags.isSplit())
        return true;
    }
    if (Outs.size() == 2 && Outs[0].VT == MVT::i32 &&
        Outs[1].VT == MVT::i32) {
      return true;
    }
    SmallVector<CCValAssign, 16> RVLocs;
    CCState CCInfo(CallConv, isVarArg, MF, RVLocs, Context);
    return CCInfo.CheckReturn(Outs, RetCC_I8085_BUILTIN);
  }

  bool IsI64 =
      (!Outs.empty() && Outs[0].VT == MVT::i64) ||
      (Outs.size() == 2 && Outs[0].VT == MVT::i32 && Outs[1].VT == MVT::i32);
  if (Outs.size() > 1 && !IsI64)
    return false;
  for (const auto &Out : Outs) {
    if (!(Out.VT.isInteger() || Out.VT == MVT::f32))
      return false;
  }

  unsigned TotalBytes = getTotalArgumentsSizeInBytes(Outs);
  return TotalBytes <= 8;
}

SDValue
I8085TargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                               bool isVarArg,
                               const SmallVectorImpl<ISD::OutputArg> &Outs,
                               const SmallVectorImpl<SDValue> &OutVals,
                               const SDLoc &dl, SelectionDAG &DAG) const {
  // CCValAssign - represent the assignment of the return value to locations.
  SmallVector<CCValAssign, 16> RVLocs;
  auto DL = DAG.getDataLayout();

  // CCState - Info about the registers and stack slot.
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());

  MachineFunction &MF = DAG.getMachineFunction();

  if (MF.getFunction().getAttributes().hasFnAttr(Attribute::Naked)) {
    return Chain;
  }

  if (MF.getFunction().getReturnType()->isIntegerTy(8) && !OutVals.empty()) {
    SDValue Val = OutVals[0];
    if (Val.getValueType() != MVT::i8)
      Val = DAG.getNode(ISD::TRUNCATE, dl, MVT::i8, Val);
    SDValue Flag;
    Chain = DAG.getCopyToReg(Chain, dl, I8085::A, Val, Flag);
    SmallVector<SDValue, 4> RetOps;
    RetOps.push_back(Chain);
    RetOps.push_back(DAG.getRegister(I8085::A, MVT::i8));
    if (Flag.getNode())
      RetOps.push_back(Flag);
    return DAG.getNode(I8085ISD::RET_FLAG, dl, MVT::Other, RetOps);
  }

  bool IsI64 =
      (!Outs.empty() && Outs[0].VT == MVT::i64) ||
      (Outs.size() == 2 && Outs[0].VT == MVT::i32 && Outs[1].VT == MVT::i32 &&
       (Outs[0].Flags.isSplit() || Outs[1].Flags.isSplit())) ||
      (Outs.size() == 2 && OutVals.size() == 2 &&
       OutVals[0].getValueType() == MVT::i32 &&
       OutVals[1].getValueType() == MVT::i32);

  bool IsF32 =
      (Outs.size() == 1 && Outs[0].VT == MVT::f32) ||
      (OutVals.size() == 1 && OutVals[0].getValueType() == MVT::f32);
  bool IsI32 =
      (Outs.size() == 1 && Outs[0].VT == MVT::i32) ||
      (OutVals.size() == 1 && OutVals[0].getValueType() == MVT::i32) ||
      IsF32;

  if (IsI64) {
    SDValue Lo;
    SDValue Hi;
    if (Outs.size() == 2) {
      Lo = OutVals[0];
      Hi = OutVals[1];
    } else {
      splitI64Value(OutVals[0], Lo, Hi, DAG, dl);
    }

    SDValue Flag;
    Chain = DAG.getCopyToReg(Chain, dl, I8085::IAX, Lo, Flag);
    Flag = Chain.getValue(1);
    Chain = DAG.getCopyToReg(Chain, dl, I8085::IBX, Hi, Flag);
    Flag = Chain.getValue(1);

    SmallVector<SDValue, 4> RetOps;
    RetOps.push_back(Chain);
    RetOps.push_back(DAG.getRegister(I8085::IAX, MVT::i32));
    RetOps.push_back(DAG.getRegister(I8085::IBX, MVT::i32));
    if (Flag.getNode())
      RetOps.push_back(Flag);

    return DAG.getNode(I8085ISD::RET_FLAG, dl, MVT::Other, RetOps);
  }

  if (IsI32) {
    SDValue Val = OutVals[0];
    if (Val.getValueType() == MVT::f32)
      Val = DAG.getNode(ISD::BITCAST, dl, MVT::i32, Val);
    SDValue Lo = DAG.getNode(ISD::TRUNCATE, dl, MVT::i16, Val);
    SDValue Hi = DAG.getNode(
        ISD::TRUNCATE, dl, MVT::i16,
        DAG.getNode(ISD::SRL, dl, MVT::i32, Val,
                    DAG.getConstant(16, dl, MVT::i32)));

    SDValue Flag;
    Chain = DAG.getCopyToReg(Chain, dl, I8085::BC, Lo, Flag);
    Flag = Chain.getValue(1);
    Chain = DAG.getCopyToReg(Chain, dl, I8085::DE, Hi, Flag);
    Flag = Chain.getValue(1);

    SmallVector<SDValue, 4> RetOps;
    RetOps.push_back(Chain);
    RetOps.push_back(DAG.getRegister(I8085::BC, MVT::i16));
    RetOps.push_back(DAG.getRegister(I8085::DE, MVT::i16));
    if (Flag.getNode())
      RetOps.push_back(Flag);

    return DAG.getNode(I8085ISD::RET_FLAG, dl, MVT::Other, RetOps);
  }

  CCInfo.AnalyzeReturn(Outs, RetCC_I8085_BUILTIN);

  MachineFrameInfo &MFI = MF.getFrameInfo();

  SDValue Flag;
  SmallVector<SDValue, 4> RetOps(1, Chain);
  // Copy the result values into the output registers.
  for (unsigned i = 0, e = RVLocs.size(); i != e; ++i) {
    CCValAssign &VA = RVLocs[i];
    if(VA.isRegLoc()){

      Chain = DAG.getCopyToReg(Chain, dl, VA.getLocReg(), OutVals[i], Flag);

      // Guarantee that all emitted copies are stuck together with flags.
      Flag = Chain.getValue(1);
      RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
    }
    else{
    int Offset = VA.getLocMemOffset();
    unsigned ObjSize = VA.getLocVT().getStoreSize();
    // Create the frame index object for the memory location.
    int FI = MFI.CreateFixedObject(ObjSize, Offset, false);

    // Create a SelectionDAG node corresponding to a store
    // to this memory location.
    SDValue FIN = DAG.getFrameIndex(FI, MVT::i16);
    if(ObjSize==8) FIN = DAG.getFrameIndex(FI, MVT::i8);


    Chain = DAG.getStore(Chain, dl, OutVals[i], FIN,MachinePointerInfo::getFixedStack(DAG.getMachineFunction(), FI));


    unsigned RetOpc = I8085ISD::RET_FLAG;  
    RetOps[0] = Chain; 

    if (Flag.getNode()) {
          RetOps.push_back(Flag);
    }

    return DAG.getNode(RetOpc, dl, MVT::Other, RetOps); 
    }
  }

  unsigned RetOpc = I8085ISD::RET_FLAG;

  RetOps[0] = Chain; // Update chain.

  if (Flag.getNode()) {
    RetOps.push_back(Flag);
  }

  return DAG.getNode(RetOpc, dl, MVT::Other, RetOps);
}

MachineBasicBlock *I8085TargetLowering::insertShiftSet(MachineInstr &MI,
                                                  MachineBasicBlock *MBB) const {

  int Opc = MI.getOpcode();
  const I8085InstrInfo &TII = (const I8085InstrInfo &)*MI.getParent()
                                ->getParent()
                                ->getSubtarget()
                                .getInstrInfo();

  DebugLoc dl = MI.getDebugLoc();

  // To "insert" a SELECT instruction, we insert the diamond
  // control-flow pattern. The incoming instruction knows the
  // destination vreg to set, the condition code register to branch
  // on, the true/false values to select between, and a branch opcode
  // to use.

  MachineFunction *MF = MBB->getParent();
  
  const BasicBlock *LLVM_BB = MBB->getBasicBlock();
  MachineBasicBlock *FallThrough = MBB->getFallThrough();

  // If the current basic block falls through to another basic block,
  // we must insert an unconditional branch to the fallthrough destination
  // if we are to insert basic blocks at the prior fallthrough point.
  if (FallThrough != nullptr) {
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(FallThrough);
  }

  MachineBasicBlock *continuationMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *shiftLoopMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *checkMBB = MF->CreateMachineBasicBlock(LLVM_BB);

  MachineFunction::iterator I;
  for (I = MF->begin(); I != MF->end() && &(*I) != MBB; ++I)
    ;
  if (I != MF->end())
    ++I;
  
  MF->insert(I, shiftLoopMBB);
  MF->insert(I, checkMBB);
  MF->insert(I, continuationMBB);


  // Transfer remaining instructions and all successors of the current
  // block to the block which will contain the Phi node for the
  // select.
  continuationMBB->splice(continuationMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());

  continuationMBB->transferSuccessorsAndUpdatePHIs(MBB);

  MBB->addSuccessor(checkMBB);
  shiftLoopMBB->addSuccessor(checkMBB);
  checkMBB->addSuccessor(shiftLoopMBB);
  checkMBB->addSuccessor(continuationMBB);

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned operandTwo = MI.getOperand(2).getReg();

  // Avoid A/HL here because GR32 shift pseudos clobber A/HL as scratch.
  unsigned counterTempReg =
      MF->getRegInfo().createVirtualRegister(&I8085::GR8NoAHLRegClass);
  unsigned counterTempReg2 =
      MF->getRegInfo().createVirtualRegister(&I8085::GR8NoAHLRegClass);
  unsigned shiftCount =
      MF->getRegInfo().createVirtualRegister(&I8085::GR8NoAHLRegClass);

  unsigned tempHolderOne,tempHolderTwo;
  int rrOpcode;
  if(Opc==I8085::SHL_16 || Opc==I8085::SRA_16 || Opc==I8085::SRL_16){
      rrOpcode = I8085::RR_16;
      if(Opc==I8085::SHL_16) rrOpcode=I8085::RL_16;
      if(Opc==I8085::SRA_16) rrOpcode=I8085::ASR_16;
      tempHolderOne = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i16));
      tempHolderTwo = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i16));
  }

  if(Opc==I8085::SHL_8 || Opc==I8085::SRA_8 || Opc==I8085::SRL_8){
      rrOpcode = I8085::RR_8;
      if(Opc==I8085::SHL_8) rrOpcode=I8085::RL_8;
      if(Opc==I8085::SRA_8) rrOpcode=I8085::ASR_8;
      tempHolderOne = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));
      tempHolderTwo = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));
  }

  if(Opc==I8085::SHL_32 || Opc==I8085::SRA_32 || Opc==I8085::SRL_32){
      rrOpcode = I8085::RR_32;
      if(Opc==I8085::SHL_32) rrOpcode=I8085::RL_32;
      if(Opc==I8085::SRA_32) rrOpcode=I8085::ASR_32;
      tempHolderOne = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i32));
      tempHolderTwo = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i32));
  }

  //MBB:
  // Jump to loop MBB

  BuildMI(MBB, dl, TII.get(TargetOpcode::COPY), shiftCount)
      .addReg(operandTwo);

  BuildMI(MBB, dl, TII.get(I8085::JMP))
     .addMBB(checkMBB);

  // LoopBB:
  // tempHolderTwo = shift tempHolderOne

  BuildMI(shiftLoopMBB, dl, TII.get(rrOpcode))
        .addReg(tempHolderTwo, RegState::Define)
        .addReg(tempHolderOne);

  // checkMBB:
  // tempHolderOne = phi [%operandOne, BB], [%tempHolderTwo, LoopBB]
  // counterTempReg2 = phi [%operandTwo, BB], [%counterTempReg, LoopBB]
  // destReg  = phi [%operandOne, BB], [%tempHolderTwo,  LoopBB]
  // counterTempReg = counterTempReg2 - 1;
  // if (counterTempReg >= 0) goto shiftLoopMBB;
  
  BuildMI(checkMBB, dl, TII.get(I8085::PHI), tempHolderOne)
      .addReg(operandOne)
      .addMBB(MBB)
      .addReg(tempHolderTwo)
      .addMBB(shiftLoopMBB);

  BuildMI(checkMBB, dl, TII.get(I8085::PHI), counterTempReg2)
      .addReg(shiftCount)
      .addMBB(MBB)
      .addReg(counterTempReg)
      .addMBB(shiftLoopMBB);

  BuildMI(checkMBB, dl, TII.get(I8085::PHI), destReg)
      .addReg(operandOne)
      .addMBB(MBB)
      .addReg(tempHolderTwo)
      .addMBB(shiftLoopMBB);

  BuildMI(checkMBB, dl, TII.get(I8085::DCR))
      .addReg(counterTempReg,RegState::Define)
      .addReg(counterTempReg2);

  BuildMI(checkMBB, dl, TII.get(I8085::JP))
            .addMBB(shiftLoopMBB);      

  MI.eraseFromParent();
  return continuationMBB;
}

static MachineBasicBlock *insertSelectPseudo(MachineInstr &MI,
                                             MachineBasicBlock *MBB) {
  const I8085InstrInfo &TII = (const I8085InstrInfo &)*MI.getParent()
                                ->getParent()
                                ->getSubtarget()
                                .getInstrInfo();
  DebugLoc dl = MI.getDebugLoc();

  MachineFunction *MF = MBB->getParent();
  const BasicBlock *LLVM_BB = MBB->getBasicBlock();
  MachineBasicBlock *FallThrough = MBB->getFallThrough();

  if (FallThrough != nullptr) {
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(FallThrough);
  }

  MachineBasicBlock *trueMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *falseMBB = MF->CreateMachineBasicBlock(LLVM_BB);

  MachineFunction::iterator I;
  for (I = MF->begin(); I != MF->end() && &(*I) != MBB; ++I)
    ;
  if (I != MF->end())
    ++I;
  MF->insert(I, trueMBB);
  MF->insert(I, falseMBB);

  trueMBB->splice(trueMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());
  trueMBB->transferSuccessorsAndUpdatePHIs(MBB);

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned condReg = MI.getOperand(1).getReg();
  unsigned trueReg = MI.getOperand(2).getReg();
  unsigned falseReg = MI.getOperand(3).getReg();

  // Materialize the condition into A immediately before branching so
  // intervening GR32 pseudo expansions cannot clobber it.
  BuildMI(MBB, dl, TII.get(I8085::MOV))
      .addReg(I8085::A, RegState::Define)
      .addReg(condReg);
  BuildMI(MBB, dl, TII.get(I8085::ORI)).addImm(0);
  BuildMI(MBB, dl, TII.get(I8085::JNZ)).addMBB(trueMBB);
  BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(falseMBB);

  MBB->addSuccessor(falseMBB);
  MBB->addSuccessor(trueMBB);

  BuildMI(falseMBB, dl, TII.get(I8085::JMP)).addMBB(trueMBB);
  falseMBB->addSuccessor(trueMBB);

  BuildMI(*trueMBB, trueMBB->begin(), dl, TII.get(I8085::PHI), destReg)
      .addReg(trueReg)
      .addMBB(MBB)
      .addReg(falseReg)
      .addMBB(falseMBB);

  MI.eraseFromParent();
  return trueMBB;
}


MachineBasicBlock *I8085TargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                               MachineBasicBlock *MBB) const {
  int Opc = MI.getOpcode();

  // Pseudo shift instructions with a non constant shift amount are expanded
  // into a loop.
  switch (Opc) {
  case I8085::SET_NE_8:
  case I8085::SET_EQ_8:
  case I8085::SET_UGT_8:
  case I8085::SET_ULT_8:
  case I8085::SET_UGE_8:
  case I8085::SET_ULE_8:
    return insertCond8Set(MI, MBB);
  case I8085::SET_GT_8:
  case I8085::SET_LT_8:
  case I8085::SET_GE_8:
  case I8085::SET_LE_8:
    return insertSigned8Cond(MI,MBB); 
  case I8085::SET_DIFF_SIGN_GT_8:
  case I8085::SET_DIFF_SIGN_LT_8:
  case I8085::SET_DIFF_SIGN_GE_8:
  case I8085::SET_DIFF_SIGN_LE_8:
    return insertDifferentSigned8Cond(MI,MBB);

  case I8085::SET_NE_16:
  case I8085::SET_EQ_16:
    return insertEqualityCond16Set(MI,MBB);

  case I8085::SET_UGT_16:
  case I8085::SET_ULT_16:
  case I8085::SET_UGE_16:
  case I8085::SET_ULE_16:
    return insertCond16Set(MI, MBB); 

  case I8085::SET_GT_16:
  case I8085::SET_LT_16:
  case I8085::SET_GE_16:
  case I8085::SET_LE_16:
    return insertSignedCond16Set(MI, MBB);   

  case I8085::SET_DIFF_SIGN_LT_16:
  case I8085::SET_DIFF_SIGN_GT_16:
    return insertDifferentSignedCond16Set(MI, MBB); 
  

  case I8085::SET_NE_32:
  case I8085::SET_EQ_32:
    return insertEqualityCond32Set(MI,MBB);
  

  case I8085::SET_UGT_32:
  case I8085::SET_ULT_32:
  case I8085::SET_UGE_32:
  case I8085::SET_ULE_32:
    return insertCond32Set(MI, MBB); 
  
  case I8085::SET_GT_32:
  case I8085::SET_LT_32:
  case I8085::SET_GE_32:
  case I8085::SET_LE_32:
    return insertSignedCond32Set(MI, MBB); 


  case I8085::SET_DIFF_SIGN_LT_32:
  case I8085::SET_DIFF_SIGN_GT_32:
    return insertDifferentSignedCond32Set(MI, MBB);   

  case I8085::SELECT_8:
  case I8085::SELECT_16:
  case I8085::SELECT_32:
    return insertSelectPseudo(MI, MBB);

  case I8085::SHL_8:
  case I8085::SRA_8:
  case I8085::SRL_8:
  case I8085::SHL_16:
  case I8085::SRA_16:
  case I8085::SRL_16:
  case I8085::SHL_32:
  case I8085::SRA_32:
  case I8085::SRL_32:
    return insertShiftSet(MI, MBB);      
  }

  assert( false &&
         "Unexpected instr type to insert");
  return MBB;
}


Register I8085TargetLowering::getRegisterByName(const char *RegName, LLT VT,
                                              const MachineFunction &MF) const {
  Register Reg;

  if (VT == LLT::scalar(8)) {
    Reg = StringSwitch<unsigned>(RegName)
              .Default(0);
  } else {
    Reg = StringSwitch<unsigned>(RegName)
              .Default(0);
  }

  if (Reg)
    return Reg;

  report_fatal_error(
      Twine("Invalid register name \"" + StringRef(RegName) + "\"."));
}

} 
