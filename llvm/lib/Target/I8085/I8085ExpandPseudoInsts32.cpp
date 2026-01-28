//===-- I8085ExpandPseudoInsts.cpp - Expand pseudo instructions -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that expands pseudo instructions into target
// instructions. This pass should be run after register allocation but before
// the post-regalloc scheduling pass.
//
//===----------------------------------------------------------------------===//

#include "I8085.h"
#include "I8085InstrInfo.h"
#include "I8085MachineFunctionInfo.h"
#include "I8085TargetMachine.h"
#include "MCTargetDesc/I8085MCTargetDesc.h"

#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include <stdint.h>

#include <array>
#include <iostream>


#define DEBUG_TYPE "i8085-expand-psuedo-32"


using namespace llvm;

#define I8085_EXPAND_PSEUDO_32_NAME "I8085 pseudo instruction expansion pass for instructions with 32 bit imaginary registers"

namespace {

/// Expands "placeholder" instructions which uses 32 bit imaginary registers marked as pseudo into
/// actual I8085 instructions.
class I8085ExpandPseudo32 : public MachineFunctionPass {
public:
  static char ID;

  I8085ExpandPseudo32() : MachineFunctionPass(ID) {
    initializeI8085ExpandPseudo32Pass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return I8085_EXPAND_PSEUDO_32_NAME; }

private:
  typedef MachineBasicBlock Block;
  typedef Block::iterator BlockIt;

  const I8085RegisterInfo *TRI;
  const TargetInstrInfo *TII;
  int ScratchFI = -1;
  int64_t ScratchBaseOffset = 0;
  bool HaveScratch = false;


  bool expandMBB(Block &MBB);
  bool expandMI(Block &MBB, BlockIt MBBI);
  template <unsigned OP> bool expand(Block &MBB, BlockIt MBBI);
  bool binOperationWithImmediateOperand(unsigned opCode, Block &MBB, BlockIt MBBI);
  bool binOperation(unsigned opCode, Block &MBB, BlockIt MBBI);
  int64_t getScratchOffset(unsigned Reg, int ByteIndex) const;
  void emitScratchAddr(Block &MBB, BlockIt MBBI, unsigned Reg, int ByteIndex);
  void emitScratchLoad(Block &MBB, BlockIt MBBI, unsigned Reg, int ByteIndex,
                       unsigned DestReg);
  void emitScratchStore(Block &MBB, BlockIt MBBI, unsigned Reg, int ByteIndex,
                        unsigned SrcReg);
  void emitScratchAdvance(Block &MBB, BlockIt MBBI, int Delta);

  MachineInstrBuilder buildMI(Block &MBB, BlockIt MBBI, unsigned Opcode) {
    return BuildMI(MBB, MBBI, MBBI->getDebugLoc(), TII->get(Opcode));
  }

  MachineInstrBuilder buildMI(Block &MBB, BlockIt MBBI, unsigned Opcode,
                              Register DstReg) {
    return BuildMI(MBB, MBBI, MBBI->getDebugLoc(), TII->get(Opcode), DstReg);
  }

  MachineRegisterInfo &getRegInfo(Block &MBB) {
    return MBB.getParent()->getRegInfo();
  }

};

char I8085ExpandPseudo32::ID = 0;

static void addAddrOperand(MachineInstrBuilder &MIB,
                           const MachineOperand &MO,
                           int64_t Offset = 0) {
  switch (MO.getType()) {
  case MachineOperand::MO_Immediate:
    MIB.addImm(MO.getImm() + Offset);
    break;
  case MachineOperand::MO_GlobalAddress:
    MIB.addGlobalAddress(MO.getGlobal(), MO.getOffset() + Offset,
                         MO.getTargetFlags());
    break;
  case MachineOperand::MO_ExternalSymbol:
    assert((MO.getOffset() + Offset) == 0 &&
           "external symbol does not support offsets");
    MIB.addExternalSymbol(MO.getSymbolName(), MO.getTargetFlags());
    break;
  case MachineOperand::MO_ConstantPoolIndex:
    MIB.addConstantPoolIndex(MO.getIndex(), MO.getOffset() + Offset,
                             MO.getTargetFlags());
    break;
  case MachineOperand::MO_BlockAddress:
    MIB.addBlockAddress(MO.getBlockAddress(), MO.getOffset() + Offset,
                        MO.getTargetFlags());
    break;
  default:
    llvm_unreachable("unexpected address operand type");
  }
}

static std::array<uint8_t, 4> splitImm32(uint64_t Imm) {
  return {static_cast<uint8_t>(Imm & 0xFF),
          static_cast<uint8_t>((Imm >> 8) & 0xFF),
          static_cast<uint8_t>((Imm >> 16) & 0xFF),
          static_cast<uint8_t>((Imm >> 24) & 0xFF)};
}

int64_t I8085ExpandPseudo32::getScratchOffset(unsigned Reg,
                                              int ByteIndex) const {
  assert(HaveScratch && "GR32 scratch not initialized");
  int Base = (Reg == I8085::IBX) ? 4 : 0;
  return ScratchBaseOffset + Base + ByteIndex;
}

void I8085ExpandPseudo32::emitScratchAddr(Block &MBB, BlockIt MBBI,
                                          unsigned Reg, int ByteIndex) {
  int64_t Offset = getScratchOffset(Reg, ByteIndex);
  buildMI(MBB, MBBI, I8085::LXI)
      .addReg(I8085::HL, RegState::Define)
      .addImm(Offset);
  buildMI(MBB, MBBI, I8085::DAD).addReg(I8085::SP);
}

void I8085ExpandPseudo32::emitScratchLoad(Block &MBB, BlockIt MBBI,
                                          unsigned Reg, int ByteIndex,
                                          unsigned DestReg) {
  emitScratchAddr(MBB, MBBI, Reg, ByteIndex);
  buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(DestReg, RegState::Define);
}

void I8085ExpandPseudo32::emitScratchStore(Block &MBB, BlockIt MBBI,
                                           unsigned Reg, int ByteIndex,
                                           unsigned SrcReg) {
  emitScratchAddr(MBB, MBBI, Reg, ByteIndex);
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(SrcReg);
}

void I8085ExpandPseudo32::emitScratchAdvance(Block &MBB, BlockIt MBBI,
                                             int Delta) {
  if (Delta == 0)
    return;
  unsigned Opc = (Delta > 0) ? I8085::INX : I8085::DCX;
  int Steps = (Delta > 0) ? Delta : -Delta;
  for (int i = 0; i < Steps; ++i)
    buildMI(MBB, MBBI, Opc).addReg(I8085::HL, RegState::Define);
}

bool I8085ExpandPseudo32::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  BlockIt MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    BlockIt NMBBI = std::next(MBBI);
    Modified |= expandMI(MBB, MBBI);
    MBBI = NMBBI;
  }

  return Modified;
}

bool I8085ExpandPseudo32::runOnMachineFunction(MachineFunction &MF) {

  LLVM_DEBUG({
    dbgs() << "********** Expand 32 bit register pseudo instructions **********\n"
           << "********** Function: " << MF.getName() << '\n';
  });

  bool Modified = false;

  const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
  TRI = STI.getRegisterInfo();
  TII = STI.getInstrInfo();
  I8085MachineFunctionInfo *AFI = MF.getInfo<I8085MachineFunctionInfo>();
  ScratchFI = AFI->getGR32ScratchFI();
  if (ScratchFI >= 0) {
    const MachineFrameInfo &MFI = MF.getFrameInfo();
    const TargetFrameLowering *TFI = STI.getFrameLowering();
    ScratchBaseOffset = MFI.getObjectOffset(ScratchFI) +
                        MFI.getStackSize() - TFI->getOffsetOfLocalArea();
    HaveScratch = true;
  }

  // We need to track liveness in order to use register scavenging.
  MF.getProperties().set(MachineFunctionProperties::Property::TracksLiveness);

  auto UsesPhysReg = [&](unsigned Reg) {
    for (auto &MBB : MF) {
      for (auto &MI : MBB) {
        for (auto &MO : MI.operands()) {
          if (MO.isReg() && MO.getReg() == Reg)
            return true;
        }
      }
    }
    return false;
  };

  bool UsesIAX = UsesPhysReg(I8085::IAX);
  bool UsesIBX = UsesPhysReg(I8085::IBX);
  if ((UsesIAX || UsesIBX) && !HaveScratch) {
    // Fallback for -run-pass tests where PEI doesn't run.
    MachineFrameInfo &MFI = MF.getFrameInfo();
    if (MFI.getObjectIndexBegin() == MFI.getObjectIndexEnd() &&
        MFI.getStackSize() == 0) {
      ScratchFI = MFI.CreateStackObject(8, Align(1), false);
      ScratchBaseOffset = 0;
      HaveScratch = true;
      AFI->setGR32ScratchFI(ScratchFI);
    } else {
      report_fatal_error("I8085 GR32 scratch slot not allocated");
    }
  }
  if (UsesIAX || UsesIBX) {
    MachineBasicBlock &Entry = MF.front();
    auto InsertPt = Entry.begin();
    DebugLoc DL = (InsertPt != Entry.end()) ? InsertPt->getDebugLoc() : DebugLoc();
    if (UsesIAX)
      BuildMI(Entry, InsertPt, DL, TII->get(TargetOpcode::IMPLICIT_DEF), I8085::IAX);
    if (UsesIBX)
      BuildMI(Entry, InsertPt, DL, TII->get(TargetOpcode::IMPLICIT_DEF), I8085::IBX);
  }

  for (Block &MBB : MF) {
    bool ContinueExpanding = true;
    unsigned ExpandCount = 0;

    // Continue expanding the block until all pseudos are expanded.
    do {
      assert(ExpandCount < 10 && "pseudo expand limit reached");

      bool BlockModified = expandMBB(MBB);
      Modified |= BlockModified;
      ExpandCount++;

      ContinueExpanding = BlockModified;
    } while (ContinueExpanding);
  }

  for (Block &MBB : MF) {
    for (auto MBBI = MBB.begin(), E = MBB.end(); MBBI != E; ) {
      auto *MI = &*MBBI++;
      if (!MI->getDesc().isReturn())
        continue;

      bool UsesIAX = (MI->findRegisterUseOperandIdx(I8085::IAX, TRI) != -1);
      bool UsesIBX = (MI->findRegisterUseOperandIdx(I8085::IBX, TRI) != -1);
      if (UsesIAX)
        BuildMI(MBB, MI, MI->getDebugLoc(),
                TII->get(TargetOpcode::IMPLICIT_DEF), I8085::IAX);
      if (UsesIBX)
        BuildMI(MBB, MI, MI->getDebugLoc(),
                TII->get(TargetOpcode::IMPLICIT_DEF), I8085::IBX);
    }
  }

  return Modified;
}

bool I8085ExpandPseudo32::binOperationWithImmediateOperand(unsigned opCode, Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(1).getReg();
  uint64_t immToAdd = MI.getOperand(2).getImm();

  auto values = splitImm32(immToAdd);

  emitScratchAddr(MBB, MBBI, operandOne, 0);
  for (int i = 0; i < 4; ++i) {
      buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A, RegState::Define);
      buildMI(MBB, MBBI, opCode).addImm(values[i]);
      buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);
      if (i != 3)
        emitScratchAdvance(MBB, MBBI, 1);
  }

  MI.eraseFromParent();
  return true;
}

bool I8085ExpandPseudo32::binOperation(unsigned opCode, Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned operandTwo = MI.getOperand(2).getReg();
  
  for(int i=0;i<4;i++){
      emitScratchLoad(MBB, MBBI, operandOne, i, I8085::A);
      emitScratchAddr(MBB, MBBI, operandTwo, i);
      buildMI(MBB, MBBI, opCode);
      emitScratchStore(MBB, MBBI, destReg, i, I8085::A);
  }

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo32::expand<I8085::XOR_32>(Block &MBB, BlockIt MBBI) {
  return binOperation(I8085::XRA_M,MBB,MBBI);
}
template <> bool I8085ExpandPseudo32::expand<I8085::OR_32>(Block &MBB, BlockIt MBBI) {
  return binOperation(I8085::ORA_M,MBB,MBBI);
}
template <> bool I8085ExpandPseudo32::expand<I8085::AND_32>(Block &MBB, BlockIt MBBI) {
  return binOperation(I8085::ANA_M,MBB,MBBI);
}

template <> bool I8085ExpandPseudo32::expand<I8085::XORI_32>(Block &MBB, BlockIt MBBI) {
  return binOperationWithImmediateOperand(I8085::XRI,MBB,MBBI);
}
template <> bool I8085ExpandPseudo32::expand<I8085::ORI_32>(Block &MBB, BlockIt MBBI) {
  return binOperationWithImmediateOperand(I8085::ORI,MBB,MBBI);
}
template <> bool I8085ExpandPseudo32::expand<I8085::ANDI_32>(Block &MBB, BlockIt MBBI) {
  return binOperationWithImmediateOperand(I8085::ANI,MBB,MBBI);
}


template <> bool I8085ExpandPseudo32::expand<I8085::RR_32>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned srcReg = MI.getOperand(0).getReg();

  // Clear carry before rotate-through-carry sequence.
  buildMI(MBB, MBBI, I8085::XRA).addReg(I8085::A);

  emitScratchAddr(MBB, MBBI, srcReg, 3);
  for(int i=3;i>-1;i--){
    buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A, RegState::Define);
    buildMI(MBB, MBBI, I8085::RAR);
    buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);
    if (i != 0)
      emitScratchAdvance(MBB, MBBI, -1);
  }

  emitScratchAddr(MBB, MBBI, srcReg, 3);
  buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A, RegState::Define);
  buildMI(MBB, MBBI, I8085::ANI).addImm(127);  
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);
  
  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo32::expand<I8085::ASR_32>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned srcReg = MI.getOperand(0).getReg();

  // Set carry from sign bit of the high byte.
  emitScratchAddr(MBB, MBBI, srcReg, 3);
  buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A, RegState::Define);
  buildMI(MBB, MBBI, I8085::RLC);

  // Shift high byte through carry to preserve sign.
  emitScratchAddr(MBB, MBBI, srcReg, 3);
  buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A, RegState::Define);
  buildMI(MBB, MBBI, I8085::RAR);
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);

  emitScratchAdvance(MBB, MBBI, -1);
  for(int i=2;i>-1;i--){
    buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A, RegState::Define);
    buildMI(MBB, MBBI, I8085::RAR);
    buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);
    if (i != 0)
      emitScratchAdvance(MBB, MBBI, -1);
  }

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo32::expand<I8085::STORE_32_ADDR_CONTENT>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned addrReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();

  bool addrIsHL = (addrReg == I8085::HL);
  bool addrIsSP = (addrReg == I8085::SP);
  unsigned addrLow = 0, addrHigh = 0;
  bool haveAddr = true;
  if (!addrIsSP) {
    if (addrReg == I8085::BC) {
      addrLow = I8085::C;
      addrHigh = I8085::B;
    } else if (addrReg == I8085::DE) {
      addrLow = I8085::E;
      addrHigh = I8085::D;
    } else if (addrReg == I8085::HL) {
      addrLow = I8085::L;
      addrHigh = I8085::H;
    } else {
      haveAddr = false;
    }
  }

  if (!haveAddr)
    return false;

  if (addrIsHL)
    buildMI(MBB, MBBI, I8085::PUSH).addReg(I8085::HL);

  for(int i=0;i<4;i++){
    // Load byte i from the 32-bit source.
    emitScratchLoad(MBB, MBBI, srcReg, i, I8085::A);

    if (addrIsHL) {
      buildMI(MBB, MBBI, I8085::POP).addReg(I8085::HL);
    } else if (addrIsSP) {
      buildMI(MBB, MBBI, I8085::LXI).addReg(I8085::HL,RegState::Define).addImm(0);
      buildMI(MBB, MBBI, I8085::DAD).addReg(I8085::SP);
    } else {
      buildMI(MBB, MBBI, I8085::MOV).addReg(I8085::H, RegState::Define).addReg(addrHigh);
      buildMI(MBB, MBBI, I8085::MOV).addReg(I8085::L, RegState::Define).addReg(addrLow);
    }

    if (!addrIsHL) {
      for (int j = 0; j < i; ++j)
        buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
    }

    buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);

    if (addrIsHL && i < 3) {
      buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
      buildMI(MBB, MBBI, I8085::PUSH).addReg(I8085::HL);
    }
  }

  MI.eraseFromParent();
  return true;
}


template <> bool I8085ExpandPseudo32::expand<I8085::RL_32>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned srcReg = MI.getOperand(0).getReg();

  // Clear carry before rotate-through-carry sequence.
  buildMI(MBB, MBBI, I8085::XRA).addReg(I8085::A);

  emitScratchAddr(MBB, MBBI, srcReg, 0);
  for(int i=0;i<4;i++){
    buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A, RegState::Define);
    buildMI(MBB, MBBI, I8085::RAL);
    buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);
    if (i != 3)
      emitScratchAdvance(MBB, MBBI, 1);
  }

  emitScratchAddr(MBB, MBBI, srcReg, 0);
  buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A, RegState::Define);
  buildMI(MBB, MBBI, I8085::ANI).addImm(254);  
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}


template <> bool I8085ExpandPseudo32::expand<I8085::SEXT32_INREG_8>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned srcReg = MI.getOperand(0).getReg();
  emitScratchLoad(MBB, MBBI, srcReg, 0, I8085::A);

  buildMI(MBB, MBBI, I8085::ADI)
    .addImm(128); // 80h in hex .. [Adding 80h will set carry flag if HSB is high]

  buildMI(MBB, MBBI, I8085::SBB)
    .addReg(I8085::A);  // will result in FFh if CF set, 0 else

  emitScratchAddr(MBB, MBBI, srcReg, 1);
  for (int i = 0; i < 3; ++i) {
    buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);
    if (i != 2)
      emitScratchAdvance(MBB, MBBI, 1);
  }

  MI.eraseFromParent();
  return true;
}


template <> bool I8085ExpandPseudo32::expand<I8085::SEXT32_INREG_16>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned srcReg = MI.getOperand(0).getReg();
  emitScratchLoad(MBB, MBBI, srcReg, 1, I8085::A);

  buildMI(MBB, MBBI, I8085::ADI)
    .addImm(128); // 80h in hex .. [Adding 80h will set carry flag if HSB is high]

  buildMI(MBB, MBBI, I8085::SBB)
    .addReg(I8085::A);  // will result in FFh if CF set, 0 else

  emitScratchAddr(MBB, MBBI, srcReg, 2);
  for (int i = 0; i < 2; ++i) {
    buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);
    if (i != 1)
      emitScratchAdvance(MBB, MBBI, 1);
  }

  MI.eraseFromParent();
  return true;
}


template <> bool I8085ExpandPseudo32::expand<I8085::TRUNC32TO16>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();

  unsigned destLow = 0, destHigh = 0;

  if(destReg==I8085::BC){  destLow=I8085::C;  destHigh=I8085::B; }
  if(destReg==I8085::DE){  destLow=I8085::E;  destHigh=I8085::D; }
  if(destReg==I8085::HL){  destLow=I8085::L;  destHigh=I8085::H; }

  if (destReg == I8085::HL) {
    emitScratchLoad(MBB, MBBI, srcReg, 0, destLow);
    emitScratchLoad(MBB, MBBI, srcReg, 1, destHigh);
  } else {
    emitScratchAddr(MBB, MBBI, srcReg, 0);
    buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(destLow, RegState::Define);
    emitScratchAdvance(MBB, MBBI, 1);
    buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(destHigh, RegState::Define);
  }

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo32::expand<I8085::SEXT16TO32>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();

  bool addrIsSP = (srcReg == I8085::SP);
  unsigned opOneLow = 0, opOneHigh = 0;
  bool haveAddr = true;
  if (!addrIsSP) {
    if (srcReg == I8085::BC) {
      opOneLow = I8085::C;
      opOneHigh = I8085::B;
    } else if (srcReg == I8085::DE) {
      opOneLow = I8085::E;
      opOneHigh = I8085::D;
    } else if (srcReg == I8085::HL) {
      opOneLow = I8085::L;
      opOneHigh = I8085::H;
    } else {
      haveAddr = false;
    }
  }

  emitScratchAddr(MBB, MBBI, destReg, 0);
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(opOneLow);
  emitScratchAdvance(MBB, MBBI, 1);
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(opOneHigh);

  buildMI(MBB, MBBI, I8085::MOV).addReg(I8085::A,RegState::Define).addReg(opOneHigh);

  buildMI(MBB, MBBI, I8085::ADI)
    .addImm(128); // 80h in hex .. [Adding 80h will set carry flag if HSB is high]

  buildMI(MBB, MBBI, I8085::SBB)
    .addReg(I8085::A);  // will result in FFh if CF set, 0 else

  emitScratchAddr(MBB, MBBI, destReg, 2);
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);
  emitScratchAdvance(MBB, MBBI, 1);
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}


template <> bool I8085ExpandPseudo32::expand<I8085::ZEXT16TO32>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();

  unsigned opOneLow = 0, opOneHigh = 0;

  if(srcReg==I8085::BC){  opOneLow=I8085::C;  opOneHigh=I8085::B; }
  if(srcReg==I8085::DE){  opOneLow=I8085::E;  opOneHigh=I8085::D; }
  if(srcReg==I8085::HL){  opOneLow=I8085::L;  opOneHigh=I8085::H; }

  emitScratchAddr(MBB, MBBI, destReg, 0);
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(opOneLow);
  emitScratchAdvance(MBB, MBBI, 1);
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(opOneHigh);
  emitScratchAdvance(MBB, MBBI, 1);
  buildMI(MBB, MBBI, I8085::MVI_M).addImm(0);
  emitScratchAdvance(MBB, MBBI, 1);
  buildMI(MBB, MBBI, I8085::MVI_M).addImm(0);

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo32::expand<I8085::AEXT16TO32>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  return expand<I8085::ZEXT16TO32>(MBB, MI);
}


template <> bool I8085ExpandPseudo32::expand<I8085::TRUNC32TO8>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();
  emitScratchLoad(MBB, MBBI, srcReg, 0, destReg);

  MI.eraseFromParent();
  return true;
}


template <> bool I8085ExpandPseudo32::expand<I8085::SEXT8TO32>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();
  emitScratchAddr(MBB, MBBI, destReg, 0);
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(srcReg);

  buildMI(MBB, MBBI, I8085::MOV).addReg(I8085::A,RegState::Define).addReg(srcReg);

  buildMI(MBB, MBBI, I8085::ADI)
    .addImm(128); // 80h in hex .. [Adding 80h will set carry flag if HSB is high]

  buildMI(MBB, MBBI, I8085::SBB)
    .addReg(I8085::A);  // will result in FFh if CF set, 0 else


  emitScratchAddr(MBB, MBBI, destReg, 1);
  for (int i = 0; i < 3; ++i) {
    buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);
    if (i != 2)
      emitScratchAdvance(MBB, MBBI, 1);
  }

  MI.eraseFromParent();
  return true;
}


template <> bool I8085ExpandPseudo32::expand<I8085::ZEXT8TO32>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();
  emitScratchAddr(MBB, MBBI, destReg, 0);
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(srcReg);
  emitScratchAdvance(MBB, MBBI, 1);
  buildMI(MBB, MBBI, I8085::MVI_M).addImm(0);
  emitScratchAdvance(MBB, MBBI, 1);
  buildMI(MBB, MBBI, I8085::MVI_M).addImm(0);
  emitScratchAdvance(MBB, MBBI, 1);
  buildMI(MBB, MBBI, I8085::MVI_M).addImm(0);

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo32::expand<I8085::AEXT8TO32>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  return expand<I8085::ZEXT8TO32>(MBB, MI);
}

template <> bool I8085ExpandPseudo32::expand<I8085::MOV_32>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();
  for(int i=0;i<4;i++){
      emitScratchLoad(MBB, MBBI, srcReg, i, I8085::A);
      emitScratchStore(MBB, MBBI, destReg, i, I8085::A);
  }

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo32::expand<I8085::STORE_32>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned offsetToStore = MI.getOperand(1).getImm();

  unsigned srcReg = MI.getOperand(2).getReg();
  
  for(int i=0;i<4;i++){
      emitScratchLoad(MBB, MBBI, srcReg, i, I8085::A);
      buildMI(MBB, MBBI, I8085::LXI).addReg(I8085::HL,RegState::Define).addImm(offsetToStore+i);
      buildMI(MBB, MBBI, I8085::DAD).addReg(I8085::SP);
      buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);
  }

  MI.eraseFromParent();
  return true;
}


template <> bool I8085ExpandPseudo32::expand<I8085::ADD_32>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned operandTwo = MI.getOperand(2).getReg();
  
  for(int i=0;i<4;i++){
      emitScratchLoad(MBB, MBBI, operandOne, i, I8085::A);
      emitScratchAddr(MBB, MBBI, operandTwo, i);
      if(i>0) buildMI(MBB, MBBI, I8085::ADC_M);
      else buildMI(MBB, MBBI, I8085::ADD_M);
      emitScratchStore(MBB, MBBI, destReg, i, I8085::A);
  }

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo32::expand<I8085::SUBI_32>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned destReg = operandOne; 
  uint64_t immToAdd = MI.getOperand(2).getImm();

  auto values = splitImm32(immToAdd);

  emitScratchAddr(MBB, MBBI, operandOne, 0);
  for (int i = 0; i < 4; ++i) {
      buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A, RegState::Define);
      if (i > 0)
        buildMI(MBB, MBBI, I8085::SBI).addImm(values[i]);
      else
        buildMI(MBB, MBBI, I8085::SUI).addImm(values[i]);
      buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);
      if (i != 3)
        emitScratchAdvance(MBB, MBBI, 1);
  }

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo32::expand<I8085::SUB_32>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned destReg = operandOne; 
  unsigned operandTwo = MI.getOperand(2).getReg();
  
  for(int i=0;i<4;i++){
      emitScratchLoad(MBB, MBBI, operandOne, i, I8085::A);
      emitScratchAddr(MBB, MBBI, operandTwo, i);
      if(i>0) buildMI(MBB, MBBI, I8085::SBB_M);
      else buildMI(MBB, MBBI, I8085::SUB_M);
      emitScratchStore(MBB, MBBI, destReg, i, I8085::A);
  }

  MI.eraseFromParent();
  return true;
}


template <> bool I8085ExpandPseudo32::expand<I8085::ADDI_32>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned destReg = operandOne; 
  uint64_t immToAdd = MI.getOperand(2).getImm();

  auto values = splitImm32(immToAdd);

  emitScratchAddr(MBB, MBBI, operandOne, 0);
  for (int i = 0; i < 4; ++i) {
      buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A, RegState::Define);
      if (i > 0)
        buildMI(MBB, MBBI, I8085::ACI).addImm(values[i]);
      else
        buildMI(MBB, MBBI, I8085::ADI).addImm(values[i]);
      buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);
      if (i != 3)
        emitScratchAdvance(MBB, MBBI, 1);
  }

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo32::expand<I8085::LOAD_32_WITH_ADDR>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  uint16_t offsetToLoad = MI.getOperand(2).getImm();
  
  for(int i=0;i<4;i++){
      buildMI(MBB, MBBI, I8085::LXI).addReg(I8085::HL,RegState::Define).addImm(offsetToLoad+i);
      buildMI(MBB, MBBI, I8085::DAD).addReg(I8085::SP);
      buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A,RegState::Define);
      emitScratchStore(MBB, MBBI, destReg, i, I8085::A);
  }

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo32::expand<I8085::LOAD_32>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  uint64_t immToLoad = MI.getOperand(1).getImm();

  auto values = splitImm32(immToLoad);
  
  emitScratchAddr(MBB, MBBI, destReg, 0);
  for (int i = 0; i < 4; ++i) {
      buildMI(MBB, MBBI, I8085::MVI_M).addImm(values[i]);
      if (i != 3)
        emitScratchAdvance(MBB, MBBI, 1);
  }

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo32::expand<I8085::MVI_32>(Block &MBB, BlockIt MBBI) {
  return expand<I8085::LOAD_32>(MBB, MBBI);
}


template <> bool I8085ExpandPseudo32::expand<I8085::JMP_32_IF_NOT_EQUAL>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(0).getReg();
  unsigned operandTwo = MI.getOperand(1).getReg();

  for(int i=0;i<4;i++){
      emitScratchLoad(MBB, MBBI, operandTwo, i, I8085::A);
      emitScratchAddr(MBB, MBBI, operandOne, i);
      buildMI(MBB, MBBI, I8085::CMP_M);
      buildMI(MBB, MBBI, I8085::JNZ).addMBB(MI.getOperand(2).getMBB());
  }            

  MI.eraseFromParent();
  return true;
}


template <> bool I8085ExpandPseudo32::expand<I8085::JMP_32_IF_SAME_SIGN>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(0).getReg();
  unsigned operandTwo = MI.getOperand(1).getReg();
  
  emitScratchLoad(MBB, MBBI, operandTwo, 3, I8085::A);
  emitScratchAddr(MBB, MBBI, operandOne, 3);
  buildMI(MBB, MBBI, I8085::XRA_M);
  buildMI(MBB, MBBI, I8085::ANI).addImm(128);  
  buildMI(MBB, MBBI, I8085::JZ).addMBB(MI.getOperand(2).getMBB());
        
  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo32::expand<I8085::JMP_32_IF_POSITIVE>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(0).getReg();
  
  emitScratchLoad(MBB, MBBI, operandOne, 3, I8085::A);
  buildMI(MBB, MBBI, I8085::ANI).addImm(128);  
  buildMI(MBB, MBBI, I8085::JZ).addMBB(MI.getOperand(1).getMBB());
        
  MI.eraseFromParent();
  return true;
}


template <> bool I8085ExpandPseudo32::expand<I8085::STORE_32_AT_OFFSET_WITH_SP>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned srcReg = MI.getOperand(0).getReg();
  unsigned offsetToStore = MI.getOperand(1).getImm();
  
  for(int i=0;i<4;i++){
      emitScratchLoad(MBB, MBBI, srcReg, i, I8085::A);
      buildMI(MBB, MBBI, I8085::LXI).addReg(I8085::HL,RegState::Define).addImm(offsetToStore+i);
      buildMI(MBB, MBBI, I8085::DAD).addReg(I8085::SP);
      buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::A);
  }

  MI.eraseFromParent();
  return true;
}


template <> bool I8085ExpandPseudo32::expand<I8085::LOAD_32_OFFSET_WITH_SP>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  uint16_t offsetToLoad = MI.getOperand(1).getImm();
  
  for(int i=0;i<4;i++){
      buildMI(MBB, MBBI, I8085::LXI).addReg(I8085::HL,RegState::Define).addImm(offsetToLoad+i);
      buildMI(MBB, MBBI, I8085::DAD).addReg(I8085::SP);
      buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A,RegState::Define);
      emitScratchStore(MBB, MBBI, destReg, i, I8085::A);
  }

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo32::expand<I8085::LOAD_32_WITH_IMM_ADDR>(Block &MBB, BlockIt MBBI) {
  const I8085Subtarget &STI = MBB.getParent()->getSubtarget<I8085Subtarget>();
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();

  const MachineOperand &AddrMO = MI.getOperand(1);
  
  for(int i=0;i<4;i++){
      MachineInstrBuilder Addr =
          buildMI(MBB, MBBI, I8085::LXI).addReg(I8085::HL, RegState::Define);
      addAddrOperand(Addr, AddrMO, i);
      buildMI(MBB, MBBI, I8085::DAD).addReg(I8085::SP);
      buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A,RegState::Define);
      emitScratchStore(MBB, MBBI, destReg, i, I8085::A);
  }

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo32::expand<I8085::LOAD_32_ADDR_CONTENT>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();

  bool addrIsSP = (srcReg == I8085::SP);
  unsigned opOneLow = 0, opOneHigh = 0;
  bool haveAddr = true;
  if (!addrIsSP) {
    if (srcReg == I8085::BC) {
      opOneLow = I8085::C;
      opOneHigh = I8085::B;
    } else if (srcReg == I8085::DE) {
      opOneLow = I8085::E;
      opOneHigh = I8085::D;
    } else if (srcReg == I8085::HL) {
      opOneLow = I8085::L;
      opOneHigh = I8085::H;
    } else {
      haveAddr = false;
    }
  }

  if (!haveAddr)
    return false;

  for(int i=0;i<4;i++){
      if (addrIsSP) {
        buildMI(MBB, MBBI, I8085::LXI).addReg(I8085::HL,RegState::Define).addImm(0);
        buildMI(MBB, MBBI, I8085::DAD).addReg(I8085::SP);
      } else {
        buildMI(MBB, MBBI,  I8085::MOV).addReg(I8085::H, RegState::Define).addReg(opOneHigh);
        buildMI(MBB, MBBI,  I8085::MOV).addReg(I8085::L, RegState::Define).addReg(opOneLow);
      }

      for (int j = 0; j < i; ++j)
        buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL,RegState::Define);

      buildMI(MBB, MBBI,  I8085::MOV_FROM_M).addReg(I8085::A,RegState::Define);

      emitScratchStore(MBB, MBBI, destReg, i, I8085::A);
  }            

  MI.eraseFromParent();
  return true;
}

bool I8085ExpandPseudo32::expandMI(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  int Opcode = MBBI->getOpcode();

#define EXPAND(Op)                                                             \
  case Op:                                                                     \
    return expand<Op>(MBB, MI)

  switch (Opcode) {
    EXPAND(I8085::XORI_32);
    EXPAND(I8085::ORI_32);
    EXPAND(I8085::ANDI_32);
    EXPAND(I8085::XOR_32);
    EXPAND(I8085::OR_32);
    EXPAND(I8085::AND_32);
    EXPAND(I8085::RL_32);
    EXPAND(I8085::RR_32);
    EXPAND(I8085::ASR_32);
    EXPAND(I8085::SEXT32_INREG_16);
    EXPAND(I8085::SEXT32_INREG_8);
    EXPAND(I8085::TRUNC32TO16);
    EXPAND(I8085::SEXT16TO32);
    EXPAND(I8085::AEXT16TO32);
    EXPAND(I8085::ZEXT16TO32);
    EXPAND(I8085::TRUNC32TO8);
    EXPAND(I8085::SEXT8TO32);
    EXPAND(I8085::AEXT8TO32);
    EXPAND(I8085::ZEXT8TO32);
    EXPAND(I8085::LOAD_32_OFFSET_WITH_SP);
    EXPAND(I8085::STORE_32_AT_OFFSET_WITH_SP);
    EXPAND(I8085::STORE_32_ADDR_CONTENT);
    EXPAND(I8085::JMP_32_IF_POSITIVE);
    EXPAND(I8085::JMP_32_IF_SAME_SIGN);
    EXPAND(I8085::JMP_32_IF_NOT_EQUAL);
    EXPAND(I8085::MOV_32);
    EXPAND(I8085::SUBI_32);
    EXPAND(I8085::SUB_32);
    EXPAND(I8085::ADD_32);
    EXPAND(I8085::ADDI_32);
    EXPAND(I8085::LOAD_32);
    EXPAND(I8085::STORE_32);
    EXPAND(I8085::LOAD_32_WITH_ADDR);
    EXPAND(I8085::LOAD_32_WITH_IMM_ADDR);
    EXPAND(I8085::LOAD_32_ADDR_CONTENT);
    EXPAND(I8085::MVI_32);
  }
#undef EXPAND
  return false;
}

} // end of anonymous namespace

INITIALIZE_PASS(I8085ExpandPseudo32, "i8085-expand-pseudo32", I8085_EXPAND_PSEUDO_32_NAME,
                false, false)
namespace llvm { 

FunctionPass *createI8085ExpandPseudo32Pass() {  return new I8085ExpandPseudo32();  }

} // end of namespace llvm
