//===-- I8085FrameLowering.cpp - I8085 Frame Information ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the I8085 implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#include "I8085FrameLowering.h"

#include "I8085.h"
#include "I8085InstrInfo.h"
#include "I8085MachineFunctionInfo.h"
#include "I8085TargetMachine.h"
#include "MCTargetDesc/I8085MCTargetDesc.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDwarf.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"

#include <vector>
#include <iostream>

namespace llvm {

I8085FrameLowering::I8085FrameLowering()
    : TargetFrameLowering(TargetFrameLowering::StackGrowsDown, Align(1), -2) {}

static Align getStackRealignAlignment(const MachineFunction &MF) {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  Align MaxAlign = MFI.getMaxAlign();
  Align StackAlign = MF.getSubtarget<I8085Subtarget>()
                         .getFrameLowering()
                         ->getStackAlign();
  if (MaxAlign <= StackAlign)
    return StackAlign;

  return MaxAlign;
}

static bool needsStackRealign(const MachineFunction &MF) {
  Align Realign = getStackRealignAlignment(MF);
  Align StackAlign = MF.getSubtarget<I8085Subtarget>()
                         .getFrameLowering()
                         ->getStackAlign();
  return Realign > StackAlign;
}

static bool getPairRegs(unsigned Pair, unsigned &LowReg, unsigned &HighReg) {
  switch (Pair) {
  case I8085::BC:
    LowReg = I8085::C;
    HighReg = I8085::B;
    return true;
  case I8085::DE:
    LowReg = I8085::E;
    HighReg = I8085::D;
    return true;
  default:
    return false;
  }
}

static void buildCFI(MachineBasicBlock &MBB,
                     MachineBasicBlock::iterator MBBI, const DebugLoc &DL,
                     const MCCFIInstruction &CFIInst,
                     MachineInstr::MIFlag Flag, const TargetInstrInfo &TII) {
  MachineFunction &MF = *MBB.getParent();
  unsigned CFIIndex = MF.addFrameInst(CFIInst);
  BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
      .addCFIIndex(CFIIndex)
      .setMIFlag(Flag);
}

static void emitCalleeSavedFrameMoves(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MBBI,
                                      const DebugLoc &DL, bool IsPrologue,
                                      const TargetInstrInfo &TII) {
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
  const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();

  for (const CalleeSavedInfo &I : CSI) {
    int64_t Offset = MFI.getObjectOffset(I.getFrameIdx());
    unsigned Reg = I.getReg();
    unsigned DwarfReg = MRI->getDwarfRegNum(Reg, true);
    if (IsPrologue) {
      buildCFI(MBB, MBBI, DL,
               MCCFIInstruction::createOffset(nullptr, DwarfReg, Offset),
               MachineInstr::FrameSetup, TII);
    } else {
      buildCFI(MBB, MBBI, DL,
               MCCFIInstruction::createRestore(nullptr, DwarfReg),
               MachineInstr::FrameDestroy, TII);
    }
  }
}

bool I8085FrameLowering::canSimplifyCallFramePseudos(
    const MachineFunction &MF) const {
  // Always simplify call frame pseudo instructions, even when
  // hasReservedCallFrame is false.
  return true;
}

bool I8085FrameLowering::hasReservedCallFrame(const MachineFunction &MF) const {
  // Reserve call frame memory in function prologue under the following
  // conditions:
  // - Y pointer is reserved to be the frame pointer.
  // - The function does not contain variable sized objects.

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return hasFP(MF) && !MFI.hasVarSizedObjects();
}

void I8085FrameLowering::emitPrologue(MachineFunction &MF,
                                    MachineBasicBlock &MBB) const {
  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL = (MBBI != MBB.end()) ? MBBI->getDebugLoc() : DebugLoc();
  const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
  const I8085InstrInfo &TII = *STI.getInstrInfo();
  const I8085MachineFunctionInfo *AFI = MF.getInfo<I8085MachineFunctionInfo>();
  bool HasFP = hasFP(MF);
  bool EmitCFI = MF.needsFrameMoves() && !AFI->hasStackRealign();

  // Early exit if the frame pointer is not needed in this function.
  if (!HasFP) {
    if (!EmitCFI)
      return;
    // Emit callee-saved CFI even in frameless functions.
    MachineBasicBlock::iterator CFIInsert = MBB.begin();
    while (CFIInsert != MBB.end() &&
           CFIInsert->getFlag(MachineInstr::FrameSetup)) {
      ++CFIInsert;
    }
    emitCalleeSavedFrameMoves(MBB, CFIInsert, DL, true, TII);
    return;
  }

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  unsigned FrameSize = MFI.getStackSize() - AFI->getCalleeSavedFrameSize();
  // Realignment establishes an aligned CFA origin, not the entry SP (which
  // is two bytes below its CFA). Keep frame-index offsets relative to that
  // origin, including the local-area and callee-save portions of the layout.
  if (AFI->hasStackRealign())
    FrameSize = MFI.getStackSize() - getOffsetOfLocalArea();

  // Skip the callee-saved push instructions.
  while (
      (MBBI != MBB.end()) && MBBI->getFlag(MachineInstr::FrameSetup)) {
    ++MBBI;
  }
  
  /*Prologue sequence for 8085 processor */ 
  bool NeedsBasePtr = MFI.hasVarSizedObjects();
  bool SaveIncomingBC = AFI->hasStackRealign() &&
      llvm::any_of(AFI->getIncomingRegSaveSlots(), [](auto Slot) {
        return Slot.first == I8085::BC || Slot.first == I8085::C;
      });
  if (SaveIncomingBC)
    BuildMI(MBB, MBBI, DL, TII.get(I8085::PUSH)).addReg(I8085::BC);
  if (AFI->hasStackRealign()) {
    Align Realign = getStackRealignAlignment(MF);
    unsigned AlignBytes = Realign.value();
    if (!isPowerOf2_32(AlignBytes) || AlignBytes > 256)
      report_fatal_error("Unsupported I8085 stack realignment");

    uint8_t Mask = static_cast<uint8_t>(~(AlignBytes - 1u));
    unsigned ScratchPair = I8085::BC;
    unsigned ScratchLow = 0, ScratchHigh = 0;
    if (!getPairRegs(ScratchPair, ScratchLow, ScratchHigh))
      report_fatal_error("I8085 stack realignment scratch pair invalid");

    BuildMI(MBB, MBBI, DL, TII.get(I8085::LXI), I8085::HL).addImm(0);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::DAD)).addReg(I8085::SP);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), ScratchHigh)
        .addReg(I8085::H);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), ScratchLow)
        .addReg(I8085::L);
    if (SaveIncomingBC) {
      BuildMI(MBB, MBBI, DL, TII.get(I8085::INX), I8085::BC);
      BuildMI(MBB, MBBI, DL, TII.get(I8085::INX), I8085::BC);
    }
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::A)
        .addReg(I8085::L);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::ANI)).addImm(Mask);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::L)
        .addReg(I8085::A);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::SPHL));
  }

  if(FrameSize) {
      BuildMI(MBB, MBBI, DL, TII.get(I8085::GROW_STACK_BY))
          .addImm(FrameSize)
          .setMIFlag(MachineInstr::FrameSetup);
      if (EmitCFI) {
        buildCFI(MBB, MBBI, DL,
                 MCCFIInstruction::createAdjustCfaOffset(nullptr, FrameSize),
                 MachineInstr::FrameSetup, TII);
      }

  }

  auto FrameOffset = [&](int FI) {
    return MFI.getObjectOffset(FI) + MFI.getStackSize() - getOffsetOfLocalArea();
  };
  if (AFI->hasStackRealignSaveFI()) {
    BuildMI(MBB, MBBI, DL, TII.get(I8085::STORE_16))
        .addReg(I8085::SP)
        .addImm(FrameOffset(AFI->getStackRealignSaveFI()))
        .addReg(I8085::BC);
  }
  if (SaveIncomingBC) {
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::H).addReg(I8085::B);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::L).addReg(I8085::C);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::DCX), I8085::HL);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::DCX), I8085::HL);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV_FROM_M), I8085::A);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::INX), I8085::HL);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV_FROM_M), I8085::B);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::C).addReg(I8085::A);
  }
  for (auto [Reg, FI] : AFI->getIncomingRegSaveSlots()) {
    MBB.addLiveIn(Reg);
    unsigned Opc = MFI.getObjectSize(FI) == 1 ? I8085::STORE_8 : I8085::STORE_16;
    BuildMI(MBB, MBBI, DL, TII.get(Opc))
        .addReg(I8085::SP).addImm(FrameOffset(FI)).addReg(Reg);
  }

  if (NeedsBasePtr) {
    BuildMI(MBB, MBBI, DL, TII.get(I8085::LXI), I8085::HL).addImm(0);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::DAD)).addReg(I8085::SP);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::D)
        .addReg(I8085::H);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::E)
        .addReg(I8085::L);
  }

  if (EmitCFI) {
    emitCalleeSavedFrameMoves(MBB, MBBI, DL, true, TII);
  }
}

static void restoreStatusRegister(MachineFunction &MF, MachineBasicBlock &MBB) {
}

void I8085FrameLowering::emitEpilogue(MachineFunction &MF,
                                    MachineBasicBlock &MBB) const {
  const I8085MachineFunctionInfo *AFI = MF.getInfo<I8085MachineFunctionInfo>();
  bool EmitCFI = MF.needsFrameMoves() && !AFI->hasStackRealign();

  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  assert(MBBI->getDesc().isReturn() &&
         "Can only insert epilog into returning blocks");

  DebugLoc DL = MBBI->getDebugLoc();
  const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
  const I8085InstrInfo &TII = *STI.getInstrInfo();

  // Early exit if the frame pointer is not needed in this function except for
  // signal/interrupt handlers where special code generation is required.
  if (!hasFP(MF) && !AFI->isInterruptOrSignalHandler()) {
    if (!EmitCFI)
      return;

    // Find the first callee-saved pop so we can insert restores after it.
    MachineBasicBlock::iterator FirstCSPop = MBBI;
    MachineBasicBlock::iterator Scan = MBBI;
    while (Scan != MBB.begin()) {
      MachineBasicBlock::iterator PI = std::prev(Scan);
      int Opc = PI->getOpcode();
      if ((Opc != I8085::POP || !PI->getFlag(MachineInstr::FrameDestroy)) &&
          !PI->isTerminator())
        break;
      FirstCSPop = PI;
      --Scan;
    }

    MachineBasicBlock::iterator AfterPop = FirstCSPop;
    while (AfterPop != MBB.end() &&
           AfterPop->getFlag(MachineInstr::FrameDestroy) &&
           AfterPop->getOpcode() == I8085::POP) {
      ++AfterPop;
    }

    emitCalleeSavedFrameMoves(MBB, AfterPop, DL, false, TII);
    return;
  }

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  unsigned FrameSize = MFI.getStackSize() - AFI->getCalleeSavedFrameSize();

  // Find the first callee-saved pop so we can insert adjustments before it.
  MachineBasicBlock::iterator FirstCSPop = MBBI;
  while (MBBI != MBB.begin()) {
    MachineBasicBlock::iterator PI = std::prev(MBBI);
    int Opc = PI->getOpcode();
    if ((Opc != I8085::POP || !PI->getFlag(MachineInstr::FrameDestroy)) &&
        !PI->isTerminator())
      break;
    FirstCSPop = PI;
    --MBBI;
  }
  MBBI = FirstCSPop;
  DL = MBBI->getDebugLoc();

  // An i32 return overwrites DE, which is also the fixed-frame base in a VLA
  // function. Return lowering places this marker before materializing BC:DE.
  // Save the base on the dynamic stack and recover it after the return value
  // is ready, avoiding assumptions about the register allocator's copy shape.
  bool BaseSavedForReturn = false;
  if (MFI.hasVarSizedObjects()) {
    MachineBasicBlock::iterator SaveMarker = MBB.end();
    for (auto I = MBB.begin(); I != MBBI; ++I) {
      if (I->getOpcode() != I8085::SAVE_VLA_BASE_FOR_RET)
        continue;
      assert(SaveMarker == MBB.end() &&
             "multiple VLA base-save markers in one return block");
      SaveMarker = I;
    }
    if (SaveMarker != MBB.end()) {
      BuildMI(MBB, SaveMarker, SaveMarker->getDebugLoc(), TII.get(I8085::PUSH))
          .addReg(I8085::DE);
      MBB.erase(SaveMarker);
      BaseSavedForReturn = true;
    }
  }

  if (AFI->hasStackRealignSaveFI()) {
    if (BaseSavedForReturn) {
      // Recover the fixed-frame base without clobbering either half of the
      // BC:DE return value, then load the pre-realignment SP directly via HL.
      BuildMI(MBB, MBBI, DL, TII.get(I8085::POP), I8085::HL);

      int64_t Offset = MFI.getObjectOffset(AFI->getStackRealignSaveFI());
      Offset += MFI.getStackSize() - getOffsetOfLocalArea();
      if (!isInt<16>(Offset))
        report_fatal_error("I8085 stack-realignment save offset out of range");
      if (Offset != 0) {
        uint16_t EncodedOffset = static_cast<uint16_t>(Offset);
        BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::A).addReg(I8085::L);
        BuildMI(MBB, MBBI, DL, TII.get(I8085::ADI))
            .addImm(EncodedOffset & 0xff);
        BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::L).addReg(I8085::A);
        BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::A).addReg(I8085::H);
        BuildMI(MBB, MBBI, DL, TII.get(I8085::ACI)).addImm(EncodedOffset >> 8);
        BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::H).addReg(I8085::A);
      }
      BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV_FROM_M), I8085::A);
      BuildMI(MBB, MBBI, DL, TII.get(I8085::INX), I8085::HL)
          .addReg(I8085::HL, RegState::Implicit);
      BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV_FROM_M), I8085::H);
      BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::L).addReg(I8085::A);
    } else {
      BuildMI(MBB, MBBI, DL, TII.get(I8085::LOAD_16_WITH_ADDR), I8085::HL)
          .addFrameIndex(AFI->getStackRealignSaveFI())
          .addImm(0);
    }
    BuildMI(MBB, MBBI, DL, TII.get(I8085::SPHL));
    restoreStatusRegister(MF, MBB);
    return;
  }

  if (MFI.hasVarSizedObjects()) {
    if (BaseSavedForReturn) {
      BuildMI(MBB, MBBI, DL, TII.get(I8085::POP), I8085::HL);
    } else {
      BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::H).addReg(I8085::D);
      BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::L).addReg(I8085::E);
    }
    BuildMI(MBB, MBBI, DL, TII.get(I8085::SPHL));
  }

  if (FrameSize) {
    BuildMI(MBB, MBBI, DL, TII.get(I8085::SHRINK_STACK_BY))
        .addImm(FrameSize)
        .setMIFlag(MachineInstr::FrameDestroy);
    if (EmitCFI) {
      buildCFI(MBB, MBBI, DL,
               MCCFIInstruction::createAdjustCfaOffset(
                   nullptr, -static_cast<int64_t>(FrameSize)),
               MachineInstr::FrameDestroy, TII);
    }
  }


  // Write back R29R28 to SP and temporarily disable interrupts.
  // BuildMI(MBB, MBBI, DL, TII.get(I8085::SPWRITE), I8085::SP)
  //     .addReg(I8085::D, RegState::Kill);

  restoreStatusRegister(MF, MBB);

  if (!EmitCFI)
    return;

  MachineBasicBlock::iterator AfterPop = FirstCSPop;
  while (AfterPop != MBB.end() && AfterPop->getFlag(MachineInstr::FrameDestroy) &&
         AfterPop->getOpcode() == I8085::POP) {
    ++AfterPop;
  }

  emitCalleeSavedFrameMoves(MBB, AfterPop, DL, false, TII);
}

void I8085FrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *RS) const {
  (void)RS;
  I8085MachineFunctionInfo *FuncInfo = MF.getInfo<I8085MachineFunctionInfo>();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  if (needsStackRealign(MF)) {
    if (!FuncInfo->hasStackRealignSaveFI())
      FuncInfo->setStackRealignSaveFI(MFI.CreateStackObject(2, Align(1), false));
    FuncInfo->setHasStackRealign(true);
    FuncInfo->setHasSpills(true);
  }
  if (FuncInfo->hasGR32ScratchFI())
    return;

  // Track usage of both 32-bit pseudo registers separately
  bool UsesIAX = false;
  bool UsesIBX = false;
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      for (auto &MO : MI.operands()) {
        if (!MO.isReg())
          continue;
        Register Reg = MO.getReg();
        if (Reg == I8085::IAX)
          UsesIAX = true;
        else if (Reg == I8085::IBX)
          UsesIBX = true;
      }
      // Early exit if both are used
      if (UsesIAX && UsesIBX)
        break;
    }
    if (UsesIAX && UsesIBX)
      break;
  }

  if (!UsesIAX && !UsesIBX)
    return;

  // Allocate scratch space only for registers that are actually used.
  // IAX uses bytes 0-3, IBX uses bytes 4-7 in the full 8-byte layout.
  // When only one register is used, we save 4 bytes of stack space.
  int ScratchSize;
  if (UsesIAX && UsesIBX) {
    ScratchSize = 8; // Both used: IAX at offset 0, IBX at offset 4
  } else {
    ScratchSize = 4; // Only one used: single register at offset 0
    if (UsesIBX) {
      // IBX normally lives at offset 4, but with a 4-byte scratch slot
      // it must be remapped to offset 0. Tell the expansion pass.
      FuncInfo->setIBXRemappedToZero(true);
    }
    // If only IAX: it's already at offset 0, no remapping needed.
  }
  int FI = MFI.CreateStackObject(ScratchSize, Align(1), false);
  FuncInfo->setGR32ScratchFI(FI);
  // Ensure we emit a frame adjustment even if there are no spills/allocas.
  FuncInfo->setHasSpills(true);
}

// This target addresses fixed frames through SP, not a dedicated FP register.
// hasFP also controls frame allocation, so frame-pointer=none must never
// suppress storage for spills, allocas, or GR32 pseudo registers.
bool I8085FrameLowering::hasFP(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const auto *FuncInfo = MF.getInfo<I8085MachineFunctionInfo>();
  return FuncInfo->hasStackRealign() || MFI.hasVarSizedObjects() ||
         FuncInfo->getHasSpills() || FuncInfo->getHasAllocas() ||
         FuncInfo->getHasStackArgs() ||
         MFI.getStackSize() > FuncInfo->getCalleeSavedFrameSize();
}

bool I8085FrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty()) {
    return false;
  }

  DebugLoc DL = MBB.findDebugLoc(MI);
  MachineFunction &MF = *MBB.getParent();
  const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
  const TargetInstrInfo &TII = *STI.getInstrInfo();
  I8085MachineFunctionInfo *I8085FI = MF.getInfo<I8085MachineFunctionInfo>();
  bool EmitCFI = MF.needsFrameMoves() && !I8085FI->hasStackRealign();

  I8085FI->setCalleeSavedFrameSize(CSI.size() * 2);

  for (const CalleeSavedInfo &I : CSI) {
    Register Reg = I.getReg();
    bool IsNotLiveIn = !MBB.isLiveIn(Reg);

    // assert(TRI->getRegSizeInBits(*TRI->getMinimalPhysRegClass(Reg)) == 8 &&
    //        "Invalid register size");

    // Add the callee-saved register as live-in only if it is not already a
    // live-in register, this usually happens with arguments that are passed
    // through callee-saved registers.
    if (IsNotLiveIn) {
      MBB.addLiveIn(Reg);
    }

    BuildMI(MBB, MI, DL, TII.get(I8085::PUSH))
        .addReg(Reg, getKillRegState(IsNotLiveIn))
        .setMIFlag(MachineInstr::FrameSetup);
    if (EmitCFI) {
      buildCFI(MBB, MI, DL,
               MCCFIInstruction::createAdjustCfaOffset(nullptr, 2),
               MachineInstr::FrameSetup, TII);
    }
  }

  return true;
}

bool I8085FrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty()) {
    return false;
  }

  DebugLoc DL = MBB.findDebugLoc(MI);
  const MachineFunction &MF = *MBB.getParent();
  const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
  const TargetInstrInfo &TII = *STI.getInstrInfo();
  const I8085MachineFunctionInfo *I8085FI = MF.getInfo<I8085MachineFunctionInfo>();
  bool EmitCFI = MF.needsFrameMoves() && !I8085FI->hasStackRealign();

  for (const CalleeSavedInfo &CCSI : llvm::reverse(CSI)) {
    Register Reg = CCSI.getReg();

    BuildMI(MBB, MI, DL, TII.get(I8085::POP), Reg)
        .setMIFlag(MachineInstr::FrameDestroy);
    if (EmitCFI) {
      buildCFI(MBB, MI, DL,
               MCCFIInstruction::createAdjustCfaOffset(nullptr, -2),
               MachineInstr::FrameDestroy, TII);
    }
  }

  return true;
}

namespace {

struct CallFrameRegion {
  bool ContainsCall = false;
  SmallVector<MachineInstr *, 2> Destroys;
};

// Call lowering may place a call-frame setup before target pseudos that later
// split the block. Follow the active call-frame region through the CFG rather
// than assuming its call and teardown remain in the setup block.
static CallFrameRegion analyzeCallFrameRegion(MachineInstr &Setup,
                                              const TargetInstrInfo &TII) {
  using WorkItem = std::pair<MachineBasicBlock *, MachineBasicBlock::iterator>;

  CallFrameRegion Region;
  MachineBasicBlock *SetupMBB = Setup.getParent();
  SmallVector<WorkItem, 4> Worklist;
  SmallPtrSet<MachineBasicBlock *, 8> Visited;
  Worklist.emplace_back(SetupMBB, std::next(Setup.getIterator()));
  Visited.insert(SetupMBB);

  while (!Worklist.empty()) {
    WorkItem Item = Worklist.pop_back_val();
    MachineBasicBlock *MBB = Item.first;
    auto I = Item.second;
    bool FoundDestroy = false;

    for (auto E = MBB->end(); I != E; ++I) {
      if (I->isCall()) {
        Region.ContainsCall = true;
      }

      if (I->getOpcode() != TII.getCallFrameDestroyOpcode())
        continue;

      Region.Destroys.push_back(&*I);
      FoundDestroy = true;
      break;
    }

    if (FoundDestroy)
      continue;

    for (MachineBasicBlock *Succ : MBB->successors()) {
      if (Visited.insert(Succ).second)
        Worklist.emplace_back(Succ, Succ->begin());
    }
  }

  return Region;
}

} // end anonymous namespace

MachineBasicBlock::iterator I8085FrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI) const {
  const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
  const I8085InstrInfo &TII = *STI.getInstrInfo();
  const I8085MachineFunctionInfo *I8085FI = MF.getInfo<I8085MachineFunctionInfo>();
  bool EmitCFI = MF.needsFrameMoves() && !I8085FI->hasStackRealign();

  // There is nothing to insert when the call frame memory is allocated during
  // function entry. Delete the call frame pseudo
  if (hasReservedCallFrame(MF)) {
    return MBB.erase(MI);
  }

  DebugLoc DL = MI->getDebugLoc();
  unsigned int Opcode = MI->getOpcode();
  int Amount = TII.getFrameSize(*MI);
  CallFrameRegion Region;
  if (Opcode == TII.getCallFrameSetupOpcode() &&
      MF.getFrameInfo().hasVarSizedObjects())
    Region = analyzeCallFrameRegion(*MI, TII);

  const bool PreserveBasePointer =
      Region.ContainsCall && !Region.Destroys.empty();

  // DE is reserved as the stable base for fixed frame objects when SP moves
  // for dynamic allocas.  It is otherwise caller-clobbered by the ABI, so save
  // it outside the outgoing argument area around every call sequence.
  if (PreserveBasePointer) {
    BuildMI(MBB, MI, DL, TII.get(I8085::PUSH)).addReg(I8085::DE);
    for (MachineInstr *CallFrameDestroy : Region.Destroys) {
      MachineBasicBlock &RestoreMBB = *CallFrameDestroy->getParent();
      auto DestroyAt = CallFrameDestroy->getIterator();
      auto RestoreAt = std::next(DestroyAt);
      auto ResultCapture = RestoreAt;
      while (ResultCapture != RestoreMBB.end() && ResultCapture->isDebugInstr())
        ++ResultCapture;

      // Fast register allocation can insert a reload immediately after CALL,
      // before CALLSEQ_END. Such a reload will use DE as its frame base after
      // PEI, but DE still contains the callee clobber at that point. The DAG
      // always places CALLSEQ_END before post-call work, so move these
      // allocator-inserted instructions back across the teardown. For an i32
      // result, place them after the atomic BC:DE capture as well.
      MachineBasicBlock::iterator CallAt = RestoreMBB.end();
      for (auto I = DestroyAt; I != RestoreMBB.begin();) {
        --I;
        if (I->isCall()) {
          CallAt = I;
          break;
        }
        if (I->getOpcode() == TII.getCallFrameSetupOpcode())
          break;
      }

      if (CallAt != RestoreMBB.end() && std::next(CallAt) != DestroyAt) {
        auto MoveBefore =
            ResultCapture != RestoreMBB.end() &&
                    ResultCapture->getOpcode() == I8085::PACK_BCDE_TO_32
                ? std::next(ResultCapture)
                : RestoreAt;
        RestoreMBB.splice(MoveBefore, &RestoreMBB, std::next(CallAt),
                          DestroyAt);
      }

      if (ResultCapture != RestoreMBB.end() &&
          ResultCapture->getOpcode() == I8085::PACK_BCDE_TO_32) {
        BuildMI(RestoreMBB, std::next(ResultCapture),
                ResultCapture->getDebugLoc(),
                TII.get(I8085::RESTORE_VLA_BASE_AFTER_PACK));
      } else {
        BuildMI(RestoreMBB, std::next(DestroyAt),
                CallFrameDestroy->getDebugLoc(), TII.get(I8085::POP),
                I8085::DE);
      }
    }
  }

  // ADJCALLSTACKUP and ADJCALLSTACKDOWN are converted to adiw/subi
  // instructions to read and write the stack pointer in I/O space.
  if (Amount != 0) {
    assert(getStackAlign() == Align(1) && "Unsupported stack alignment");

    if (Opcode == TII.getCallFrameSetupOpcode()) {

        BuildMI(MBB, MI, DL, TII.get(I8085::GROW_STACK_BY))
            .addImm(Amount);
        if (EmitCFI) {
          buildCFI(MBB, MI, DL,
                   MCCFIInstruction::createAdjustCfaOffset(nullptr, Amount),
                   MachineInstr::FrameSetup, TII);
        }

    } else {
      assert(Opcode == TII.getCallFrameDestroyOpcode());

        BuildMI(MBB, MI, DL, TII.get(I8085::SHRINK_STACK_BY))
            .addImm(Amount);
        if (EmitCFI) {
          buildCFI(MBB, MI, DL,
                   MCCFIInstruction::createAdjustCfaOffset(nullptr, -Amount),
                   MachineInstr::FrameDestroy, TII);
        }

    }
  }

  return MBB.erase(MI);
}

void I8085FrameLowering::determineCalleeSaves(MachineFunction &MF,
                                            BitVector &SavedRegs,
                                            RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);

  if (MF.getFrameInfo().hasVarSizedObjects()) {
    SavedRegs.set(I8085::DE);
  }
}
/// The frame analyzer pass.
///
/// Scans the function for allocas and used arguments
/// that are passed through the stack.
struct I8085FrameAnalyzer : public MachineFunctionPass {
  static char ID;
  I8085FrameAnalyzer() : MachineFunctionPass(ID) {
    initializeI8085FrameAnalyzerPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const MachineFrameInfo &MFI = MF.getFrameInfo();
    I8085MachineFunctionInfo *FuncInfo = MF.getInfo<I8085MachineFunctionInfo>();

    // If there are no fixed frame indexes during this stage it means there
    // are allocas present in the function.
    if (MFI.getNumObjects() != MFI.getNumFixedObjects()) {
      // Check for the type of allocas present in the function. We only care
      // about fixed size allocas so do not give false positives if only
      // variable sized allocas are present.
      for (unsigned i = 0, e = MFI.getObjectIndexEnd(); i != e; ++i) {
        // Variable sized objects have size 0.
        if (MFI.getObjectSize(i)) {
          FuncInfo->setHasAllocas(true);
          break;
        }
      }
    }

    // If there are fixed frame indexes present, scan the function to see if
    // they are really being used.
    if (MFI.getNumFixedObjects() == 0) {
      return false;
    }

    // Ok fixed frame indexes present, now scan the function to see if they
    // are really being used, otherwise we can ignore them.
    for (const MachineBasicBlock &BB : MF) {
      for (const MachineInstr &MI : BB) {
        for (const MachineOperand &MO : MI.operands()) {
          if (!MO.isFI()) {
            continue;
          }

          if (MFI.isFixedObjectIndex(MO.getIndex())) {
            FuncInfo->setHasStackArgs(true);
            return false;
          }
        }
      }
    }

    return false;
  }

  StringRef getPassName() const override { return "I8085 Frame Analyzer"; }
};

char I8085FrameAnalyzer::ID = 0;

/// Creates instance of the frame analyzer pass.
FunctionPass *createI8085FrameAnalyzerPass() { return new I8085FrameAnalyzer(); }

} // end of namespace llvm

using namespace llvm;

#define I8085_FRAME_ANALYZER_NAME "I8085 Frame Analyzer"
INITIALIZE_PASS(I8085FrameAnalyzer, "i8085-frame-analyzer",
                I8085_FRAME_ANALYZER_NAME, false, false)
