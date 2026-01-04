//===-- TMS9900InstrInfo.cpp - TMS9900 Instruction Information ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the TMS9900 implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "TMS9900InstrInfo.h"
#include "TMS9900.h"
#include "TMS9900Subtarget.h"
#include "TMS9900TargetMachine.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "tms9900-instr-info"

#define GET_INSTRINFO_CTOR_DTOR
#include "TMS9900GenInstrInfo.inc"

TMS9900InstrInfo::TMS9900InstrInfo(const TMS9900Subtarget &STI)
    : TMS9900GenInstrInfo(TMS9900::ADJCALLSTACKDOWN, TMS9900::ADJCALLSTACKUP),
      RI(STI), Subtarget(STI) {}

void TMS9900InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator I,
                                    const DebugLoc &DL, MCRegister DestReg,
                                    MCRegister SrcReg, bool KillSrc) const {
  // Use MOV instruction for register copy
  // MOV Rs,Rd copies Rs to Rd
  BuildMI(MBB, I, DL, get(TMS9900::MOVrr), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc));
}

void TMS9900InstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
    bool isKill, int FrameIndex, const TargetRegisterClass *RC,
    const TargetRegisterInfo *TRI, Register VReg) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOStore, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));

  // Use MOV_FI_Store: MOV Rs,@offset(Ri) where Ri will be R10 (SP)
  // Operands: (base register, offset, source register)
  // The frame index will be eliminated later by eliminateFrameIndex
  BuildMI(MBB, MI, DL, get(TMS9900::MOV_FI_Store))
      .addFrameIndex(FrameIndex)  // Will become R10
      .addImm(0)                  // Offset (will be resolved by eliminateFrameIndex)
      .addReg(SrcReg, getKillRegState(isKill))
      .addMemOperand(MMO);
}

void TMS9900InstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register DestReg,
    int FrameIndex, const TargetRegisterClass *RC,
    const TargetRegisterInfo *TRI, Register VReg) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOLoad, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));

  // Use MOV_FI_Load: MOV @offset(Ri),Rd where Ri will be R10 (SP)
  // Operands: (destination register, base register, offset)
  // The frame index will be eliminated later by eliminateFrameIndex
  BuildMI(MBB, MI, DL, get(TMS9900::MOV_FI_Load), DestReg)
      .addFrameIndex(FrameIndex)  // Will become R10
      .addImm(0)                  // Offset (will be resolved by eliminateFrameIndex)
      .addMemOperand(MMO);
}

bool TMS9900InstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                      MachineBasicBlock *&TBB,
                                      MachineBasicBlock *&FBB,
                                      SmallVectorImpl<MachineOperand> &Cond,
                                      bool AllowModify) const {
  // Start from the bottom of the block and work up
  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;

    if (I->isDebugInstr())
      continue;

    // If we see a non-branch, we're done
    if (!I->isBranch())
      break;

    // Handle unconditional branch
    if (I->getOpcode() == TMS9900::JMP) {
      if (TBB == nullptr) {
        TBB = I->getOperand(0).getMBB();
        continue;
      }
      // Already have a target, this is unreachable code
      return true;
    }

    // Handle conditional branches
    // TODO: Implement conditional branch analysis
    return true;
  }

  return false;
}

unsigned TMS9900InstrInfo::insertBranch(MachineBasicBlock &MBB,
                                         MachineBasicBlock *TBB,
                                         MachineBasicBlock *FBB,
                                         ArrayRef<MachineOperand> Cond,
                                         const DebugLoc &DL,
                                         int *BytesAdded) const {
  assert(TBB && "insertBranch must have a target");
  assert((Cond.size() == 0 || Cond.size() == 1) &&
         "TMS9900 branch conditions have zero or one component");

  if (Cond.empty()) {
    // Unconditional branch
    BuildMI(&MBB, DL, get(TMS9900::JMP)).addMBB(TBB);
    if (BytesAdded)
      *BytesAdded = 2;
    return 1;
  }

  // Conditional branch
  // TODO: Implement based on condition
  // For now, just insert unconditional
  BuildMI(&MBB, DL, get(TMS9900::JMP)).addMBB(TBB);
  if (BytesAdded)
    *BytesAdded = 2;
  return 1;
}

unsigned TMS9900InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                         int *BytesRemoved) const {
  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;

  while (I != MBB.begin()) {
    --I;

    if (I->isDebugInstr())
      continue;

    if (!I->isBranch())
      break;

    // Remove the branch
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }

  if (BytesRemoved)
    *BytesRemoved = Count * 2;  // Each branch is 2 bytes (may be wrong for some)

  return Count;
}

bool TMS9900InstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  // TODO: Implement condition reversal
  // JEQ <-> JNE
  // JGT <-> JLE
  // JLT <-> JGE (using JHE?)
  // etc.
  return true;  // Return true to indicate we can't reverse (for now)
}

bool TMS9900InstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();

  switch (MI.getOpcode()) {
  default:
    return false;
  case TMS9900::RET:
    // Expand RET pseudo to B *R11
    BuildMI(MBB, MI, DL, get(TMS9900::RET_REAL));
    MBB.erase(MI);
    return true;
  case TMS9900::ANDrr: {
    // Expand ANDrr pseudo to INV+SZC+INV sequence
    // AND rd, rs2 becomes:
    //   INV rs2      ; rs2 = NOT rs2
    //   SZC rs2, rd  ; rd = rd AND (NOT rs2) = rd AND (NOT (NOT original_rs2)) = rd AND original_rs2
    //   INV rs2      ; restore rs2
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(2).getReg();

    // INV rs2
    BuildMI(MBB, MI, DL, get(TMS9900::INVr), SrcReg)
        .addReg(SrcReg);
    // SZC rs2, rd
    BuildMI(MBB, MI, DL, get(TMS9900::SZCrr), DstReg)
        .addReg(DstReg)
        .addReg(SrcReg);
    // INV rs2 (restore)
    BuildMI(MBB, MI, DL, get(TMS9900::INVr), SrcReg)
        .addReg(SrcReg);

    MBB.erase(MI);
    return true;
  }
  }
}
