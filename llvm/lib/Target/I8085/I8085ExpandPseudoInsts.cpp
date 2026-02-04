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
#include "I8085TargetMachine.h"
#include "MCTargetDesc/I8085MCTargetDesc.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include <stdint.h>

using namespace llvm;

#define I8085_EXPAND_PSEUDO_NAME "I8085 pseudo instruction expansion pass"

namespace {

/// Expands "placeholder" instructions marked as pseudo into
/// actual I8085 instructions.
class I8085ExpandPseudo : public MachineFunctionPass {
public:
  static char ID;

  I8085ExpandPseudo() : MachineFunctionPass(ID) {
    initializeI8085ExpandPseudoPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return I8085_EXPAND_PSEUDO_NAME; }

private:
  typedef MachineBasicBlock Block;
  typedef Block::iterator BlockIt;

  const I8085RegisterInfo *TRI;
  const TargetInstrInfo *TII;


  bool expandMBB(Block &MBB);
  bool expandMI(Block &MBB, BlockIt MBBI);
  template <unsigned OP> bool expand(Block &MBB, BlockIt MBBI);

  MachineInstrBuilder buildMI(Block &MBB, BlockIt MBBI, unsigned Opcode) {
    return BuildMI(MBB, MBBI, MBBI->getDebugLoc(), TII->get(Opcode));
  }

  MachineInstrBuilder buildMI(Block &MBB, BlockIt MBBI, unsigned Opcode,
                              Register DstReg) {
    return BuildMI(MBB, MBBI, MBBI->getDebugLoc(), TII->get(Opcode), DstReg);
  }

  void pushPSW(Block &MBB, BlockIt MBBI, bool CanClobberFlags) {
    if (CanClobberFlags)
      buildMI(MBB, MBBI, I8085::ORI).addImm(0);
    buildMI(MBB, MBBI, I8085::PUSH).addReg(I8085::PSW);
  }

  MachineRegisterInfo &getRegInfo(Block &MBB) {
    return MBB.getParent()->getRegInfo();
  }

  bool isPhysRegLive(Block &MBB, BlockIt MBBI, Register Reg) const {
    LivePhysRegs LiveRegs(*TRI);
    LiveRegs.addLiveOuts(MBB);
    for (auto I = MBB.rbegin(), E = MBBI.getReverse(); I != E; ++I)
      LiveRegs.stepBackward(*I);
    return LiveRegs.contains(Reg);
  }

  bool isHLOrSubRegLive(Block &MBB, BlockIt MBBI) const {
    if (isPhysRegLive(MBB, MBBI, I8085::HL) ||
        isPhysRegLive(MBB, MBBI, I8085::H) ||
        isPhysRegLive(MBB, MBBI, I8085::L)) {
      return true;
    }
    return MBB.isLiveIn(I8085::HL) || MBB.isLiveIn(I8085::H) ||
           MBB.isLiveIn(I8085::L);
  }

  bool shouldPreservePSW(Block &MBB, BlockIt MBBI) const {
    return isPhysRegLive(MBB, MBBI, I8085::A) ||
           isPhysRegLive(MBB, MBBI, I8085::SREG);
  }

};

char I8085ExpandPseudo::ID = 0;

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
  case I8085::HL:
    LowReg = I8085::L;
    HighReg = I8085::H;
    return true;
  default:
    return false;
  }
}

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

bool I8085ExpandPseudo::expandMBB(MachineBasicBlock &MBB) {
  for (BlockIt MBBI = MBB.begin(), E = MBB.end(); MBBI != E; ) {
    // Some expansions splice instructions into new blocks, which can invalidate
    // iterators. Restart the scan after any successful expansion.
    if (expandMI(MBB, MBBI))
      return true;
    MBBI = std::next(MBBI);
  }

  return false;
}

bool I8085ExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  bool Modified = false;

  const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
  TRI = STI.getRegisterInfo();
  TII = STI.getInstrInfo();

  // We need to track liveness in order to use register scavenging.
  MF.getProperties().set(MachineFunctionProperties::Property::TracksLiveness);

  for (Block &MBB : MF) {
    bool ContinueExpanding = true;
    unsigned ExpandCount = 0;
    unsigned MaxExpansions = static_cast<unsigned>(MBB.size()) * 20;
    if (MaxExpansions < 500)
      MaxExpansions = 500;

    // Continue expanding the block until all pseudos are expanded.
    do {
      if (ExpandCount++ >= MaxExpansions)
        report_fatal_error("I8085 pseudo expand limit reached");

      bool BlockModified = expandMBB(MBB);
      Modified |= BlockModified;

      ContinueExpanding = BlockModified;
    } while (ContinueExpanding);
  }

  return Modified;
}

template <>
bool I8085ExpandPseudo::expand<I8085::LOAD_16_ADDR_CONTENT>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned srcLowReg,srcHighReg;
  unsigned destLowReg,destHighReg;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();

  if ((srcReg == I8085::BC || srcReg == I8085::DE) && destReg != srcReg) {
    bool DestIsSP = (destReg == I8085::SP);
    if (DestIsSP)
      destReg = I8085::HL;

    if (!getPairRegs(destReg, destLowReg, destHighReg))
      return false;

    const bool PreservePSW = shouldPreservePSW(MBB, MBBI);
    const bool CanClobberFlags =
        !isPhysRegLive(MBB, MBBI, I8085::SREG);
    if (PreservePSW)
      pushPSW(MBB, MBBI, CanClobberFlags);
    buildMI(MBB, MBBI, I8085::LDAX).addReg(srcReg);
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(destLowReg, RegState::Define)
        .addReg(I8085::A);

    buildMI(MBB, MBBI, I8085::INX).addReg(srcReg, RegState::Define);
    buildMI(MBB, MBBI, I8085::LDAX).addReg(srcReg);
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(destHighReg, RegState::Define)
        .addReg(I8085::A);

    if (srcReg != destReg)
      buildMI(MBB, MBBI, I8085::DCX).addReg(srcReg, RegState::Define);

    if (PreservePSW)
      buildMI(MBB, MBBI, I8085::POP).addReg(I8085::PSW, RegState::Define);
    if (DestIsSP)
      buildMI(MBB, MBBI, I8085::SPHL);

    MI.eraseFromParent();
    return true;
  }

  bool HaveSrc = getPairRegs(srcReg, srcLowReg, srcHighReg);
  if (!HaveSrc && srcReg == I8085::SP) {
    buildMI(MBB, MBBI, I8085::LXI)
        .addReg(I8085::HL, RegState::Define)
        .addImm(0);
    buildMI(MBB, MBBI, I8085::DAD).addReg(I8085::SP);
    srcLowReg = I8085::L;
    srcHighReg = I8085::H;
    HaveSrc = true;
  }

  if (!HaveSrc)
    return false;

  // Load via HL pointer.
  if (srcReg != I8085::HL && srcReg != I8085::SP) {
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::H, RegState::Define)
        .addReg(srcHighReg);
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::L, RegState::Define)
        .addReg(srcLowReg);
  }

  if (destReg == I8085::HL || destReg == I8085::SP) {
    const bool PreservePSW = shouldPreservePSW(MBB, MBBI);
    const bool CanClobberFlags =
        !isPhysRegLive(MBB, MBBI, I8085::SREG);
    if (PreservePSW)
      pushPSW(MBB, MBBI, CanClobberFlags);
    buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A, RegState::Define);
    buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
    buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::H, RegState::Define);
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::L, RegState::Define)
        .addReg(I8085::A);
    if (PreservePSW)
      buildMI(MBB, MBBI, I8085::POP).addReg(I8085::PSW, RegState::Define);
    if (destReg == I8085::SP)
      buildMI(MBB, MBBI, I8085::SPHL);
    MI.eraseFromParent();
    return true;
  }

  if (!getPairRegs(destReg, destLowReg, destHighReg))
    return false;

  buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(destLowReg, RegState::Define);
  buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
  buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(destHighReg, RegState::Define);
  if (srcReg == I8085::HL)
    buildMI(MBB, MBBI, I8085::DCX).addReg(I8085::HL, RegState::Define);

  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::LOAD_8_ADDR_CONTENT>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned lowReg,highReg;
  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();

  if (srcReg == I8085::BC || srcReg == I8085::DE) {
    const bool PreservePSW =
        (destReg != I8085::A) && shouldPreservePSW(MBB, MBBI);
    const bool CanClobberFlags =
        !isPhysRegLive(MBB, MBBI, I8085::SREG);
    if (PreservePSW)
      pushPSW(MBB, MBBI, CanClobberFlags);
    buildMI(MBB, MBBI, I8085::LDAX).addReg(srcReg);
    if (destReg != I8085::A) {
      buildMI(MBB, MBBI, I8085::MOV)
          .addReg(destReg, RegState::Define)
          .addReg(I8085::A);
    }
    if (PreservePSW)
      buildMI(MBB, MBBI, I8085::POP).addReg(I8085::PSW, RegState::Define);
    MI.eraseFromParent();
    return true;
  }

  bool HaveSrc = getPairRegs(srcReg, lowReg, highReg);
  if (!HaveSrc && srcReg == I8085::SP) {
    buildMI(MBB, MBBI, I8085::LXI)
        .addReg(I8085::HL, RegState::Define)
        .addImm(0);
    buildMI(MBB, MBBI, I8085::DAD).addReg(I8085::SP);
    lowReg = I8085::L;
    highReg = I8085::H;
    HaveSrc = true;
  }

  if (!HaveSrc)
    return false;

  if (srcReg != I8085::HL && srcReg != I8085::SP) {
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::H, RegState::Define)
        .addReg(highReg);
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::L, RegState::Define)
        .addReg(lowReg);
  }
  buildMI(MBB, MBBI,  I8085::MOV_FROM_M).addReg(destReg ,RegState::Define);
  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::STORE_8_ADDR_CONTENT>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned addrReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();

  if (addrReg == I8085::BC || addrReg == I8085::DE) {
    const bool PreservePSW =
        (srcReg != I8085::A) && shouldPreservePSW(MBB, MBBI);
    const bool CanClobberFlags =
        !isPhysRegLive(MBB, MBBI, I8085::SREG);
    if (PreservePSW)
      pushPSW(MBB, MBBI, CanClobberFlags);
    if (srcReg != I8085::A) {
      buildMI(MBB, MBBI, I8085::MOV)
          .addReg(I8085::A, RegState::Define)
          .addReg(srcReg);
    }
    buildMI(MBB, MBBI, I8085::STAX).addReg(addrReg);
    if (PreservePSW)
      buildMI(MBB, MBBI, I8085::POP).addReg(I8085::PSW, RegState::Define);
    MI.eraseFromParent();
    return true;
  }

  unsigned lowReg, highReg;
  bool HaveAddr = getPairRegs(addrReg, lowReg, highReg);
  if (!HaveAddr && addrReg == I8085::SP) {
    buildMI(MBB, MBBI, I8085::LXI)
        .addReg(I8085::HL, RegState::Define)
        .addImm(0);
    buildMI(MBB, MBBI, I8085::DAD).addReg(I8085::SP);
    lowReg = I8085::L;
    highReg = I8085::H;
    HaveAddr = true;
  }

  if (!HaveAddr)
    return false;

  if (addrReg != I8085::HL && addrReg != I8085::SP) {
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::H, RegState::Define)
        .addReg(highReg);
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::L, RegState::Define)
        .addReg(lowReg);
  }

  buildMI(MBB, MBBI, I8085::MOV_M).addReg(srcReg);

  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::STORE_16_ADDR_CONTENT>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned addrReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();

  if (addrReg == I8085::HL && srcReg == I8085::HL) {
    buildMI(MBB, MBBI, I8085::PUSH).addReg(I8085::BC);
    buildMI(MBB, MBBI, I8085::PUSH).addReg(I8085::HL);
    buildMI(MBB, MBBI, I8085::POP).addReg(I8085::BC, RegState::Define);
    buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::C);
    buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
    buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::B);
    buildMI(MBB, MBBI, I8085::POP).addReg(I8085::BC, RegState::Define);
    MI.eraseFromParent();
    return true;
  }

  if ((addrReg == I8085::BC || addrReg == I8085::DE) && addrReg != srcReg) {
    unsigned srcLow, srcHigh;
    if (!getPairRegs(srcReg, srcLow, srcHigh))
      return false;

    const bool PreservePSW = shouldPreservePSW(MBB, MBBI);
    const bool CanClobberFlags =
        !isPhysRegLive(MBB, MBBI, I8085::SREG);
    if (PreservePSW)
      pushPSW(MBB, MBBI, CanClobberFlags);
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::A, RegState::Define)
        .addReg(srcLow);
    buildMI(MBB, MBBI, I8085::STAX).addReg(addrReg);
    buildMI(MBB, MBBI, I8085::INX).addReg(addrReg, RegState::Define);
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::A, RegState::Define)
        .addReg(srcHigh);
    buildMI(MBB, MBBI, I8085::STAX).addReg(addrReg);
    buildMI(MBB, MBBI, I8085::DCX).addReg(addrReg, RegState::Define);
    if (PreservePSW)
      buildMI(MBB, MBBI, I8085::POP).addReg(I8085::PSW, RegState::Define);

    MI.eraseFromParent();
    return true;
  }

  unsigned addrLow, addrHigh;
  bool HaveAddr = getPairRegs(addrReg, addrLow, addrHigh);
  if (!HaveAddr && addrReg == I8085::SP) {
    buildMI(MBB, MBBI, I8085::LXI)
        .addReg(I8085::HL, RegState::Define)
        .addImm(0);
    buildMI(MBB, MBBI, I8085::DAD).addReg(I8085::SP);
    addrLow = I8085::L;
    addrHigh = I8085::H;
    HaveAddr = true;
  }

  if (!HaveAddr)
    return false;

  unsigned srcLow, srcHigh;
  if (!getPairRegs(srcReg, srcLow, srcHigh))
    return false;

  if (addrReg != I8085::HL && addrReg != I8085::SP) {
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::H, RegState::Define)
        .addReg(addrHigh);
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::L, RegState::Define)
        .addReg(addrLow);
  }

  buildMI(MBB, MBBI, I8085::MOV_M).addReg(srcLow);
  buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(srcHigh);
  if (addrReg == I8085::HL)
    buildMI(MBB, MBBI, I8085::DCX).addReg(I8085::HL, RegState::Define);

  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::CALL_INDIRECT>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  MachineFunction *MF = MBB.getParent();

  const BasicBlock *LLVMBB = MBB.getBasicBlock();
  MachineBasicBlock *ReturnMBB = MF->CreateMachineBasicBlock(LLVMBB);
  auto InsertPos = std::next(MBB.getIterator());
  MF->insert(InsertPos, ReturnMBB);
  // Renumber ALL blocks to avoid conflicts with existing block numbers.
  MF->RenumberBlocks();

  ReturnMBB->splice(ReturnMBB->begin(), &MBB, std::next(MBBI), MBB.end());
  ReturnMBB->transferSuccessorsAndUpdatePHIs(&MBB);
  MBB.addSuccessor(ReturnMBB);
  LivePhysRegs LiveRegs;
  computeAndAddLiveIns(LiveRegs, *ReturnMBB);

  buildMI(MBB, MBBI, I8085::LXI)
      .addReg(I8085::BC, RegState::Define)
      .addMBB(ReturnMBB);
  buildMI(MBB, MBBI, I8085::PUSH).addReg(I8085::BC);
  buildMI(MBB, MBBI, I8085::PCHL);

  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::STORE_8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;


  unsigned srcReg = MI.getOperand(2).getReg();
  unsigned baseReg = MI.getOperand(0).getReg();
  int64_t offsetToStore = MI.getOperand(1).getImm();

  const bool PreserveHL = (baseReg != I8085::HL) &&
                          isHLOrSubRegLive(MBB, MBBI);
  const bool PreserveHLFromSP = PreserveHL && (baseReg == I8085::SP);

  if (PreserveHL) {
    buildMI(MBB, MBBI, I8085::PUSH).addReg(I8085::HL);
    if (PreserveHLFromSP)
      offsetToStore += 2;
  }

  auto addOffsetToHL = [&](int64_t Offset) {
    if (Offset > 0) {
      for (int64_t i = 0; i < Offset; ++i)
        buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
    } else if (Offset < 0) {
      for (int64_t i = 0; i < -Offset; ++i)
        buildMI(MBB, MBBI, I8085::DCX).addReg(I8085::HL, RegState::Define);
    }
  };

  if (baseReg == I8085::HL) {
    addOffsetToHL(offsetToStore);
  } else {
    buildMI(MBB, MBBI, I8085::LXI)
        .addReg(I8085::HL,RegState::Define)
        .addImm(offsetToStore);
    buildMI(MBB, MBBI, I8085::DAD)
        .addReg(baseReg);
  }
  
  /* Store the register value pointed by HL reg */
  
  buildMI(MBB, MBBI, I8085::MOV_M)
      .addReg(srcReg);

  if (baseReg == I8085::HL)
    addOffsetToHL(-offsetToStore);

  if (PreserveHL)
    buildMI(MBB, MBBI, I8085::POP).addReg(I8085::HL, RegState::Define);
  
  MI.eraseFromParent();
  return true;
}


template <>
bool I8085ExpandPseudo::expand<I8085::LOAD_16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  const MachineOperand &Src = MI.getOperand(1);

  MachineInstrBuilder MIB = buildMI(MBB, MBBI, I8085::LXI)
                                .addReg(destReg, RegState::Define);
  addAddrOperand(MIB, Src);

  MI.eraseFromParent();

  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::LOAD_8_WITH_IMM_ADDR>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();

  const MachineOperand &AddrMO = MI.getOperand(1);

  MachineInstrBuilder Addr = buildMI(MBB, MBBI, I8085::LXI)
                                 .addReg(I8085::HL, RegState::Define);
  addAddrOperand(Addr, AddrMO);
  buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(destReg,RegState::Define);

  MI.eraseFromParent();

  return true;
}


template <>
bool I8085ExpandPseudo::expand<I8085::LOAD_16_WITH_IMM_ADDR>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned lowReg,highReg;
  unsigned destReg = MI.getOperand(0).getReg();

  const MachineOperand &AddrMO = MI.getOperand(1);

  if (destReg == I8085::HL || destReg == I8085::SP) {
    const bool PreservePSW = shouldPreservePSW(MBB, MBBI);
    const bool CanClobberFlags =
        !isPhysRegLive(MBB, MBBI, I8085::SREG);
    if (PreservePSW)
      pushPSW(MBB, MBBI, CanClobberFlags);
    MachineInstrBuilder AddrLo = buildMI(MBB, MBBI, I8085::LXI)
                                     .addReg(I8085::HL, RegState::Define);
    addAddrOperand(AddrLo, AddrMO);
    buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A, RegState::Define);
    buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
    buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::H, RegState::Define);
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::L, RegState::Define)
        .addReg(I8085::A);

    if (PreservePSW)
      buildMI(MBB, MBBI, I8085::POP).addReg(I8085::PSW, RegState::Define);
    if (destReg == I8085::SP)
      buildMI(MBB, MBBI, I8085::SPHL);

    MI.eraseFromParent();
    return true;
  }

  if (!getPairRegs(destReg, lowReg, highReg))
    return false;

  MachineInstrBuilder AddrHi = buildMI(MBB, MBBI, I8085::LXI)
                                   .addReg(I8085::HL, RegState::Define);
  addAddrOperand(AddrHi, AddrMO, 1);
  buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(highReg,RegState::Define);

  MachineInstrBuilder AddrLo = buildMI(MBB, MBBI, I8085::LXI)
                                   .addReg(I8085::HL, RegState::Define);
  addAddrOperand(AddrLo, AddrMO);
  buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(lowReg,RegState::Define);

  MI.eraseFromParent();

  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::STORE_8_WITH_IMM_ADDR>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned srcReg = MI.getOperand(1).getReg();
  const MachineOperand &AddrMO = MI.getOperand(0);

  MachineInstrBuilder Addr = buildMI(MBB, MBBI, I8085::LXI)
                                 .addReg(I8085::HL, RegState::Define);
  addAddrOperand(Addr, AddrMO);
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(srcReg);

  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::STORE_16_WITH_IMM_ADDR>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned srcReg = MI.getOperand(1).getReg();
  unsigned srcLow, srcHigh;
  if (!getPairRegs(srcReg, srcLow, srcHigh))
    return false;

  const MachineOperand &AddrMO = MI.getOperand(0);
  MachineInstrBuilder Addr = buildMI(MBB, MBBI, I8085::LXI)
                                 .addReg(I8085::HL, RegState::Define);
  addAddrOperand(Addr, AddrMO);
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(srcLow);
  buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
  buildMI(MBB, MBBI, I8085::MOV_M).addReg(srcHigh);

  MI.eraseFromParent();
  return true;
}


template <>
bool I8085ExpandPseudo::expand<I8085::STORE_16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned lowReg,highReg;
  unsigned baseReg = MI.getOperand(0).getReg();
  int64_t offsetToStore = MI.getOperand(1).getImm();
  unsigned destReg = MI.getOperand(2).getReg();

  auto addOffsetToHL = [&](int64_t Offset) {
    if (Offset > 0) {
      for (int64_t i = 0; i < Offset; ++i)
        buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
    } else if (Offset < 0) {
      for (int64_t i = 0; i < -Offset; ++i)
        buildMI(MBB, MBBI, I8085::DCX).addReg(I8085::HL, RegState::Define);
    }
  };

  if (destReg == I8085::HL) {
    const bool SrcIsKill = MI.getOperand(2).isKill();
    int64_t addrOffset = offsetToStore;
    if (baseReg == I8085::SP)
      addrOffset += 4;

    buildMI(MBB, MBBI, I8085::PUSH).addReg(I8085::BC);
    buildMI(MBB, MBBI, I8085::PUSH).addReg(I8085::HL);

    if (baseReg == I8085::HL) {
      addOffsetToHL(addrOffset);
    } else {
      buildMI(MBB, MBBI, I8085::LXI)
          .addReg(I8085::HL, RegState::Define)
          .addImm(addrOffset);
      buildMI(MBB, MBBI, I8085::DAD).addReg(baseReg);
    }

    buildMI(MBB, MBBI, I8085::POP).addReg(I8085::BC, RegState::Define);
    buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::C);
    buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
    buildMI(MBB, MBBI, I8085::MOV_M).addReg(I8085::B);

    if (!SrcIsKill) {
      buildMI(MBB, MBBI, I8085::MOV)
          .addReg(I8085::H, RegState::Define)
          .addReg(I8085::B);
      buildMI(MBB, MBBI, I8085::MOV)
          .addReg(I8085::L, RegState::Define)
          .addReg(I8085::C);
    }

    buildMI(MBB, MBBI, I8085::POP).addReg(I8085::BC, RegState::Define);
    MI.eraseFromParent();
    return true;
  }

  const bool PreserveHL = (baseReg != I8085::HL) &&
                          isHLOrSubRegLive(MBB, MBBI);
  const bool PreserveHLFromSP = PreserveHL && (baseReg == I8085::SP);

  if (PreserveHL) {
    buildMI(MBB, MBBI, I8085::PUSH).addReg(I8085::HL);
    if (PreserveHLFromSP)
      offsetToStore += 2;
  }
  
  if (destReg == I8085::SP) {
    buildMI(MBB, MBBI, I8085::LXI)
        .addReg(I8085::HL, RegState::Define)
        .addImm(0);
    buildMI(MBB, MBBI, I8085::DAD).addReg(I8085::SP);
    lowReg = I8085::L;
    highReg = I8085::H;
  } else if (!getPairRegs(destReg, lowReg, highReg)) {
    return false;
  }

  if (baseReg == I8085::HL) {
    addOffsetToHL(offsetToStore);
    buildMI(MBB, MBBI, I8085::MOV_M).addReg(lowReg);
    buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
    buildMI(MBB, MBBI, I8085::MOV_M).addReg(highReg);
    if (PreserveHL)
      buildMI(MBB, MBBI, I8085::POP).addReg(I8085::HL, RegState::Define);
    MI.eraseFromParent();
    return true;
  }

  buildMI(MBB, MBBI,  I8085::STORE_8)
        .addReg(baseReg)
        .addImm(offsetToStore+1)
        .addReg(highReg);

  buildMI(MBB, MBBI,  I8085::STORE_8)
        .addReg(baseReg)
        .addImm(offsetToStore)
        .addReg(lowReg);    

  if (PreserveHL)
    buildMI(MBB, MBBI, I8085::POP).addReg(I8085::HL, RegState::Define);

  MI.eraseFromParent();

  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::SHRINK_STACK_BY>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  int64_t Amount = MI.getOperand(0).getImm();

  buildMI(MBB, MBBI,  I8085::LXI)
        .addReg(I8085::HL,RegState::Define)
        .addImm(Amount);

  buildMI(MBB, MBBI,  I8085::DAD)
        .addReg(I8085::SP);

  buildMI(MBB, MBBI,  I8085::SPHL);
  
  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::GROW_STACK_BY>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  int64_t AmountImm = MI.getOperand(0).getImm();
  uint16_t Amount = static_cast<uint16_t>(-AmountImm);
  
  buildMI(MBB, MBBI,  I8085::LXI)
        .addReg(I8085::HL,RegState::Define)
        .addImm(Amount);

  buildMI(MBB, MBBI,  I8085::DAD)
        .addReg(I8085::SP);

  buildMI(MBB, MBBI,  I8085::SPHL);
  
  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::LOAD_8_WITH_ADDR>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
 

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned baseReg = MI.getOperand(1).getReg();
  int64_t offsetToLoad = MI.getOperand(2).getImm();

  const bool PreserveHL =
      (baseReg != I8085::HL && destReg != I8085::H &&
       destReg != I8085::L && isHLOrSubRegLive(MBB, MBBI));
  const bool PreserveHLFromSP = PreserveHL && (baseReg == I8085::SP);
  const bool PreserveHLFromHL =
      (baseReg == I8085::HL && destReg != I8085::H && destReg != I8085::L);

  if (PreserveHL) {
    buildMI(MBB, MBBI, I8085::PUSH).addReg(I8085::HL);
    if (PreserveHLFromSP)
      offsetToLoad += 2;
  }

  auto addOffsetToHL = [&](int64_t Offset) {
    if (Offset > 0) {
      for (int64_t i = 0; i < Offset; ++i)
        buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
    } else if (Offset < 0) {
      for (int64_t i = 0; i < -Offset; ++i)
        buildMI(MBB, MBBI, I8085::DCX).addReg(I8085::HL, RegState::Define);
    }
  };

  if (baseReg == I8085::HL) {
    addOffsetToHL(offsetToLoad);
  } else {
    buildMI(MBB, MBBI, I8085::LXI)
        .addReg(I8085::HL,RegState::Define)
        .addImm(offsetToLoad);
    buildMI(MBB, MBBI, I8085::DAD)
        .addReg(baseReg);
  }
  
  /* Store the register value pointed by HL reg */
  
  buildMI(MBB, MBBI, I8085::MOV_FROM_M)
      .addReg(destReg,RegState::Define);
  
  if (baseReg == I8085::HL && PreserveHLFromHL)
    addOffsetToHL(-offsetToLoad);

  if (PreserveHL)
    buildMI(MBB, MBBI, I8085::POP).addReg(I8085::HL, RegState::Define);

  MI.eraseFromParent();
  return true;
}


template <>
bool I8085ExpandPseudo::expand<I8085::LOAD_16_WITH_ADDR>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned lowReg,highReg;
  unsigned destReg = MI.getOperand(0).getReg();
  unsigned baseReg = MI.getOperand(1).getReg();
  int64_t offsetToLoad = MI.getOperand(2).getImm();

  const bool PreserveHL =
      (baseReg != I8085::HL && destReg != I8085::HL &&
       destReg != I8085::SP && isHLOrSubRegLive(MBB, MBBI));
  const bool PreserveHLFromSP = PreserveHL && (baseReg == I8085::SP);
  const bool PreservePSW =
      (destReg == I8085::HL || destReg == I8085::SP) &&
      shouldPreservePSW(MBB, MBBI);

  if (PreserveHL) {
    buildMI(MBB, MBBI, I8085::PUSH).addReg(I8085::HL);
    if (PreserveHLFromSP)
      offsetToLoad += 2;
  }
  if (PreservePSW) {
    const bool CanClobberFlags =
        !isPhysRegLive(MBB, MBBI, I8085::SREG);
    pushPSW(MBB, MBBI, CanClobberFlags);
    if (baseReg == I8085::SP)
      offsetToLoad += 2;
  }
  
  auto addOffsetToHL = [&](int64_t Offset) {
    if (Offset > 0) {
      for (int64_t i = 0; i < Offset; ++i)
        buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
    } else if (Offset < 0) {
      for (int64_t i = 0; i < -Offset; ++i)
        buildMI(MBB, MBBI, I8085::DCX).addReg(I8085::HL, RegState::Define);
    }
  };

  if (destReg == I8085::HL || destReg == I8085::SP) {
    if (baseReg == I8085::HL) {
      addOffsetToHL(offsetToLoad);
    } else {
      buildMI(MBB, MBBI, I8085::LXI)
          .addReg(I8085::HL,RegState::Define)
          .addImm(offsetToLoad);
      buildMI(MBB, MBBI, I8085::DAD)
          .addReg(baseReg);
    }

    buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::A, RegState::Define);
    buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
    buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(I8085::H, RegState::Define);
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::L, RegState::Define)
        .addReg(I8085::A);

    if (PreservePSW)
      buildMI(MBB, MBBI, I8085::POP).addReg(I8085::PSW, RegState::Define);

    if (destReg == I8085::SP)
      buildMI(MBB, MBBI, I8085::SPHL);
    MI.eraseFromParent();
    return true;
  }

  if (!getPairRegs(destReg, lowReg, highReg))
    return false;

  if (PreserveHL) {
    buildMI(MBB, MBBI, I8085::LXI)
        .addReg(I8085::HL, RegState::Define)
        .addImm(offsetToLoad);
    buildMI(MBB, MBBI, I8085::DAD)
        .addReg(baseReg);
    buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(lowReg, RegState::Define);
    buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
    buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(highReg, RegState::Define);
    if (PreservePSW)
      buildMI(MBB, MBBI, I8085::POP).addReg(I8085::PSW, RegState::Define);
    buildMI(MBB, MBBI, I8085::POP).addReg(I8085::HL, RegState::Define);
    MI.eraseFromParent();
    return true;
  }

  if (baseReg == I8085::HL) {
    addOffsetToHL(offsetToLoad);
    buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(lowReg, RegState::Define);
    buildMI(MBB, MBBI, I8085::INX).addReg(I8085::HL, RegState::Define);
    buildMI(MBB, MBBI, I8085::MOV_FROM_M).addReg(highReg, RegState::Define);
    MI.eraseFromParent();
    return true;
  }

  buildMI(MBB, MBBI, I8085::LOAD_8_WITH_ADDR)
      .addReg(highReg)
      .addReg(baseReg)
      .addImm(offsetToLoad+1);

  buildMI(MBB, MBBI, I8085::LOAD_8_WITH_ADDR)
      .addReg(lowReg)
      .addReg(baseReg)
      .addImm(offsetToLoad);    
  
  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::FRMIDX>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned baseReg = MI.getOperand(1).getReg();
  int16_t offset = MI.getOperand(2).getImm();

  // HL = base + offset
  buildMI(MBB, MBBI, I8085::LXI)
      .addReg(I8085::HL, RegState::Define)
      .addImm(offset);
  buildMI(MBB, MBBI, I8085::DAD)
      .addReg(baseReg);

  if (destReg == I8085::HL) {
    MI.eraseFromParent();
    return true;
  }

  if (destReg == I8085::SP) {
    buildMI(MBB, MBBI, I8085::SPHL);
    MI.eraseFromParent();
    return true;
  }

  unsigned lowReg = 0, highReg = 0;
  if (!getPairRegs(destReg, lowReg, highReg))
    return false;

  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(highReg, RegState::Define)
      .addReg(I8085::H);
  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(lowReg, RegState::Define)
      .addReg(I8085::L);

  MI.eraseFromParent();
  return true;
}


template <>
bool I8085ExpandPseudo::expand<I8085::ANDI_8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned destReg = operandOne;
  int64_t immValue = MI.getOperand(2).getImm();    
  
  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(I8085::A,RegState::Define)
      .addReg(operandOne);

  buildMI(MBB, MBBI, I8085::ANI)
      .addImm(immValue);

  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(destReg,RegState::Define)
      .addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::ORI_8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned destReg = operandOne;
  int64_t immValue = MI.getOperand(2).getImm();    
  
  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(I8085::A,RegState::Define)
      .addReg(operandOne);

  buildMI(MBB, MBBI, I8085::ORI)
      .addImm(immValue);

  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(destReg,RegState::Define)
      .addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::XORI_8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned destReg = operandOne;
  int64_t immValue = MI.getOperand(2).getImm();    
  
  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(I8085::A,RegState::Define)
      .addReg(operandOne);

  buildMI(MBB, MBBI, I8085::XRI)
      .addImm(immValue);

  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(destReg,RegState::Define)
      .addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}



template <>
bool I8085ExpandPseudo::expand<I8085::AND_8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned operandOne = MI.getOperand(1).getReg();
  uint16_t operandTwo = MI.getOperand(2).getReg();    

  if (destReg != operandOne) {
    buildMI(MBB, MBBI, TargetOpcode::COPY, destReg)
        .addReg(operandOne);
  }
  
  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(I8085::A,RegState::Define)
      .addReg(operandOne);

  buildMI(MBB, MBBI, I8085::ANA)
      .addReg(operandTwo);

  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(destReg,RegState::Define)
      .addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::OR_8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned destReg = operandOne;
  uint16_t operandTwo = MI.getOperand(2).getReg();    
  
  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(I8085::A,RegState::Define)
      .addReg(operandOne);

  buildMI(MBB, MBBI, I8085::ORA)
      .addReg(operandTwo);

  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(destReg,RegState::Define)
      .addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::XOR_8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned destReg = operandOne;
  uint16_t operandTwo = MI.getOperand(2).getReg();    
  
  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(I8085::A,RegState::Define)
      .addReg(operandOne);

  buildMI(MBB, MBBI, I8085::XRA)
      .addReg(operandTwo);

  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(destReg,RegState::Define)
      .addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::AND_16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned operandOne = MI.getOperand(1).getReg();
  uint16_t operandTwo = MI.getOperand(2).getReg();  
  bool DstIsDead = MI.getOperand(0).isDead();
  
  unsigned opLow,opHigh;
  unsigned destLow,destHigh;

  if (!getPairRegs(destReg, destLow, destHigh))
    return false;
  if (destReg != operandOne) {
    buildMI(MBB, MBBI, TargetOpcode::COPY, destReg)
        .addReg(operandOne);
  }
  if (!getPairRegs(operandTwo, opLow, opHigh))
    return false;
  
  buildMI(MBB, MBBI, I8085::AND_8)
      .addReg(destLow, RegState::Define | getDeadRegState(DstIsDead))
      .addReg(destLow)
      .addReg(opLow);

  buildMI(MBB, MBBI, I8085::AND_8)
      .addReg(destHigh, RegState::Define | getDeadRegState(DstIsDead))
      .addReg(destHigh)
      .addReg(opHigh);    

  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::OR_16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned destReg = operandOne;
  uint16_t operandTwo = MI.getOperand(2).getReg();  
  bool DstIsDead = MI.getOperand(0).isDead();
  
  unsigned opLow,opHigh;
  unsigned destLow,destHigh;

  if (!getPairRegs(destReg, destLow, destHigh))
    return false;
  if (!getPairRegs(operandTwo, opLow, opHigh))
    return false;
  
  buildMI(MBB, MBBI, I8085::OR_8)
      .addReg(destLow, RegState::Define | getDeadRegState(DstIsDead))
      .addReg(destLow)
      .addReg(opLow);

  buildMI(MBB, MBBI, I8085::OR_8)
      .addReg(destHigh, RegState::Define | getDeadRegState(DstIsDead))
      .addReg(destHigh)
      .addReg(opHigh);    
      
  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::XOR_16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned destReg = operandOne;
  uint16_t operandTwo = MI.getOperand(2).getReg();  
  bool DstIsDead = MI.getOperand(0).isDead();
  
  unsigned opLow,opHigh;
  unsigned destLow,destHigh;

  if (!getPairRegs(destReg, destLow, destHigh))
    return false;
  if (!getPairRegs(operandTwo, opLow, opHigh))
    return false;
  
  buildMI(MBB, MBBI, I8085::XOR_8)
      .addReg(destLow, RegState::Define | getDeadRegState(DstIsDead))
      .addReg(destLow)
      .addReg(opLow);

  buildMI(MBB, MBBI, I8085::XOR_8)
      .addReg(destHigh, RegState::Define | getDeadRegState(DstIsDead))
      .addReg(destHigh)
      .addReg(opHigh);    
      
  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::ADD_8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned operandOne = MI.getOperand(1).getReg();
  MachineOperand &Op2 = MI.getOperand(2);

  bool Op2IsImm = Op2.isImm();
  bool Op2IsA = !Op2IsImm && Op2.getReg() == I8085::A;

  // Only copy into the destination when we won't clobber an A-held operand.
  if (destReg != operandOne && !(Op2IsA && destReg == I8085::A)) {
    buildMI(MBB, MBBI, TargetOpcode::COPY, destReg)
        .addReg(operandOne);
  }

  if (Op2IsImm) {
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::A, RegState::Define)
        .addReg(destReg);
    buildMI(MBB, MBBI, I8085::ADI)
        .addImm(static_cast<uint8_t>(Op2.getImm()));
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(destReg, RegState::Define)
        .addReg(I8085::A);
  } else if (Op2IsA) {
    // A already holds the RHS; add the LHS directly.
    if (destReg == I8085::A) {
      buildMI(MBB, MBBI, I8085::ADD)
          .addReg(operandOne);
    } else {
      buildMI(MBB, MBBI, I8085::ADD)
          .addReg(destReg);
      buildMI(MBB, MBBI, I8085::MOV)
          .addReg(destReg, RegState::Define)
          .addReg(I8085::A);
    }
  } else {
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::A, RegState::Define)
        .addReg(destReg);
    buildMI(MBB, MBBI, I8085::ADD)
        .addReg(Op2.getReg());
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(destReg, RegState::Define)
        .addReg(I8085::A);
  }

  MI.eraseFromParent();
  return true;
}


template <>
bool I8085ExpandPseudo::expand<I8085::SUB_8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned operandOne = MI.getOperand(1).getReg();
  MachineOperand &Op2 = MI.getOperand(2);

  if (destReg != operandOne) {
    buildMI(MBB, MBBI, TargetOpcode::COPY, destReg)
        .addReg(operandOne);
  }
  
  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(I8085::A, RegState::Define)
      .addReg(destReg);

  if (Op2.isImm()) {
    buildMI(MBB, MBBI, I8085::SUI)
        .addImm(static_cast<uint8_t>(Op2.getImm()));
  } else {
    buildMI(MBB, MBBI, I8085::SUB)
        .addReg(Op2.getReg());
  }

  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(destReg, RegState::Define)
      .addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::SUBI_8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned operandOne = MI.getOperand(1).getReg();
  int64_t Imm = MI.getOperand(2).getImm();

  if (destReg != operandOne) {
    buildMI(MBB, MBBI, TargetOpcode::COPY, destReg)
        .addReg(operandOne);
  }

  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(I8085::A, RegState::Define)
      .addReg(destReg);
  buildMI(MBB, MBBI, I8085::SUI)
      .addImm(static_cast<uint8_t>(Imm));
  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(destReg, RegState::Define)
      .addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}

template <>
bool I8085ExpandPseudo::expand<I8085::ADD_16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned operandTwo = MI.getOperand(2).getReg();
  bool DstIsDead = MI.getOperand(0).isDead();
  unsigned workReg = destReg;
  bool copyBack = false;

  if (destReg == operandTwo && destReg != operandOne) {
    std::swap(operandOne, operandTwo);
  }

  unsigned opLow,opHigh;
  unsigned destLow,destHigh;

  if (!getPairRegs(workReg, destLow, destHigh))
    return false;
  if (workReg != operandOne) {
    buildMI(MBB, MBBI, TargetOpcode::COPY, workReg)
        .addReg(operandOne);
  }
  if (!getPairRegs(operandTwo, opLow, opHigh))
    return false;

  bool WorkIsDead = DstIsDead && !copyBack;
  buildMI(MBB, MBBI, I8085::ADD_8)
      .addReg(destLow, RegState::Define | getDeadRegState(WorkIsDead))
      .addReg(destLow)
      .addReg(opLow);
  
  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(I8085::A, RegState::Define)
      .addReg(destHigh);

  buildMI(MBB, MBBI, I8085::ADC)
      .addReg(opHigh);

  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(destHigh, RegState::Define | getDeadRegState(WorkIsDead))
      .addReg(I8085::A);

  if (copyBack) {
    buildMI(MBB, MBBI, TargetOpcode::COPY, destReg)
        .addReg(workReg);
  }

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::SUB_16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned destReg = MI.getOperand(0).getReg();
  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned operandTwo = MI.getOperand(2).getReg();  
  bool DstIsDead=MI.getOperand(0).isDead();
  unsigned workReg = destReg;
  bool copyBack = false;
  unsigned opLow,opHigh;
  unsigned destLow,destHigh;

  if (destReg == operandTwo && destReg != operandOne) {
    if (!getPairRegs(destReg, destLow, destHigh))
      return false;
    if (!getPairRegs(operandOne, opLow, opHigh))
      return false;

    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::A, RegState::Define)
        .addReg(opLow);
    buildMI(MBB, MBBI, I8085::SUB)
        .addReg(destLow);
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(destLow, RegState::Define | getDeadRegState(DstIsDead))
        .addReg(I8085::A);

    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(I8085::A, RegState::Define)
        .addReg(opHigh);
    buildMI(MBB, MBBI, I8085::SBB)
        .addReg(destHigh);
    buildMI(MBB, MBBI, I8085::MOV)
        .addReg(destHigh, RegState::Define | getDeadRegState(DstIsDead))
        .addReg(I8085::A);

    MI.eraseFromParent();
    return true;
  }

  if (!getPairRegs(workReg, destLow, destHigh))
    return false;
  if (workReg != operandOne) {
    buildMI(MBB, MBBI, TargetOpcode::COPY, workReg)
        .addReg(operandOne);
  }
  if (!getPairRegs(operandTwo, opLow, opHigh))
    return false;
  bool WorkIsDead = DstIsDead && !copyBack;
  
  buildMI(MBB, MBBI, I8085::SUB_8)
      .addReg(destLow, RegState::Define | getDeadRegState(WorkIsDead))
      .addReg(destLow)
      .addReg(opLow);
  
  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(I8085::A, RegState::Define)
      .addReg(destHigh);

  buildMI(MBB, MBBI, I8085::SBB)
      .addReg(opHigh);

  buildMI(MBB, MBBI, I8085::MOV)
      .addReg(destHigh, RegState::Define | getDeadRegState(WorkIsDead))
      .addReg(I8085::A);

  if (copyBack) {
    buildMI(MBB, MBBI, TargetOpcode::COPY, destReg)
        .addReg(workReg);
  }

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::SET_EQ_8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned operandOne = MI.getOperand(0).getReg();
  unsigned destReg = operandOne;
  uint16_t operandTwo = MI.getOperand(2).getReg();  

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(destReg);

  buildMI(MBB, MBBI, I8085::SUB)
    .addReg(operandTwo);

  buildMI(MBB, MBBI, I8085::CMA);  

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(destReg,RegState::Define)
    .addReg(I8085::A);
  
  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::SET_NE_8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned operandOne = MI.getOperand(0).getReg();
  unsigned destReg = operandOne;
  uint16_t operandTwo = MI.getOperand(2).getReg();  

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(destReg);

  buildMI(MBB, MBBI, I8085::SUB)
    .addReg(operandTwo);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(destReg,RegState::Define)
    .addReg(I8085::A);
  
  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::SET_EQ_16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned destReg = operandOne;
  uint16_t operandTwo = MI.getOperand(2).getReg();  
  bool DstIsDead=MI.getOperand(0).isDead();

  buildMI(MBB, MBBI, I8085::SUB_16)
    .addReg(destReg, RegState::Define | getDeadRegState(DstIsDead))
    .addReg(operandOne)
    .addReg(operandTwo);

  buildMI(MBB, MBBI, I8085::CMA);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(destReg,RegState::Define)
    .addReg(I8085::A);
  
  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::SET_NE_16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned operandOne = MI.getOperand(1).getReg();
  unsigned destReg = operandOne;
  uint16_t operandTwo = MI.getOperand(2).getReg();  
  bool DstIsDead=MI.getOperand(0).isDead();

  buildMI(MBB, MBBI, I8085::SUB_16)
    .addReg(destReg, RegState::Define | getDeadRegState(DstIsDead))
    .addReg(operandOne)
    .addReg(operandTwo);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(destReg,RegState::Define)
    .addReg(I8085::A);
  
  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::JMP_8_IF>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned operand = MI.getOperand(0).getReg();


  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(operand);

  buildMI(MBB, MBBI, I8085::ORI)
    .addImm(0);

  buildMI(MBB, MBBI, I8085::JNZ).add(MI.getOperand(1));
  
  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::TRUNC16TO8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned destReg = MI.getOperand(0).getReg();
  unsigned operand = MI.getOperand(1).getReg();

  unsigned operandLow,operandHigh;
  if (!getPairRegs(operand, operandLow, operandHigh))
    return false;
  
  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(destReg,RegState::Define)
    .addReg(operandLow);
     
  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::SEXT8TO16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned destReg = MI.getOperand(0).getReg();
  unsigned operand = MI.getOperand(1).getReg();

  unsigned destLow,destHigh;
  if (!getPairRegs(destReg, destLow, destHigh))
    return false;
  
  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(operand);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(destLow,RegState::Define)
    .addReg(I8085::A);

  buildMI(MBB, MBBI, I8085::ADI)
    .addImm(128); // 80h in hex .. [Adding 80h will set carry flag if HSB is high]


  buildMI(MBB, MBBI, I8085::SBB)
    .addReg(I8085::A);  // will result in FFh if CF set, 0 else

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(destHigh,RegState::Define)
    .addReg(I8085::A);  
     
  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::ZEXT8TO16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned destReg = MI.getOperand(0).getReg();
  unsigned operand = MI.getOperand(1).getReg();

  unsigned destLow,destHigh;
  if (!getPairRegs(destReg, destLow, destHigh))
    return false;
  
  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(operand);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(destLow,RegState::Define)
    .addReg(I8085::A);
  
  buildMI(MBB, MBBI, I8085::MVI)
    .addReg(destHigh,RegState::Define)
    .addImm(0);
     
  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::AEXT8TO16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  return expand<I8085::ZEXT8TO16>(MBB, MI);
}

template <> bool I8085ExpandPseudo::expand<I8085::BSWAP16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();

  unsigned srcLow, srcHigh;
  if (!getPairRegs(srcReg, srcLow, srcHigh))
    return false;

  unsigned destLow, destHigh;
  if (!getPairRegs(destReg, destLow, destHigh))
    return false;

  // BSWAP16: swap the two bytes
  // Use A as temporary to swap: A = srcLow, destLow = srcHigh, destHigh = A
  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A, RegState::Define)
    .addReg(srcLow);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(destLow, RegState::Define)
    .addReg(srcHigh);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(destHigh, RegState::Define)
    .addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::RL_16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();

  unsigned regLow, regHigh;
  if (!getPairRegs(destReg, regLow, regHigh))
    return false;
  if (destReg != srcReg) {
    unsigned srcLow, srcHigh;
    if (!getPairRegs(srcReg, srcLow, srcHigh))
      return false;
    buildMI(MBB, MBBI, I8085::MOV)
      .addReg(regLow, RegState::Define)
      .addReg(srcLow);
    buildMI(MBB, MBBI, I8085::MOV)
      .addReg(regHigh, RegState::Define)
      .addReg(srcHigh);
  }

  // Clear carry before rotating through carry so logical shift inserts 0s.
  buildMI(MBB, MBBI, I8085::STC);
  buildMI(MBB, MBBI, I8085::CMC);
  
  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(regLow);

  buildMI(MBB, MBBI, I8085::RAL);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(regLow,RegState::Define)
    .addReg(I8085::A);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(regHigh);

  buildMI(MBB, MBBI, I8085::RAL);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(regHigh,RegState::Define)
    .addReg(I8085::A);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(regLow);

  buildMI(MBB, MBBI, I8085::ANI)
    .addImm(254);  

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(regLow,RegState::Define)
    .addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}


template <> bool I8085ExpandPseudo::expand<I8085::RR_16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();

  unsigned regLow, regHigh;
  if (!getPairRegs(destReg, regLow, regHigh))
    return false;
  if (destReg != srcReg) {
    unsigned srcLow, srcHigh;
    if (!getPairRegs(srcReg, srcLow, srcHigh))
      return false;
    buildMI(MBB, MBBI, I8085::MOV)
      .addReg(regLow, RegState::Define)
      .addReg(srcLow);
    buildMI(MBB, MBBI, I8085::MOV)
      .addReg(regHigh, RegState::Define)
      .addReg(srcHigh);
  }

  // Clear carry before rotating through carry so logical shift inserts 0s.
  buildMI(MBB, MBBI, I8085::STC);
  buildMI(MBB, MBBI, I8085::CMC);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(regHigh);

  buildMI(MBB, MBBI, I8085::RAR);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(regHigh,RegState::Define)
    .addReg(I8085::A);


  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(regLow);

  buildMI(MBB, MBBI, I8085::RAR);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(regLow,RegState::Define)
    .addReg(I8085::A);


  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(regHigh);

  buildMI(MBB, MBBI, I8085::ANI)
    .addImm(127);  

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(regHigh,RegState::Define)
    .addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::ASR_16>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();

  unsigned regLow, regHigh;
  if (!getPairRegs(destReg, regLow, regHigh))
    return false;
  if (destReg != srcReg) {
    unsigned srcLow, srcHigh;
    if (!getPairRegs(srcReg, srcLow, srcHigh))
      return false;
    buildMI(MBB, MBBI, I8085::MOV)
      .addReg(regLow, RegState::Define)
      .addReg(srcLow);
    buildMI(MBB, MBBI, I8085::MOV)
      .addReg(regHigh, RegState::Define)
      .addReg(srcHigh);
  }

  // Set carry from sign bit of the high byte.
  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A, RegState::Define)
    .addReg(regHigh);

  buildMI(MBB, MBBI, I8085::RLC);

  // Restore high byte and shift through carry to preserve sign.
  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A, RegState::Define)
    .addReg(regHigh);

  buildMI(MBB, MBBI, I8085::RAR);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(regHigh, RegState::Define)
    .addReg(I8085::A);

  // Shift low byte through carry.
  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A, RegState::Define)
    .addReg(regLow);

  buildMI(MBB, MBBI, I8085::RAR);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(regLow, RegState::Define)
    .addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::RL_8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();
  if (destReg != srcReg) {
    buildMI(MBB, MBBI, I8085::MOV)
      .addReg(destReg, RegState::Define)
      .addReg(srcReg);
  }

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(destReg);

  buildMI(MBB, MBBI, I8085::ADD)
    .addReg(I8085::A);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(destReg,RegState::Define)
    .addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}


template <> bool I8085ExpandPseudo::expand<I8085::RR_8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();
  if (destReg != srcReg) {
    buildMI(MBB, MBBI, I8085::MOV)
      .addReg(destReg, RegState::Define)
      .addReg(srcReg);
  }

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(destReg);

  buildMI(MBB, MBBI, I8085::RRC);

  buildMI(MBB, MBBI, I8085::ANI)
    .addImm(127);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(destReg,RegState::Define)
    .addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::ASR_8>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned destReg = MI.getOperand(0).getReg();
  unsigned srcReg = MI.getOperand(1).getReg();
  if (destReg != srcReg) {
    buildMI(MBB, MBBI, I8085::MOV)
      .addReg(destReg, RegState::Define)
      .addReg(srcReg);
  }

  // Set carry from sign bit.
  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A, RegState::Define)
    .addReg(destReg);

  buildMI(MBB, MBBI, I8085::RLC);

  // Restore and shift through carry.
  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A, RegState::Define)
    .addReg(destReg);

  buildMI(MBB, MBBI, I8085::RAR);

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(destReg, RegState::Define)
    .addReg(I8085::A);

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::JMP_16_IF>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned operandOne = MI.getOperand(0).getReg();

  unsigned opOneLow,opOneHigh;
  if (!getPairRegs(operandOne, opOneLow, opOneHigh))
    return false;

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(opOneHigh);

  buildMI(MBB, MBBI, I8085::ANI)
    .addImm(255);

  buildMI(MBB, MBBI, I8085::JNZ)
    .addMBB(MI.getOperand(1).getMBB());

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(opOneLow);

  buildMI(MBB, MBBI, I8085::ANI)
    .addImm(255);

  buildMI(MBB, MBBI, I8085::JNZ)
    .addMBB(MI.getOperand(1).getMBB());
  
  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::JMP_16_IF_NOT_EQUAL>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  MachineFunction *MF = MBB.getParent();
  const DebugLoc &DL = MI.getDebugLoc();
  
  unsigned operandOne = MI.getOperand(0).getReg();
  unsigned operandTwo = MI.getOperand(1).getReg();
  MachineBasicBlock *TargetMBB = MI.getOperand(2).getMBB();

  unsigned opOneLow,opOneHigh;
  unsigned opTwoLow,opTwoHigh;
  if (!getPairRegs(operandOne, opOneLow, opOneHigh))
    return false;
  if (!getPairRegs(operandTwo, opTwoLow, opTwoHigh))
    return false;

  const BasicBlock *LLVMBB = MBB.getBasicBlock();
  MachineBasicBlock *CmpLowMBB = MF->CreateMachineBasicBlock(LLVMBB);
  MachineBasicBlock *TailMBB = MF->CreateMachineBasicBlock(LLVMBB);
  MachineBasicBlock *DiffMBB = MF->CreateMachineBasicBlock(LLVMBB);
  auto InsertPos = std::next(MBB.getIterator());
  MF->insert(InsertPos, CmpLowMBB);
  MF->insert(InsertPos, TailMBB);
  MF->insert(InsertPos, DiffMBB);
  // Renumber ALL blocks to avoid conflicts with existing block numbers.
  MF->RenumberBlocks();

  TailMBB->splice(TailMBB->begin(), &MBB, std::next(MBBI), MBB.end());
  TailMBB->transferSuccessorsAndUpdatePHIs(&MBB);
  if (TailMBB->isSuccessor(TargetMBB)) {
    TailMBB->removeSuccessor(TargetMBB);
    TargetMBB->replacePhiUsesWith(TailMBB, DiffMBB);
  }

  MBB.addSuccessor(CmpLowMBB);
  MBB.addSuccessor(DiffMBB);
  CmpLowMBB->addSuccessor(TailMBB);
  CmpLowMBB->addSuccessor(DiffMBB);
  DiffMBB->addSuccessor(TargetMBB);

  BuildMI(MBB, MBBI, DL, TII->get(I8085::MOV))
      .addReg(I8085::A, RegState::Define)
      .addReg(opOneHigh);
  BuildMI(MBB, MBBI, DL, TII->get(I8085::CMP)).addReg(opTwoHigh);
  BuildMI(MBB, MBBI, DL, TII->get(I8085::JNZ)).addMBB(DiffMBB);
  BuildMI(MBB, MBBI, DL, TII->get(I8085::JMP)).addMBB(CmpLowMBB);

  BuildMI(CmpLowMBB, DL, TII->get(I8085::MOV))
      .addReg(I8085::A, RegState::Define)
      .addReg(opOneLow);
  BuildMI(CmpLowMBB, DL, TII->get(I8085::CMP)).addReg(opTwoLow);
  BuildMI(CmpLowMBB, DL, TII->get(I8085::JNZ)).addMBB(DiffMBB);
  BuildMI(CmpLowMBB, DL, TII->get(I8085::JMP)).addMBB(TailMBB);

  BuildMI(DiffMBB, DL, TII->get(I8085::JMP)).addMBB(TargetMBB);

  if (TailMBB->succ_size() == 1) {
    auto Last = TailMBB->getLastNonDebugInstr();
    if (Last == TailMBB->end() || !Last->isTerminator())
      BuildMI(TailMBB, DL, TII->get(I8085::JMP))
          .addMBB(*TailMBB->succ_begin());
  }

  LivePhysRegs LiveRegs;
  computeAndAddLiveIns(LiveRegs, *TailMBB);
  computeAndAddLiveIns(LiveRegs, *DiffMBB);
  computeAndAddLiveIns(LiveRegs, *CmpLowMBB);

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::JMP_16_IF_ZERO>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned operand = MI.getOperand(0).getReg();

  unsigned opLow, opHigh;
  if (!getPairRegs(operand, opLow, opHigh))
    return false;

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A, RegState::Define)
    .addReg(opHigh);

  buildMI(MBB, MBBI, I8085::ORA)
    .addReg(opLow);

  buildMI(MBB, MBBI, I8085::JZ)
    .addMBB(MI.getOperand(1).getMBB());

  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::JMP_16_IF_NOT_ZERO>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned operand = MI.getOperand(0).getReg();

  unsigned opLow, opHigh;
  if (!getPairRegs(operand, opLow, opHigh))
    return false;

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A, RegState::Define)
    .addReg(opHigh);

  buildMI(MBB, MBBI, I8085::ORA)
    .addReg(opLow);

  buildMI(MBB, MBBI, I8085::JNZ)
    .addMBB(MI.getOperand(1).getMBB());

  MI.eraseFromParent();
  return true;
}



template <> bool I8085ExpandPseudo::expand<I8085::JMP_16_IF_SAME_SIGN>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned operandOne = MI.getOperand(0).getReg();
  unsigned operandTwo = MI.getOperand(1).getReg();

  unsigned opOneLow,opOneHigh;
  unsigned opTwoLow,opTwoHigh;
  if (!getPairRegs(operandOne, opOneLow, opOneHigh))
    return false;
  if (!getPairRegs(operandTwo, opTwoLow, opTwoHigh))
    return false;

  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(opOneHigh);

  buildMI(MBB, MBBI, I8085::XRA)
    .addReg(opTwoHigh);

  buildMI(MBB, MBBI, I8085::ANI)
        .addImm(128);  

  buildMI(MBB, MBBI, I8085::JZ)
    .addMBB(MI.getOperand(2).getMBB());
  
  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::JMP_16_IF_POSITIVE>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  
  unsigned operandOne = MI.getOperand(0).getReg();

  unsigned opOneLow,opOneHigh;
  if (!getPairRegs(operandOne, opOneLow, opOneHigh))
    return false;


  buildMI(MBB, MBBI, I8085::MOV)
    .addReg(I8085::A,RegState::Define)
    .addReg(opOneHigh);

  buildMI(MBB, MBBI, I8085::ANI)
        .addImm(128);  

  buildMI(MBB, MBBI, I8085::JZ)
    .addMBB(MI.getOperand(1).getMBB());
  
  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::STORE_8_AT_OFFSET_WITH_SP>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned srcReg = MI.getOperand(0).getReg();
  int64_t offsetToStore = MI.getOperand(1).getImm();
  
  buildMI(MBB, MBBI, I8085::STORE_8)
    .addReg(I8085::SP)
    .addImm(offsetToStore)
    .addReg(srcReg);
  
  MI.eraseFromParent();
  return true;
}

template <> bool I8085ExpandPseudo::expand<I8085::STORE_16_AT_OFFSET_WITH_SP>(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;

  unsigned srcReg = MI.getOperand(0).getReg();
  int64_t offsetToStore = MI.getOperand(1).getImm();
  
  buildMI(MBB, MBBI, I8085::STORE_16)
    .addReg(I8085::SP)
    .addImm(offsetToStore)
    .addReg(srcReg);
  
  MI.eraseFromParent();
  return true;
}

bool I8085ExpandPseudo::expandMI(Block &MBB, BlockIt MBBI) {
  MachineInstr &MI = *MBBI;
  int Opcode = MBBI->getOpcode();

  if (Opcode == I8085::MOV && MI.getNumOperands() >= 2 &&
      MI.getOperand(0).isReg() && MI.getOperand(1).isReg() &&
      MI.getOperand(0).getReg() == MI.getOperand(1).getReg()) {
    MI.eraseFromParent();
    return true;
  }

#define EXPAND(Op)                                                             \
  case Op:                                                                     \
    return expand<Op>(MBB, MI)

  switch (Opcode) {
    EXPAND(I8085::LOAD_16_ADDR_CONTENT);
    EXPAND(I8085::LOAD_8_ADDR_CONTENT);
    EXPAND(I8085::STORE_16_AT_OFFSET_WITH_SP);
    EXPAND(I8085::STORE_8_AT_OFFSET_WITH_SP);
    EXPAND(I8085::JMP_16_IF);
    EXPAND(I8085::JMP_16_IF_POSITIVE);
    EXPAND(I8085::JMP_16_IF_SAME_SIGN);
    EXPAND(I8085::JMP_16_IF_NOT_EQUAL);
    EXPAND(I8085::JMP_16_IF_ZERO);
    EXPAND(I8085::JMP_16_IF_NOT_ZERO);
    EXPAND(I8085::JMP_8_IF);
    EXPAND(I8085::TRUNC16TO8);
    EXPAND(I8085::AEXT8TO16);
    EXPAND(I8085::SEXT8TO16);
    EXPAND(I8085::ZEXT8TO16);
    EXPAND(I8085::BSWAP16);
    EXPAND(I8085::RL_16);
    EXPAND(I8085::RR_16);
    EXPAND(I8085::ASR_16);
    EXPAND(I8085::RL_8);
    EXPAND(I8085::RR_8);
    EXPAND(I8085::ASR_8);
    EXPAND(I8085::STORE_16_ADDR_CONTENT);
    EXPAND(I8085::STORE_8_ADDR_CONTENT);
    EXPAND(I8085::CALL_INDIRECT);
    // EXPAND(I8085::SET_NE_16);
    // EXPAND(I8085::SET_EQ_16);
    // EXPAND(I8085::SET_NE_8);
    // EXPAND(I8085::SET_EQ_8);
    EXPAND(I8085::XOR_16);
    EXPAND(I8085::OR_16);
    EXPAND(I8085::AND_16);
    EXPAND(I8085::XOR_8);
    EXPAND(I8085::OR_8);
    EXPAND(I8085::AND_8);
    EXPAND(I8085::XORI_8);
    EXPAND(I8085::ORI_8);
    EXPAND(I8085::ANDI_8);
    EXPAND(I8085::SUB_16);
    EXPAND(I8085::ADD_16);
    EXPAND(I8085::SUB_8);
    EXPAND(I8085::SUBI_8);
    EXPAND(I8085::ADD_8);
    EXPAND(I8085::LOAD_16_WITH_ADDR);
    EXPAND(I8085::LOAD_8_WITH_ADDR);
    EXPAND(I8085::LOAD_8_WITH_IMM_ADDR);
    EXPAND(I8085::LOAD_16_WITH_IMM_ADDR);
    EXPAND(I8085::LOAD_16);
    EXPAND(I8085::FRMIDX);
    EXPAND(I8085::STORE_8_WITH_IMM_ADDR);
    EXPAND(I8085::STORE_16_WITH_IMM_ADDR);
    EXPAND(I8085::STORE_16);
    EXPAND(I8085::SHRINK_STACK_BY);
    EXPAND(I8085::GROW_STACK_BY);
    EXPAND(I8085::STORE_8);
  }
#undef EXPAND
  return false;
}

} // end of anonymous namespace

INITIALIZE_PASS(I8085ExpandPseudo, "i8085-expand-pseudo", I8085_EXPAND_PSEUDO_NAME,
                false, false)
namespace llvm {

FunctionPass *createI8085ExpandPseudoPass() { return new I8085ExpandPseudo(); }

} // end of namespace llvm
