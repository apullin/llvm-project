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
  if (!MF.getFunction().hasFnAttribute("stackrealign"))
    return Align(1);

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
    return;
  }

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  unsigned FrameSize = MFI.getStackSize() - AFI->getCalleeSavedFrameSize();

  // Skip the callee-saved push instructions.
  while (
      (MBBI != MBB.end()) && MBBI->getFlag(MachineInstr::FrameSetup)) {
    ++MBBI;
  }
  
  /*Prologue sequence for 8085 processor */ 
  bool NeedsBasePtr = MFI.hasVarSizedObjects();
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
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::A)
        .addReg(I8085::L);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::ANI)).addImm(Mask);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::L)
        .addReg(I8085::A);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::SPHL));
  }

  if(FrameSize) {
      BuildMI(MBB, MBBI, DL, TII.get(I8085::GROW_STACK_BY))
          .addImm(FrameSize);
      if (EmitCFI) {
        buildCFI(MBB, MBBI, DL,
                 MCCFIInstruction::createAdjustCfaOffset(nullptr, FrameSize),
                 MachineInstr::FrameSetup, TII);
      }
      
  }

  if (NeedsBasePtr) {
    BuildMI(MBB, MBBI, DL, TII.get(I8085::LXI), I8085::HL).addImm(0);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::DAD)).addReg(I8085::SP);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::D)
        .addReg(I8085::H);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::E)
        .addReg(I8085::L);
  }

  if (AFI->hasStackRealignSaveFI()) {
    BuildMI(MBB, MBBI, DL, TII.get(I8085::STORE_16))
        .addFrameIndex(AFI->getStackRealignSaveFI())
        .addImm(0)
        .addReg(I8085::BC);
  }

  if (EmitCFI) {
    emitCalleeSavedFrameMoves(MBB, MBBI, DL, true, TII);
  }
}

static void restoreStatusRegister(MachineFunction &MF, MachineBasicBlock &MBB) {
  const I8085MachineFunctionInfo *AFI = MF.getInfo<I8085MachineFunctionInfo>();

  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();

  DebugLoc DL = MBBI->getDebugLoc();
  const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
  const I8085InstrInfo &TII = *STI.getInstrInfo();

}

void I8085FrameLowering::emitEpilogue(MachineFunction &MF,
                                    MachineBasicBlock &MBB) const {
  const I8085MachineFunctionInfo *AFI = MF.getInfo<I8085MachineFunctionInfo>();
  bool EmitCFI = MF.needsFrameMoves() && !AFI->hasStackRealign();

  // Early exit if the frame pointer is not needed in this function except for
  // signal/interrupt handlers where special code generation is required.
  if (!hasFP(MF) && !AFI->isInterruptOrSignalHandler()) {
    return;
  }

  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  assert(MBBI->getDesc().isReturn() &&
         "Can only insert epilog into returning blocks");

  DebugLoc DL = MBBI->getDebugLoc();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  unsigned FrameSize = MFI.getStackSize() - AFI->getCalleeSavedFrameSize();
  const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
  const I8085InstrInfo &TII = *STI.getInstrInfo();

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

  if (AFI->hasStackRealignSaveFI()) {
    BuildMI(MBB, MBBI, DL, TII.get(I8085::LOAD_16_WITH_ADDR), I8085::HL)
        .addFrameIndex(AFI->getStackRealignSaveFI())
        .addImm(0);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::SPHL));
    restoreStatusRegister(MF, MBB);
    return;
  }

  if (MFI.hasVarSizedObjects()) {
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::H).addReg(I8085::D);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::MOV), I8085::L).addReg(I8085::E);
    BuildMI(MBB, MBBI, DL, TII.get(I8085::SPHL));
  }

  if (FrameSize) {
    BuildMI(MBB, MBBI, DL, TII.get(I8085::SHRINK_STACK_BY))
        .addImm(FrameSize);
    if (EmitCFI) {
      buildCFI(MBB, MBBI, DL,
               MCCFIInstruction::createAdjustCfaOffset(nullptr, -FrameSize),
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
    int FI = MFI.CreateStackObject(2, Align(1), false);
    FuncInfo->setStackRealignSaveFI(FI);
    FuncInfo->setHasStackRealign(true);
    FuncInfo->setHasSpills(true);
  }
  if (FuncInfo->hasGR32ScratchFI())
    return;

  bool UsesGR32 = false;
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      for (auto &MO : MI.operands()) {
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

  if (!UsesGR32)
    return;

  // MFI already available above.
  int FI = MFI.CreateStackObject(8, Align(1), false);
  FuncInfo->setGR32ScratchFI(FI);
  // Ensure we emit a frame adjustment even if there are no spills/allocas.
  FuncInfo->setHasSpills(true);
}

// Return true if the specified function should have a dedicated frame
// pointer register. This is true if the function meets any of the following
// conditions:
//  - a register has been spilled
//  - has allocas
//  - input arguments are passed using the stack
//
// Notice that strictly this is not a frame pointer because it contains SP after
// frame allocation instead of having the original SP in function entry.
bool I8085FrameLowering::hasFP(const MachineFunction &MF) const {
  const I8085MachineFunctionInfo *FuncInfo = MF.getInfo<I8085MachineFunctionInfo>();

  return (FuncInfo->getHasSpills() || FuncInfo->getHasAllocas() ||
          FuncInfo->getHasStackArgs() || FuncInfo->hasStackRealign() ||
          MF.getFrameInfo().hasVarSizedObjects());
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
        int Opcode = MI.getOpcode();

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
