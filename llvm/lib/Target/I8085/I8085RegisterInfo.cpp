//===-- I8085RegisterInfo.cpp - I8085 Register Information --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the I8085 implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "I8085RegisterInfo.h"

#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/IR/Function.h"

#include "I8085.h"
#include "I8085InstrInfo.h"
#include "I8085MachineFunctionInfo.h"
#include "I8085TargetMachine.h"
#include "MCTargetDesc/I8085MCTargetDesc.h"


#include <iostream>

#define GET_REGINFO_TARGET_DESC
#include "I8085GenRegisterInfo.inc"

namespace llvm {

I8085RegisterInfo::I8085RegisterInfo() : I8085GenRegisterInfo(0) {}

static bool needsBasePointer(const MachineFunction &MF) {
  return MF.getFrameInfo().hasVarSizedObjects();
}

const uint16_t *
I8085RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return CSR_Normal_SaveList;
}

const uint32_t *
I8085RegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                      CallingConv::ID CC) const {
  return CSR_Normal_RegMask;
}

BitVector I8085RegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());

  Reserved.set(I8085::SP);
  Reserved.set(I8085::PC);

  Reserved.set(I8085::M);

  if (needsBasePointer(MF)) {
    Reserved.set(I8085::DE);
    Reserved.set(I8085::D);
    Reserved.set(I8085::E);
  }

  // GR32 pseudos use HL as a scratch address register after regalloc.
  // Reserve HL when IAX/IBX are present to avoid clobbering live values.
  bool UsesGR32 = false;
  for (const auto &MBB : MF) {
    for (const auto &MI : MBB) {
      for (const auto &MO : MI.operands()) {
        if (!MO.isReg())
          continue;
        Register Reg = MO.getReg();
        if (Reg == I8085::IAX || Reg == I8085::IBX) {
          UsesGR32 = true;
          break;
        }
      }
      if (UsesGR32)
        break;
    }
    if (UsesGR32)
      break;
  }
  if (UsesGR32) {
    Reserved.set(I8085::A);
    Reserved.set(I8085::HL);
    Reserved.set(I8085::H);
    Reserved.set(I8085::L);
  }

  return Reserved;
}

const TargetRegisterClass *
I8085RegisterInfo::getLargestLegalSuperClass(const TargetRegisterClass *RC,
                                           const MachineFunction &MF) const {
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();

  if (RC == &I8085::GR16BDRegClass || RC == &I8085::GR16BDSPRegClass)
    return RC;

  if (TRI->isTypeLegalForClass(*RC, MVT::i16)) {
    return &I8085::GR16RegClass;
  }

  if (TRI->isTypeLegalForClass(*RC, MVT::i8)) {
    return &I8085::GR8RegClass;
  }
  
  if (TRI->isTypeLegalForClass(*RC, MVT::i32)) {
    return &I8085::GR32RegClass;
  }

  llvm_unreachable("Invalid register size");
}

/// Fold a frame offset shared between two add instructions into a single one.


bool I8085RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                          int SPAdj, unsigned FIOperandNum,
                                          RegScavenger *RS) const {
  MachineInstr &MI = *II;
  DebugLoc dl = MI.getDebugLoc();
  MachineBasicBlock &MBB = *MI.getParent();
  const MachineFunction &MF = *MBB.getParent();
  const I8085TargetMachine &TM = (const I8085TargetMachine &)MF.getTarget();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetFrameLowering *TFI = TM.getSubtargetImpl()->getFrameLowering();
  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  int Offset = MFI.getObjectOffset(FrameIndex);

  Offset += MFI.getStackSize() - TFI->getOffsetOfLocalArea();
  // Fold incoming offset.
  Offset += MI.getOperand(FIOperandNum + 1).getImm();
  // Account for any mid-function stack pointer adjustment (e.g., outgoing args).
  Offset += SPAdj;

  

  // If the offset is too big we have to adjust and restore the frame pointer
  // to materialize a valid load/store with displacement.
  //: TODO: consider using only one adiw/sbiw chain for more than one frame
  //: index

  unsigned BaseReg = needsBasePointer(MF) ? I8085::DE : I8085::SP;
  MI.getOperand(FIOperandNum).ChangeToRegister(BaseReg, false);
  assert(isInt<16>(Offset) && "Offset is out of range");
  MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);

  return false;
}

Register I8085RegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return needsBasePointer(MF) ? I8085::DE : I8085::SP;
}

// const TargetRegisterClass *
// I8085RegisterInfo::getPointerRegClass(const MachineFunction &MF,
//                                     unsigned Kind) const {
//   return &I8085::GRSP;
// }

void I8085RegisterInfo::splitReg(Register Reg, Register &LoReg,
                               Register &HiReg) const {                        

  LoReg = getSubReg(Reg, I8085::sub_lo);
  HiReg = getSubReg(Reg, I8085::sub_hi);
}

bool I8085RegisterInfo::shouldCoalesce(
    MachineInstr *MI, const TargetRegisterClass *SrcRC, unsigned SubReg,
    const TargetRegisterClass *DstRC, unsigned DstSubReg,
    const TargetRegisterClass *NewRC, LiveIntervals &LIS) const {
  // Prevent widening GR16BD copies into GR16, which can reintroduce HL for
  // base-address uses that must stay in BC/DE.
  if ((SrcRC == &I8085::GR16BDRegClass ||
       DstRC == &I8085::GR16BDRegClass ||
       SrcRC == &I8085::GR16BDSPRegClass ||
       DstRC == &I8085::GR16BDSPRegClass) &&
      NewRC == &I8085::GR16RegClass) {
    return false;
  }

  return TargetRegisterInfo::shouldCoalesce(
      MI, SrcRC, SubReg, DstRC, DstSubReg, NewRC, LIS);
}

} // end of namespace llvm
