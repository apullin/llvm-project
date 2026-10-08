//===-- I8085InstrInfo.cpp - I8085 Instruction Information --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the I8085 implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "I8085InstrInfo.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineOutliner.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

#include "I8085.h"
#include "I8085MachineFunctionInfo.h"
#include "I8085RegisterInfo.h"
#include "I8085TargetMachine.h"
#include "MCTargetDesc/I8085MCTargetDesc.h"

#include <iostream>

#define GET_INSTRINFO_CTOR_DTOR
#include "I8085GenInstrInfo.inc"

namespace llvm {

I8085InstrInfo::I8085InstrInfo()
    : I8085GenInstrInfo(I8085::ADJCALLSTACKDOWN, I8085::ADJCALLSTACKUP), RI() {}

static bool getPairRegs(MCRegister Reg, unsigned &LowReg, unsigned &HighReg) {
  switch (Reg) {
  case I8085::BC:
    LowReg = I8085::C;
    HighReg = I8085::B;
    return true;
  case I8085::DE:
    LowReg = I8085::E;
    HighReg = I8085::D;
    return true;
  case I8085::HL:
    LowReg = I8085::L;
    HighReg = I8085::H;
    return true;
  default:
    return false;
  }
}

void I8085InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator MI,
                               const DebugLoc &DL, MCRegister DestReg,
                               MCRegister SrcReg, bool KillSrc) const {
  unsigned Opc;


  if (I8085::GR8RegClass.contains(DestReg, SrcReg)) {
      Opc = I8085::MOV;

      BuildMI(MBB, MI, DL, get(Opc), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
  } else if(I8085::GR16RegClass.contains(DestReg, SrcReg)) {

  unsigned destLow, destHigh;
  unsigned srcLow, srcHigh;

  if (DestReg == I8085::SP) {
    if (SrcReg == I8085::HL) {
      BuildMI(MBB, MI, DL, get(I8085::SPHL));
      return;
    }
    if (!getPairRegs(SrcReg, srcLow, srcHigh))
      return;
    BuildMI(MBB, MI, DL, get(I8085::MOV), I8085::H)
        .addReg(srcHigh, getKillRegState(KillSrc));
    BuildMI(MBB, MI, DL, get(I8085::MOV), I8085::L)
        .addReg(srcLow, getKillRegState(KillSrc));
    BuildMI(MBB, MI, DL, get(I8085::SPHL));
    return;
  }

  if (SrcReg == I8085::SP) {
    bool PreserveHL = DestReg != I8085::HL;
    bool PreservePSW = true;
    if (MI != MBB.end() && MI->getOpcode() == TargetOpcode::COPY &&
        MBB.getParent()->getRegInfo().tracksLiveness()) {
      LivePhysRegs LiveRegs(RI);
      LiveRegs.addLiveOuts(MBB);
      for (auto I = MBB.rbegin(), E = MI.getReverse(); I != E; ++I)
        LiveRegs.stepBackward(*I);
      PreserveHL = DestReg != I8085::HL &&
                   (LiveRegs.contains(I8085::HL) ||
                    LiveRegs.contains(I8085::H) || LiveRegs.contains(I8085::L));
      PreservePSW =
          LiveRegs.contains(I8085::A) || LiveRegs.contains(I8085::SREG);
    }

    if (PreserveHL)
      BuildMI(MBB, MI, DL, get(I8085::PUSH)).addReg(I8085::HL);
    if (PreservePSW)
      BuildMI(MBB, MI, DL, get(I8085::PUSH)).addReg(I8085::PSW);

    const unsigned StackAdjustment =
        2 * (unsigned(PreserveHL) + unsigned(PreservePSW));
    BuildMI(MBB, MI, DL, get(I8085::LXI), I8085::HL).addImm(StackAdjustment);
    BuildMI(MBB, MI, DL, get(I8085::DAD)).addReg(I8085::SP);

    if (DestReg != I8085::HL) {
      if (!getPairRegs(DestReg, destLow, destHigh))
        return;
      BuildMI(MBB, MI, DL, get(I8085::MOV), destHigh).addReg(I8085::H);
      BuildMI(MBB, MI, DL, get(I8085::MOV), destLow).addReg(I8085::L);
    }

    if (PreservePSW)
      BuildMI(MBB, MI, DL, get(I8085::POP), I8085::PSW);
    if (PreserveHL)
      BuildMI(MBB, MI, DL, get(I8085::POP), I8085::HL);
    return;
  }

  if (!getPairRegs(DestReg, destLow, destHigh))
    return;
  if (!getPairRegs(SrcReg, srcLow, srcHigh))
    return;

  Opc = I8085::MOV;
  BuildMI(MBB, MI, DL, get(Opc), destHigh)
        .addReg(srcHigh, getKillRegState(KillSrc));
  BuildMI(MBB, MI, DL, get(Opc), destLow)
        .addReg(srcLow, getKillRegState(KillSrc));    
  } else if (I8085::GR32RegClass.contains(DestReg, SrcReg)) {
    Opc = I8085::MOV_32;
    BuildMI(MBB, MI, DL, get(Opc), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
  } else if (I8085::GR32RegClass.contains(DestReg) &&
             I8085::GR8RegClass.contains(SrcReg)) {
    BuildMI(MBB, MI, DL, get(I8085::ZEXT8TO32), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
  } else if (I8085::GR32RegClass.contains(DestReg) &&
             I8085::GR16RegClass.contains(SrcReg)) {
    BuildMI(MBB, MI, DL, get(I8085::AEXT16TO32), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
  } else if (I8085::GR8RegClass.contains(DestReg) &&
             I8085::GR32RegClass.contains(SrcReg)) {
    BuildMI(MBB, MI, DL, get(I8085::TRUNC32TO8), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
  } else if (I8085::GR16RegClass.contains(DestReg) &&
             I8085::GR32RegClass.contains(SrcReg)) {
    BuildMI(MBB, MI, DL, get(I8085::TRUNC32TO16), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
  }
}

Register I8085InstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                           int &FrameIndex) const {
  switch (MI.getOpcode()) {
  case I8085::LOAD_8_WITH_ADDR:
  case I8085::LOAD_16_WITH_ADDR:
  case I8085::LOAD_32_WITH_ADDR: { 
    if (MI.getOperand(1).isFI() && MI.getOperand(2).isImm() &&
        MI.getOperand(2).getImm() == 0) {
      FrameIndex = MI.getOperand(1).getIndex();
      return MI.getOperand(0).getReg();
    }
    break;
  }
  default:
    break;
  }

  return 0;
}

Register I8085InstrInfo::isStoreToStackSlot(const MachineInstr &MI,
                                          int &FrameIndex) const {
  switch (MI.getOpcode()) {
  case I8085::STORE_8:
  case I8085::STORE_16: 
  case I8085::STORE_32: {
    if (MI.getOperand(0).isFI() && MI.getOperand(1).isImm() &&
        MI.getOperand(1).getImm() == 0) {
      FrameIndex = MI.getOperand(0).getIndex();
      return MI.getOperand(2).getReg();
    }
    break;
  }
  default:
    break;
  }

  return 0;
}

void I8085InstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MI,
                                       Register SrcReg, bool isKill,
                                       int FrameIndex,
                                       const TargetRegisterClass *RC,
                                       const TargetRegisterInfo *TRI,
                                       Register VReg) const {
  MachineFunction &MF = *MBB.getParent();
  I8085MachineFunctionInfo *AFI = MF.getInfo<I8085MachineFunctionInfo>();

  AFI->setHasSpills(true);

  DebugLoc DL;
  if (MI != MBB.end()) {
    DL = MI->getDebugLoc();
  }

  const MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOStore, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));

  unsigned Opcode = 0;
  if (TRI->isTypeLegalForClass(*RC, MVT::i8)) {
    Opcode = I8085::STORE_8;
  } else if (TRI->isTypeLegalForClass(*RC, MVT::i16)) {
    Opcode = I8085::STORE_16;
  } else if (TRI->isTypeLegalForClass(*RC, MVT::i32)) {
    Opcode = I8085::STORE_32;
  } else {
    llvm_unreachable("Cannot store this register into a stack slot!");
  }

  // Spilling must accept every register in RC, including H/L. Expansion
  // captures their value before using HL to compute the stack address.
  MachineInstrBuilder Spill =
      BuildMI(MBB, MI, DL, get(Opcode))
          .addFrameIndex(FrameIndex)
          .addImm(0)
          .addReg(SrcReg, getKillRegState(isKill))
          .addMemOperand(MMO);
  for (MachineOperand &MO : Spill->implicit_operands())
    if (MO.isReg() && MO.isDef())
      MO.setIsDead();
}

void I8085InstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MI,
                                        Register DestReg, int FrameIndex,
                                        const TargetRegisterClass *RC,
                                        const TargetRegisterInfo *TRI,
                                        Register VReg) const {
  DebugLoc DL;
  if (MI != MBB.end()) {
    DL = MI->getDebugLoc();
  }

  MachineFunction &MF = *MBB.getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOLoad, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));

  unsigned Opcode = 0;
  if (TRI->isTypeLegalForClass(*RC, MVT::i8)) {
    Opcode = I8085::LOAD_8_WITH_ADDR;
  } else if (TRI->isTypeLegalForClass(*RC, MVT::i16)) {
    Opcode = I8085::LOAD_16_WITH_ADDR;
  } else if (TRI->isTypeLegalForClass(*RC, MVT::i32)) {
    Opcode = I8085::LOAD_32_WITH_ADDR;
  } else {
    llvm_unreachable("Cannot load this register from a stack slot!");
  }

  MachineInstrBuilder Reload = BuildMI(MBB, MI, DL, get(Opcode), DestReg)
                                   .addFrameIndex(FrameIndex)
                                   .addImm(0)
                                   .addMemOperand(MMO);
  for (MachineOperand &MO : Reload->implicit_operands())
    if (MO.isReg() && MO.isDef())
      MO.setIsDead();
}


unsigned I8085InstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  unsigned Opcode = MI.getOpcode();

  switch (Opcode) {
  case I8085::TCRETURN:
    return 3;
  // A regular instruction
  default: {
    const MCInstrDesc &Desc = get(Opcode);
    unsigned Size = Desc.getSize();
    // Pseudos have Size=0 in tablegen. Return a conservative estimate
    // for any pass that queries size before expansion. Most pseudos expand
    // to 5-20 bytes; 16 is a reasonable upper bound for estimation.
    if (Size == 0 && Desc.isPseudo())
      return 16;
    return Size;
  }
  case TargetOpcode::EH_LABEL:
  case TargetOpcode::IMPLICIT_DEF:
  case TargetOpcode::KILL:
  case TargetOpcode::DBG_VALUE:
    return 0;
  }
}

static bool isCondBranchOpcode(unsigned Opc) {
  switch (Opc) {
  case I8085::JZ:
  case I8085::JNZ:
  case I8085::JC:
  case I8085::JNC:
  case I8085::JP:
  case I8085::JM:
  case I8085::JPE:
  case I8085::JPO:
    return true;
  default:
    return false;
  }
}

static unsigned getOppositeBranchOpcode(unsigned Opc) {
  switch (Opc) {
  case I8085::JZ:
    return I8085::JNZ;
  case I8085::JNZ:
    return I8085::JZ;
  case I8085::JC:
    return I8085::JNC;
  case I8085::JNC:
    return I8085::JC;
  case I8085::JP:
    return I8085::JM;
  case I8085::JM:
    return I8085::JP;
  case I8085::JPE:
    return I8085::JPO;
  case I8085::JPO:
    return I8085::JPE;
  default:
    llvm_unreachable("unexpected branch opcode!");
  }
}

bool I8085InstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                   MachineBasicBlock *&TBB,
                                   MachineBasicBlock *&FBB,
                                   SmallVectorImpl<MachineOperand> &Cond,
                                   bool AllowModify) const {
  TBB = nullptr;
  FBB = nullptr;
  Cond.clear();

  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;

    if (!isUnpredicatedTerminator(*I))
      break;

    if (!I->isBranch())
      return true;

    unsigned Opc = I->getOpcode();
    if (Opc == I8085::JMP) {
      if (!AllowModify) {
        TBB = I->getOperand(0).getMBB();
        continue;
      }

      MBB.erase(std::next(I), MBB.end());
      Cond.clear();
      FBB = nullptr;

      if (MBB.isLayoutSuccessor(I->getOperand(0).getMBB())) {
        TBB = nullptr;
        I->eraseFromParent();
        I = MBB.end();
        continue;
      }

      TBB = I->getOperand(0).getMBB();
      continue;
    }

    if (!isCondBranchOpcode(Opc))
      return true;

    if (Cond.empty()) {
      FBB = TBB;
      TBB = I->getOperand(0).getMBB();
      Cond.push_back(MachineOperand::CreateImm(Opc));
      continue;
    }

    assert(Cond.size() == 1);
    assert(TBB);

    unsigned OldOpc = Cond[0].getImm();
    MachineBasicBlock *OldTBB = TBB;
    MachineBasicBlock *ThisTBB = I->getOperand(0).getMBB();
    if (OldOpc == Opc && OldTBB == ThisTBB)
      continue;

    // Selection can emit an exhaustive pair such as JZ true; JNZ false.
    // Model it like the equivalent JZ true; JMP false sequence so generic
    // block placement does not mistake it for an un-analyzable fallthrough.
    if (OldOpc == getOppositeBranchOpcode(Opc)) {
      if (OldTBB == ThisTBB) {
        TBB = ThisTBB;
        FBB = nullptr;
        Cond.clear();
      } else {
        TBB = ThisTBB;
        FBB = OldTBB;
        Cond[0].setImm(Opc);
      }
      continue;
    }

    return true;
  }

  return false;
}

unsigned I8085InstrInfo::insertBranch(MachineBasicBlock &MBB,
                                      MachineBasicBlock *TBB,
                                      MachineBasicBlock *FBB,
                                      ArrayRef<MachineOperand> Cond,
                                      const DebugLoc &DL,
                                      int *BytesAdded) const {
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert((Cond.size() == 1 || Cond.empty()) &&
         "I8085 branch conditions have one component!");

  if (BytesAdded)
    *BytesAdded = 0;

  if (Cond.empty()) {
    assert(!FBB && "Unconditional branch with multiple successors!");
    auto &MI = *BuildMI(&MBB, DL, get(I8085::JMP)).addMBB(TBB);
    if (BytesAdded)
      *BytesAdded += getInstSizeInBytes(MI);
    return 1;
  }

  unsigned Opc = Cond[0].getImm();
  assert(isCondBranchOpcode(Opc) && "Invalid conditional branch opcode");
  auto &CondMI = *BuildMI(&MBB, DL, get(Opc)).addMBB(TBB);
  if (BytesAdded)
    *BytesAdded += getInstSizeInBytes(CondMI);

  if (FBB) {
    auto &MI = *BuildMI(&MBB, DL, get(I8085::JMP)).addMBB(FBB);
    if (BytesAdded)
      *BytesAdded += getInstSizeInBytes(MI);
    return 2;
  }

  return 1;
}

unsigned I8085InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                      int *BytesRemoved) const {
  if (BytesRemoved)
    *BytesRemoved = 0;

  unsigned Count = 0;
  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;

    unsigned Opc = I->getOpcode();
    if (Opc != I8085::JMP && !isCondBranchOpcode(Opc))
      break;

    if (BytesRemoved)
      *BytesRemoved += getInstSizeInBytes(*I);
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }

  return Count;
}

bool I8085InstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1 && "Invalid I8085 branch condition");
  unsigned Opc = Cond[0].getImm();
  Cond[0].setImm(getOppositeBranchOpcode(Opc));
  return false;
}

MachineBasicBlock *I8085InstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  default:
    llvm_unreachable("unexpected branch opcode!");
  case I8085::JMP:
  case I8085::JZ:
  case I8085::JP:
  case I8085::JM:
  case I8085::JNZ:
  case I8085::JC:
  case I8085::JNC:
  case I8085::JPE:
  case I8085::JPO:
    return MI.getOperand(0).getMBB();
  }
}


bool I8085InstrInfo::isBranchOffsetInRange(unsigned BranchOp,
                                         int64_t BrOffset) const {

  switch (BranchOp) {
  default:
    llvm_unreachable("unexpected opcode!");
  case I8085::JMP:
  case I8085::JZ:
  case I8085::JP:
  case I8085::JM:
  case I8085::JNZ:
  case I8085::JC:
  case I8085::JNC:
  case I8085::JPE:
  case I8085::JPO:
    return true;
  }
}

enum I8085MachineOutlinerConstructionID {
  MachineOutlinerDefault,
  MachineOutlinerTailCall,
};

static bool isDADSP(const MachineInstr &MI) {
  return MI.getOpcode() == I8085::DAD && MI.getOperand(0).isReg() &&
         MI.getOperand(0).getReg() == I8085::SP;
}

static bool isLXIHLImm(const MachineInstr &MI) {
  return MI.getOpcode() == I8085::LXI && MI.getOperand(0).isReg() &&
         MI.getOperand(0).getReg() == I8085::HL && MI.getOperand(1).isImm();
}

static bool isI8085OutlinerMemoryOpcode(unsigned Opcode) {
  switch (Opcode) {
  case I8085::MOV_FROM_M:
  case I8085::MOV_M:
  case I8085::MVI_M:
    return true;
  default:
    return false;
  }
}

static bool isI8085OutlinerOpcode(unsigned Opcode) {
  switch (Opcode) {
  case I8085::ACI:
  case I8085::ADC:
  case I8085::ADD:
  case I8085::ADI:
  case I8085::ANA:
  case I8085::ANI:
  case I8085::CMA:
  case I8085::CMC:
  case I8085::CMP:
  case I8085::CPI:
  case I8085::DAA:
  case I8085::DAD:
  case I8085::DCR:
  case I8085::DCX:
  case I8085::INR:
  case I8085::INX:
  case I8085::LXI:
  case I8085::MOV:
  case I8085::MOV_FROM_M:
  case I8085::MOV_M:
  case I8085::MVI:
  case I8085::MVI_M:
  case I8085::NOP:
  case I8085::ORA:
  case I8085::ORI:
  case I8085::RAL:
  case I8085::RAR:
  case I8085::RLC:
  case I8085::RRC:
  case I8085::SBB:
  case I8085::SBI:
  case I8085::STC:
  case I8085::SUB:
  case I8085::SUI:
  case I8085::XCHG:
  case I8085::XRA:
  case I8085::XRI:
    return true;
  default:
    return false;
  }
}

static bool hasSafeStackRelativeFixups(outliner::Candidate &C) {
  for (auto It = C.begin(), End = C.end(); It != End; ++It) {
    if (!isDADSP(*It))
      continue;
    if (It == C.begin())
      return false;
    if (!isLXIHLImm(*std::prev(It)))
      return false;
  }
  return true;
}

static bool regOverlapsAny(Register Reg, ArrayRef<Register> Regs,
                           const TargetRegisterInfo &TRI) {
  return any_of(Regs, [&](Register Other) {
    return TRI.regsOverlap(Reg, Other);
  });
}

static void addUniqueReg(SmallVectorImpl<Register> &Regs, Register Reg,
                         const TargetRegisterInfo &TRI) {
  if (!regOverlapsAny(Reg, Regs, TRI))
    Regs.push_back(Reg);
}

static SmallVector<Register, 8>
getExposedUses(outliner::Candidate &C, const TargetRegisterInfo &TRI) {
  SmallVector<Register, 8> UseRegs;

  for (auto It = C.end(); It != C.begin();) {
    --It;

    SmallVector<Register, 8> InstrUseRegs;
    for (MachineOperand &MOP : It->operands()) {
      if (!MOP.isReg() || !MOP.getReg().isPhysical())
        continue;

      Register Reg = MOP.getReg();
      if (MOP.isDef()) {
        if (!regOverlapsAny(Reg, InstrUseRegs, TRI))
          llvm::erase_if(UseRegs, [&](Register UseReg) {
            return TRI.regsOverlap(Reg, UseReg);
          });
      } else if (!MOP.isUndef()) {
        addUniqueReg(UseRegs, Reg, TRI);
        addUniqueReg(InstrUseRegs, Reg, TRI);
      }
    }
  }

  return UseRegs;
}

static bool isLiveInToBlock(Register Reg, const MachineBasicBlock &MBB,
                            const TargetRegisterInfo &TRI) {
  for (const MachineBasicBlock::RegisterMaskPair &LiveIn : MBB.liveins())
    if (TRI.regsOverlap(Reg, Register(LiveIn.PhysReg)))
      return true;
  return false;
}

static bool hasPriorDef(Register Reg, outliner::Candidate &C,
                        const TargetRegisterInfo &TRI) {
  MachineBasicBlock &MBB = *C.getMBB();
  for (auto It = MBB.begin(), End = C.begin(); It != End; ++It)
    for (MachineOperand &MOP : It->operands())
      if (MOP.isReg() && MOP.isDef() && MOP.getReg().isPhysical() &&
          TRI.regsOverlap(Reg, MOP.getReg()))
        return true;

  return false;
}

static bool candidateDefinesRegisterAlias(outliner::Candidate &C, Register Reg,
                                          const TargetRegisterInfo &TRI) {
  for (MachineInstr &MI : C)
    for (MachineOperand &MOP : MI.operands())
      if (MOP.isReg() && MOP.isDef() && MOP.getReg().isPhysical() &&
          TRI.regsOverlap(Reg, MOP.getReg()))
        return true;

  return false;
}

static void addPairAliasDefs(MachineInstrBuilder &MIB, outliner::Candidate &C,
                             const TargetRegisterInfo &TRI) {
  // The generic outliner adds implicit operands using exact register numbers.
  // Add pair aliases so a tail jump that outlines B/C definitions can also
  // satisfy a cloned RET's implicit BC use.
  if (candidateDefinesRegisterAlias(C, Register(I8085::BC), TRI))
    MIB.addReg(I8085::BC, RegState::ImplicitDefine);
  if (candidateDefinesRegisterAlias(C, Register(I8085::DE), TRI))
    MIB.addReg(I8085::DE, RegState::ImplicitDefine);
  if (candidateDefinesRegisterAlias(C, Register(I8085::HL), TRI))
    MIB.addReg(I8085::HL, RegState::ImplicitDefine);
}

static bool hasSafeTailCallReplacement(outliner::Candidate &C,
                                       const TargetRegisterInfo &TRI) {
  MachineBasicBlock &MBB = *C.getMBB();

  for (Register Reg : getExposedUses(C, TRI)) {
    if (TRI.regsOverlap(Reg, Register(I8085::SP)))
      continue;

    if (!isLiveInToBlock(Reg, MBB, TRI) && !hasPriorDef(Reg, C, TRI))
      return false;
  }

  return true;
}

bool I8085InstrInfo::isFunctionSafeToOutlineFrom(
    MachineFunction &MF, bool OutlineFromLinkOnceODRs) const {
  const Function &F = MF.getFunction();

  if (!OutlineFromLinkOnceODRs && F.hasLinkOnceODRLinkage())
    return false;
  if (F.hasSection())
    return false;
  if (F.hasFnAttribute(Attribute::Naked))
    return false;

  return true;
}

bool I8085InstrInfo::shouldOutlineFromFunctionByDefault(
    MachineFunction &MF) const {
  return MF.getFunction().hasMinSize();
}

std::optional<outliner::OutlinedFunction>
I8085InstrInfo::getOutliningCandidateInfo(
    const MachineModuleInfo &MMI,
    std::vector<outliner::Candidate> &RepeatedSequenceLocs) const {
  if (RepeatedSequenceLocs.empty())
    return std::nullopt;

  const TargetRegisterInfo *TRI =
      RepeatedSequenceLocs[0].getMF()->getSubtarget().getRegisterInfo();

  llvm::erase_if(RepeatedSequenceLocs, [](outliner::Candidate &C) {
    return !hasSafeStackRelativeFixups(C);
  });

  if (RepeatedSequenceLocs.size() < 2)
    return std::nullopt;

  if (RepeatedSequenceLocs[0].back().getOpcode() == I8085::RET) {
    llvm::erase_if(RepeatedSequenceLocs, [&](outliner::Candidate &C) {
      return !hasSafeTailCallReplacement(C, *TRI);
    });

    if (RepeatedSequenceLocs.size() < 2)
      return std::nullopt;
  }

  unsigned SequenceSize = 0;
  for (MachineInstr &MI : RepeatedSequenceLocs[0])
    SequenceSize += getInstSizeInBytes(MI);

  constexpr unsigned BranchOverhead = 3;

  if (RepeatedSequenceLocs[0].back().getOpcode() == I8085::RET) {
    for (outliner::Candidate &C : RepeatedSequenceLocs)
      C.setCallInfo(MachineOutlinerTailCall, BranchOverhead);

    return outliner::OutlinedFunction(RepeatedSequenceLocs, SequenceSize, 0,
                                      MachineOutlinerTailCall);
  }

  // I8085 CALL addr16 is 3 bytes and does not modify condition flags.
  for (outliner::Candidate &C : RepeatedSequenceLocs)
    C.setCallInfo(MachineOutlinerDefault, BranchOverhead);

  // Default outlined functions return with RET, a 1-byte instruction.
  constexpr unsigned FrameOverhead = 1;
  return outliner::OutlinedFunction(RepeatedSequenceLocs, SequenceSize,
                                    FrameOverhead, MachineOutlinerDefault);
}

outliner::InstrType
I8085InstrInfo::getOutliningTypeImpl(const MachineModuleInfo &MMI,
                                     MachineBasicBlock::iterator &MBBI,
                                     unsigned Flags) const {
  MachineInstr &MI = *MBBI;
  const MachineFunction *MF = MI.getMF();
  const TargetRegisterInfo *TRI = MF->getSubtarget().getRegisterInfo();

  if (MI.isDebugInstr() || MI.isPosition() || MI.isCFIInstruction())
    return outliner::InstrType::Invisible;

  if (MI.getOpcode() == I8085::RET)
    return outliner::InstrType::LegalTerminator;

  if (MI.isInlineAsm() || MI.isCall() || MI.isBranch() || MI.isReturn() ||
      MI.isTerminator() || MI.isBarrier())
    return outliner::InstrType::Illegal;

  if (MI.getDesc().isPseudo())
    return outliner::InstrType::Illegal;

  if ((MI.mayLoad() || MI.mayStore()) &&
      !isI8085OutlinerMemoryOpcode(MI.getOpcode()))
    return outliner::InstrType::Illegal;

  // Most patternless I8085 real instructions are conservatively tagged with
  // unmodeled side effects. Keep outlining useful, but only for an explicit
  // set of register/immediate operations and selected HL memory operations.
  if (MI.hasUnmodeledSideEffects() &&
      !isI8085OutlinerOpcode(MI.getOpcode()))
    return outliner::InstrType::Illegal;

  // CALL pushes a 2-byte return address. Only allow DAD SP when
  // getOutliningCandidateInfo can prove the candidate contains the immediately
  // preceding LXI H,imm, which buildOutlinedFrame then repairs by +2.
  if ((MI.readsRegister(I8085::SP, TRI) ||
       MI.modifiesRegister(I8085::SP, TRI)) &&
      !isDADSP(MI))
    return outliner::InstrType::Illegal;

  return outliner::InstrType::Legal;
}

void I8085InstrInfo::buildOutlinedFrame(
    MachineBasicBlock &MBB, MachineFunction &MF,
    const outliner::OutlinedFunction &OF) const {
  if (OF.FrameConstructionID == MachineOutlinerDefault) {
    for (auto It = MBB.begin(), End = MBB.end(); It != End; ++It) {
      if (!isDADSP(*It))
        continue;

      assert(It != MBB.begin() &&
             "DAD SP candidate should include a preceding LXI H,imm");
      MachineInstr &LXI = *std::prev(It);
      assert(isLXIHLImm(LXI) && "DAD SP candidate should have LXI H,imm");
      MachineOperand &Offset = LXI.getOperand(1);
      Offset.setImm(Offset.getImm() + 2);
    }

    MBB.insert(MBB.end(), BuildMI(MF, DebugLoc(), get(I8085::RET)));
    return;
  }

  assert(OF.FrameConstructionID == MachineOutlinerTailCall &&
         "unexpected I8085 outliner frame construction ID");
}

MachineBasicBlock::iterator I8085InstrInfo::insertOutlinedCall(
    Module &M, MachineBasicBlock &MBB, MachineBasicBlock::iterator &It,
    MachineFunction &MF, outliner::Candidate &C) const {
  if (C.CallConstructionID == MachineOutlinerTailCall) {
    MachineInstrBuilder MIB =
        BuildMI(MF, DebugLoc(), get(I8085::TCRETURN))
            .addGlobalAddress(M.getNamedValue(MF.getName()))
            .addImm(0);
    const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
    addPairAliasDefs(MIB, C, TRI);
    It = MBB.insert(It, MIB);
  } else
    It = MBB.insert(It, BuildMI(MF, DebugLoc(), get(I8085::CALL_OUTLINE))
                            .addGlobalAddress(M.getNamedValue(MF.getName())));
  return It;
}

} // end of namespace llvm
