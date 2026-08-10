//===-- TMS9900FrameLowering.cpp - TMS9900 Frame Lowering -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the TMS9900 implementation of TargetFrameLowering class.
//
// TMS9900 Stack Frame Layout:
//
// The TMS9900 has no hardware stack, so we implement a software stack using R10
// as the stack pointer. The stack grows downward (high to low addresses).
//
// Function Prologue:
//   1. Save return address (R11) when needed
//   2. Allocate the fixed frame
//   3. Save and establish R13 when a stable frame pointer is required
//   4. Save other used callee-saved registers into fixed frame slots
//
// Function Epilogue:
//   1. Restore other callee-saved registers through the fixed frame base
//   2. Restore R10 from R13 and then restore the caller's R13
//   3. Deallocate the fixed frame
//   4. Restore R11 and return
//
//===----------------------------------------------------------------------===//

#include "TMS9900FrameLowering.h"
#include "TMS9900.h"
#include "TMS9900InstrInfo.h"
#include "TMS9900MachineFunctionInfo.h"
#include "TMS9900Subtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/MC/MCDwarf.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetOptions.h"

using namespace llvm;

TMS9900FrameLowering::TMS9900FrameLowering(const TMS9900Subtarget &STI)
    : TargetFrameLowering(StackGrowsDown,
                          /*StackAlignment=*/Align(4),
                          /*LocalAreaOffset=*/0) {}
// NOTE: Stack alignment is 4 bytes (not 2) to match LLVM's type legalizer
// assumptions. When splitting i32 loads/stores, LLVM uses OR-instead-of-ADD
// to compute the low-word address (e.g., ORI Rx,2 instead of AI Rx,2).
// This optimization assumes bit 1 of the base address is zero, which requires
// 4-byte stack alignment. With 2-byte alignment, non-4-byte-aligned stack
// addresses cause ORI to be a no-op, reading the high word twice.

bool TMS9900FrameLowering::hasFPImpl(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();

  // R13-R15 hold the hardware return context in an interrupt workspace.  An
  // interrupt that genuinely needs a frame pointer is rejected before frame
  // layout, while a global no-omit-frame-pointer option is harmlessly ignored.
  if (MF.getFunction().hasFnAttribute("interrupt"))
    return MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken();

  return MF.getTarget().Options.DisableFramePointerElim(MF) ||
         MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken() ||
         MFI.shouldRealignStack();
}

static int64_t getFramePointerSaveOffset(const MachineFunction &MF) {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const auto *FuncInfo = MF.getInfo<TMS9900MachineFunctionInfo>();
  int FI = FuncInfo->getFramePointerSaveIndex();
  assert(FI >= 0 && "frame-pointer save slot was not allocated");

  int64_t Offset = MFI.getObjectOffset(FI) + MFI.getStackSize();
  const Function &F = MF.getFunction();
  const bool SavesLR = MFI.hasCalls() && !F.hasFnAttribute("interrupt");
  if (SavesLR && MFI.getStackSize() > 0)
    Offset += 2;
  return Offset;
}

void TMS9900FrameLowering::emitPrologue(MachineFunction &MF,
                                         MachineBasicBlock &MBB) const {
  const Function &F = MF.getFunction();
  const bool IsInterrupt = F.hasFnAttribute("interrupt");

  // Naked functions have no prologue
  if (F.hasFnAttribute(Attribute::Naked))
    return;

  MachineBasicBlock::iterator MBBI = MBB.begin();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const bool HasFP = hasFP(MF);
  const TMS9900Subtarget &STI = MF.getSubtarget<TMS9900Subtarget>();
  const TMS9900InstrInfo &TII = *STI.getInstrInfo();
  const bool RealignsStack = STI.getRegisterInfo()->hasStackRealignment(MF);
  DebugLoc DL;

  if (MBBI != MBB.end())
    DL = MBBI->getDebugLoc();

  // Get the stack size (already aligned by processFunctionBeforeFrameFinalized)
  uint64_t StackSize = MFI.getStackSize();

  // If we have a stack frame, we need to:
  // 1. Save the return address (R11) - done by caller or here
  // 2. Adjust the stack pointer

  // For non-leaf functions, save R11 (return address)
  // DECT R10        ; SP -= 2
  // MOV R11,*R10    ; Push R11
  if (!MFI.hasCalls() && StackSize == 0 && !HasFP) {
    // Leaf function with no locals - no prologue needed
    return;
  }

  // Push return address for non-leaf functions
  // An interrupt returns through R13-R15 with RTWP, so its R11 need not be
  // preserved.  Its interrupt workspace must nevertheless provide a valid,
  // 4-byte-aligned stack pointer in R10 whenever the handler can spill, use
  // local stack storage, or make a call.
  const bool SavesLR = MFI.hasCalls() && !IsInterrupt;
  if (SavesLR) {
    // DECT R10 - decrement stack pointer
    BuildMI(MBB, MBBI, DL, TII.get(TMS9900::DECTr), TMS9900::R10)
        .addReg(TMS9900::R10);
    // MOV R11,*R10 - store R11 at address in R10
    BuildMI(MBB, MBBI, DL, TII.get(TMS9900::MOVmi))
        .addReg(TMS9900::R10)   // pointer (destination address)
        .addReg(TMS9900::R11);  // source value

    // CFI: CFA is now at R10+2 (we pushed 2 bytes)
    unsigned CFIIndex = MF.addFrameInst(
        MCCFIInstruction::cfiDefCfaOffset(nullptr, 2));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);

    // CFI: R11 (return address, DWARF reg 11) is saved at CFA-2
    CFIIndex = MF.addFrameInst(
        MCCFIInstruction::createOffset(nullptr, 11, -2));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);
  }

  if (RealignsStack) {
    uint64_t CFAOffset = SavesLR ? 2 : 0;

    // Preserve the caller's frame anchor and aligned base outside the
    // variably positioned frame. R13 then records the exact pre-alignment SP,
    // while R14 will address local objects from the aligned frame base.
    for (Register Reg : {TMS9900::R13, TMS9900::R14}) {
      BuildMI(MBB, MBBI, DL, TII.get(TMS9900::DECTr), TMS9900::R10)
          .addReg(TMS9900::R10);
      BuildMI(MBB, MBBI, DL, TII.get(TMS9900::MOVmi))
          .addReg(TMS9900::R10)
          .addReg(Reg);

      CFAOffset += 2;
      unsigned CFIIndex = MF.addFrameInst(
          MCCFIInstruction::cfiDefCfaOffset(nullptr, CFAOffset));
      BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
          .addCFIIndex(CFIIndex);
      CFIIndex = MF.addFrameInst(MCCFIInstruction::createOffset(
          nullptr, Reg == TMS9900::R13 ? 13 : 14,
          -static_cast<int64_t>(CFAOffset)));
      BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
          .addCFIIndex(CFIIndex);
    }

    BuildMI(MBB, MBBI, DL, TII.get(TMS9900::MOVrr), TMS9900::R13)
        .addReg(TMS9900::R10);
    unsigned CFIIndex = MF.addFrameInst(
        MCCFIInstruction::cfiDefCfa(nullptr, 13, CFAOffset));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);

    // Subtract the nominal frame size, then round down. The rounding itself
    // supplies up to MaxAlign-2 bytes of alignment slop because R10 is always
    // at least word-aligned.
    uint64_t MaxAlign = MFI.getMaxAlign().value();
    if (StackSize > 0)
      BuildMI(MBB, MBBI, DL, TII.get(TMS9900::AI), TMS9900::R10)
          .addReg(TMS9900::R10)
          .addImm(-static_cast<int64_t>(StackSize));
    BuildMI(MBB, MBBI, DL, TII.get(TMS9900::ANDI), TMS9900::R10)
        .addReg(TMS9900::R10)
        .addImm(-static_cast<int64_t>(MaxAlign));
    BuildMI(MBB, MBBI, DL, TII.get(TMS9900::MOVrr), TMS9900::R14)
        .addReg(TMS9900::R10);
    return;
  }

  // Allocate stack space.
  // For non-leaf functions, the DECT above subtracted 2 from SP, breaking
  // 4-byte alignment (SP is now 4k+2). Always add 2 bytes of padding before
  // a call, even when the function has no fixed frame. This makes the total
  // prologue displacement = 2 (DECT) + StackSize + 2 (pad) = StackSize + 4,
  // which is 4-byte aligned (since StackSize is always 4-aligned).
  // The extra 2 bytes are accounted for in eliminateFrameIndex.
  uint64_t AllocSize = StackSize;
  if (SavesLR)
    AllocSize += 2;  // alignment padding after DECT

  if (AllocSize > 0) {
    // AI R10,-AllocSize
    BuildMI(MBB, MBBI, DL, TII.get(TMS9900::AI), TMS9900::R10)
        .addReg(TMS9900::R10)
        .addImm(-static_cast<int64_t>(AllocSize));

    // CFI: CFA offset grows by AllocSize
    unsigned CFIIndex = MF.addFrameInst(
        MCCFIInstruction::cfiDefCfaOffset(nullptr,
                                          AllocSize + (SavesLR ? 2 : 0)));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);
  }

  if (HasFP) {
    int64_t SaveOffset = getFramePointerSaveOffset(MF);
    if (SaveOffset == 0) {
      BuildMI(MBB, MBBI, DL, TII.get(TMS9900::MOVmi))
          .addReg(TMS9900::R10)
          .addReg(TMS9900::R13);
    } else {
      BuildMI(MBB, MBBI, DL, TII.get(TMS9900::MOVmx))
          .addImm(SaveOffset)
          .addReg(TMS9900::R10)
          .addReg(TMS9900::R13);
    }

    uint64_t CFAOffset = AllocSize + (SavesLR ? 2 : 0);
    unsigned CFIIndex = MF.addFrameInst(MCCFIInstruction::createOffset(
        nullptr, 13, SaveOffset - static_cast<int64_t>(CFAOffset)));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);

    BuildMI(MBB, MBBI, DL, TII.get(TMS9900::MOVrr), TMS9900::R13)
        .addReg(TMS9900::R10);

    CFIIndex = MF.addFrameInst(
        MCCFIInstruction::cfiDefCfa(nullptr, 13, CFAOffset));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);
  }
}

void TMS9900FrameLowering::emitEpilogue(MachineFunction &MF,
                                         MachineBasicBlock &MBB) const {
  const Function &F = MF.getFunction();
  const bool IsInterrupt = F.hasFnAttribute("interrupt");

  // Naked functions have no epilogue
  if (F.hasFnAttribute(Attribute::Naked))
    return;

  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const bool HasFP = hasFP(MF);
  const TMS9900Subtarget &STI = MF.getSubtarget<TMS9900Subtarget>();
  const TMS9900InstrInfo &TII = *STI.getInstrInfo();
  const bool RealignsStack = STI.getRegisterInfo()->hasStackRealignment(MF);
  DebugLoc DL;

  if (MBBI != MBB.end())
    DL = MBBI->getDebugLoc();

  uint64_t StackSize = MFI.getStackSize();
  const bool RestoresLR = MFI.hasCalls() && !IsInterrupt;

  if (!RestoresLR && StackSize == 0 && !HasFP) {
    // Leaf function with no locals - no epilogue needed
    return;
  }

  if (RealignsStack) {
    uint64_t CFAOffset = (RestoresLR ? 2 : 0) + 4;

    // Discard dynamic allocations and alignment slop in one step.
    BuildMI(MBB, MBBI, DL, TII.get(TMS9900::MOVrr), TMS9900::R10)
        .addReg(TMS9900::R13);
    unsigned CFIIndex = MF.addFrameInst(
        MCCFIInstruction::cfiDefCfa(nullptr, 10, CFAOffset));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);

    BuildMI(MBB, MBBI, DL, TII.get(TMS9900::MOVpim))
        .addDef(TMS9900::R14)
        .addDef(TMS9900::R10)
        .addUse(TMS9900::R10);
    CFIIndex = MF.addFrameInst(
        MCCFIInstruction::createRestore(nullptr, 14));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);
    CFAOffset -= 2;
    CFIIndex = MF.addFrameInst(
        MCCFIInstruction::cfiDefCfaOffset(nullptr, CFAOffset));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);

    BuildMI(MBB, MBBI, DL, TII.get(TMS9900::MOVpim))
        .addDef(TMS9900::R13)
        .addDef(TMS9900::R10)
        .addUse(TMS9900::R10);
    CFIIndex = MF.addFrameInst(
        MCCFIInstruction::createRestore(nullptr, 13));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);
    CFAOffset -= 2;
    CFIIndex = MF.addFrameInst(
        MCCFIInstruction::cfiDefCfaOffset(nullptr, CFAOffset));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);

    if (RestoresLR) {
      BuildMI(MBB, MBBI, DL, TII.get(TMS9900::MOVpim))
          .addDef(TMS9900::R11)
          .addDef(TMS9900::R10)
          .addUse(TMS9900::R10);
      CFIIndex = MF.addFrameInst(
          MCCFIInstruction::createRestore(nullptr, 11));
      BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
          .addCFIIndex(CFIIndex);
    }

    CFIIndex = MF.addFrameInst(
        MCCFIInstruction::cfiDefCfaOffset(nullptr, 0));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);
    return;
  }

  // Deallocate stack space (must match the allocation in emitPrologue)
  uint64_t DeallocSize = StackSize;
  if (RestoresLR)
    DeallocSize += 2;  // alignment padding (matches prologue)

  if (HasFP) {
    uint64_t CFAOffset = DeallocSize + (RestoresLR ? 2 : 0);
    BuildMI(MBB, MBBI, DL, TII.get(TMS9900::MOVrr), TMS9900::R10)
        .addReg(TMS9900::R13);

    unsigned CFIIndex = MF.addFrameInst(
        MCCFIInstruction::cfiDefCfa(nullptr, 10, CFAOffset));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);

    int64_t SaveOffset = getFramePointerSaveOffset(MF);
    if (SaveOffset == 0) {
      BuildMI(MBB, MBBI, DL, TII.get(TMS9900::MOVim), TMS9900::R13)
          .addReg(TMS9900::R10);
    } else {
      BuildMI(MBB, MBBI, DL, TII.get(TMS9900::MOVxm), TMS9900::R13)
          .addImm(SaveOffset)
          .addReg(TMS9900::R10);
    }

    CFIIndex = MF.addFrameInst(
        MCCFIInstruction::createRestore(nullptr, 13));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);
  }

  if (DeallocSize > 0) {
    // AI R10,DeallocSize
    BuildMI(MBB, MBBI, DL, TII.get(TMS9900::AI), TMS9900::R10)
        .addReg(TMS9900::R10)
        .addImm(DeallocSize);

    // R10 now points at the saved link register, or back at the entry stack
    // pointer for a leaf frame. Keep the unwind row synchronized with that
    // physical adjustment before any following instruction can be sampled.
    unsigned CFIIndex = MF.addFrameInst(MCCFIInstruction::cfiDefCfaOffset(
        nullptr, RestoresLR ? 2 : 0));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);
  }

  // Restore return address for non-leaf functions
  if (RestoresLR) {
    // MOV *R10+,R11 - load R11 from address in R10, then R10 += 2
    BuildMI(MBB, MBBI, DL, TII.get(TMS9900::MOVpim))
        .addDef(TMS9900::R11)   // $rd - loaded value
        .addDef(TMS9900::R10)   // $rs_wb - incremented pointer (tied to $rs)
        .addUse(TMS9900::R10);  // $rs - original pointer

    unsigned CFIIndex = MF.addFrameInst(
        MCCFIInstruction::createRestore(nullptr, 11));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);
  }

  // CFI: Restore CFA to entry state
  {
    unsigned CFIIndex = MF.addFrameInst(
        MCCFIInstruction::cfiDefCfaOffset(nullptr, 0));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);
  }

  // The actual return (B *R11) is handled by the RET pseudo
}

MachineBasicBlock::iterator TMS9900FrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  const TMS9900Subtarget &STI = MF.getSubtarget<TMS9900Subtarget>();
  const TMS9900InstrInfo &TII = *STI.getInstrInfo();

  if (hasReservedCallFrame(MF))
    return MBB.erase(I);

  MachineInstr &MI = *I;
  DebugLoc DL = MI.getDebugLoc();
  int64_t Amount = MI.getOperand(0).getImm();

  if (Amount != 0) {
    // Adjust stack pointer
    if (MI.getOpcode() == TMS9900::ADJCALLSTACKDOWN) {
      // Allocate space: AI R10,-Amount
      BuildMI(MBB, I, DL, TII.get(TMS9900::AI), TMS9900::R10)
          .addReg(TMS9900::R10)
          .addImm(-Amount);
    } else {
      assert(MI.getOpcode() == TMS9900::ADJCALLSTACKUP);
      // Deallocate space: AI R10,Amount
      BuildMI(MBB, I, DL, TII.get(TMS9900::AI), TMS9900::R10)
          .addReg(TMS9900::R10)
          .addImm(Amount);
    }
  }

  return MBB.erase(I);
}

void TMS9900FrameLowering::determineCalleeSaves(MachineFunction &MF,
                                                 BitVector &SavedRegs,
                                                 RegScavenger *RS) const {
  const Function &F = MF.getFunction();

  // Naked functions don't spill callee-saved registers
  if (F.hasFnAttribute(Attribute::Naked))
    return;

  // Interrupt handlers use a separate workspace.  R13-R15 are reserved as
  // the hardware return context, and no other workspace register is
  // callee-saved across RTWP.
  if (F.hasFnAttribute("interrupt"))
    return;

  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);

  // R13, R14, R15 are callee-saved
  // They will be saved if used
}

void TMS9900FrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *RS) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const Function &F = MF.getFunction();
  const bool NeedsStableFrame =
      MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken();
  const bool NeedsRealignment = MFI.shouldRealignStack();

  const bool HasRestrictedPrologue =
      F.hasFnAttribute(Attribute::Naked) || F.hasFnAttribute("interrupt");
  if (NeedsStableFrame && HasRestrictedPrologue)
    report_fatal_error("TMS9900: variable stack objects and frame addresses "
                       "are unsupported in naked and interrupt functions");
  if (NeedsRealignment && HasRestrictedPrologue)
    report_fatal_error("TMS9900: stack realignment is unsupported in naked "
                       "and interrupt functions");

  if (!hasFP(MF))
    return;

  // Realigned frames preserve R13 and R14 with explicit pushes because their
  // final stack offsets depend on the runtime alignment adjustment.
  if (NeedsRealignment)
    return;

  auto *FuncInfo = MF.getInfo<TMS9900MachineFunctionInfo>();
  assert(FuncInfo->getFramePointerSaveIndex() < 0 &&
         "frame-pointer save slot allocated twice");
  int FI = MFI.CreateStackObject(2, Align(2), /*isSpillSlot=*/true);
  FuncInfo->setFramePointerSaveIndex(FI);
}
