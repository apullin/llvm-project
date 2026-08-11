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
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineOutliner.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define DEBUG_TYPE "tms9900-instr-info"

#define GET_INSTRINFO_CTOR_DTOR
#include "TMS9900GenInstrInfo.inc"

static bool isCondBranchOpcode(unsigned Opc) {
  switch (Opc) {
  case TMS9900::JEQ:
  case TMS9900::JNE:
  case TMS9900::JGT:
  case TMS9900::JLT:
  case TMS9900::JH:
  case TMS9900::JHE:
  case TMS9900::JL:
  case TMS9900::JLE:
  case TMS9900::JOC:
  case TMS9900::JNC:
  case TMS9900::JNO:
  case TMS9900::JOP:
    return true;
  default:
    return false;
  }
}

static unsigned getOppositeCondBranchOpcode(unsigned Opc) {
  switch (Opc) {
  case TMS9900::JEQ: return TMS9900::JNE;
  case TMS9900::JNE: return TMS9900::JEQ;
  case TMS9900::JL:  return TMS9900::JHE; // unsigned <  -> unsigned >=
  case TMS9900::JHE: return TMS9900::JL;  // unsigned >= -> unsigned <
  case TMS9900::JH:  return TMS9900::JLE; // unsigned >  -> unsigned <=
  case TMS9900::JLE: return TMS9900::JH;  // unsigned <= -> unsigned >
  case TMS9900::JOC: return TMS9900::JNC;
  case TMS9900::JNC: return TMS9900::JOC;
  default:
    return 0;
  }
}

static unsigned getJumpOpcodeForCC(ISD::CondCode CC) {
  switch (CC) {
  default:
    llvm_unreachable("Unknown condition code");
  case ISD::SETEQ:
    return TMS9900::JEQ;
  case ISD::SETNE:
    return TMS9900::JNE;
  case ISD::SETGT:
    return TMS9900::JGT;
  case ISD::SETLT:
    return TMS9900::JLT;
  case ISD::SETGE:
    return TMS9900::JGT;
  case ISD::SETLE:
    return TMS9900::JLT;
  case ISD::SETUGT:
    return TMS9900::JH;
  case ISD::SETUGE:
    return TMS9900::JHE;
  case ISD::SETULT:
    return TMS9900::JL;
  case ISD::SETULE:
    return TMS9900::JLE;
  }
}

static unsigned getCmpBrSizeInBytes(unsigned Opc, ISD::CondCode CC) {
  assert((Opc == TMS9900::CMPBRrr || Opc == TMS9900::CMPBRri) &&
         "expected a fused compare-branch");

  // Register compares are one word, while CI includes a second immediate
  // word. Signed >= and <= each require JEQ plus JGT/JLT; every other
  // supported condition expands to one jump.
  unsigned CompareSize = Opc == TMS9900::CMPBRri ? 4 : 2;
  unsigned BranchSize = (CC == ISD::SETGE || CC == ISD::SETLE) ? 4 : 2;
  return CompareSize + BranchSize;
}

TMS9900InstrInfo::TMS9900InstrInfo(const TMS9900Subtarget &STI)
    : TMS9900GenInstrInfo(TMS9900::ADJCALLSTACKDOWN, TMS9900::ADJCALLSTACKUP),
      RI(STI) {}

enum TMS9900MachineOutlinerConstructionID {
  MachineOutlinerDefault,
  MachineOutlinerTailCall,
};

static bool isPureTMS9900OutlinerOpcode(unsigned Opcode) {
  switch (Opcode) {
  case TMS9900::MOVrr:
  case TMS9900::MOVBrr:
  case TMS9900::LI:
  case TMS9900::CLRr:
  case TMS9900::SETOr:
  case TMS9900::SWPBr:
  case TMS9900::Arr:
  case TMS9900::ABrr:
  case TMS9900::AI:
  case TMS9900::Srr:
  case TMS9900::SBrr:
  case TMS9900::INCr:
  case TMS9900::INCTr:
  case TMS9900::DECr:
  case TMS9900::DECTr:
  case TMS9900::NEGr:
  case TMS9900::ABSr:
  case TMS9900::INVr:
  case TMS9900::SOCrr:
  case TMS9900::SOCBrr:
  case TMS9900::ORI:
  case TMS9900::SZCrr:
  case TMS9900::SZCBrr:
  case TMS9900::ANDI:
  case TMS9900::XORrr:
  case TMS9900::COCrr:
  case TMS9900::CZCrr:
  case TMS9900::SLAri:
  case TMS9900::SRAri:
  case TMS9900::SRLri:
  case TMS9900::SRCri:
  case TMS9900::SLAr0:
  case TMS9900::SRAr0:
  case TMS9900::SRLr0:
  case TMS9900::Crr:
  case TMS9900::CBrr:
  case TMS9900::CI:
    return true;
  default:
    return false;
  }
}

static bool isTMS9900OutlinerMemoryOpcode(unsigned Opcode) {
  switch (Opcode) {
  case TMS9900::MOVxm:
  case TMS9900::MOVmx:
  case TMS9900::MOVBxm:
  case TMS9900::MOVBmx:
    return true;
  default:
    return false;
  }
}

bool TMS9900InstrInfo::isFunctionSafeToOutlineFrom(
    MachineFunction &MF, bool OutlineFromLinkOnceODRs) const {
  const Function &F = MF.getFunction();

  if (!OutlineFromLinkOnceODRs && F.hasLinkOnceODRLinkage())
    return false;
  if (F.hasSection())
    return false;
  if (F.hasFnAttribute(Attribute::Naked) || F.hasFnAttribute("interrupt"))
    return false;

  return true;
}

bool TMS9900InstrInfo::shouldOutlineFromFunctionByDefault(
    MachineFunction &MF) const {
  return MF.getFunction().hasMinSize();
}

std::optional<std::unique_ptr<outliner::OutlinedFunction>>
TMS9900InstrInfo::getOutliningCandidateInfo(
    const MachineModuleInfo &MMI,
    std::vector<outliner::Candidate> &RepeatedSequenceLocs,
    unsigned MinRepeats) const {
  if (RepeatedSequenceLocs.size() < MinRepeats)
    return std::nullopt;

  const bool IsTailCallOutline =
      RepeatedSequenceLocs[0].back().getOpcode() == TMS9900::RET_REAL;

  if (!IsTailCallOutline) {
    // The MachineOutliner runs after prologue/epilogue insertion. On TMS9900,
    // BL writes the return address to R11. Leaf functions still hold their own
    // incoming return address in R11 at this point, so inserting a BL would
    // corrupt the final B *R11 return. Functions that already had calls have
    // already saved/restored R11 in their prologue/epilogue, so BL-based
    // outlining is safe there.
    llvm::erase_if(RepeatedSequenceLocs, [](outliner::Candidate &C) {
      return !C.getMF()->getFrameInfo().hasCalls();
    });

    if (RepeatedSequenceLocs.size() < MinRepeats)
      return std::nullopt;
  }

  unsigned SequenceSize = 0;
  for (MachineInstr &MI : RepeatedSequenceLocs[0])
    SequenceSize += getInstSizeInBytes(MI);

  if (IsTailCallOutline) {
    // B @symbol is 4 bytes and reuses the current R11 return address, so
    // suffixes ending in B *R11 can be outlined safely even from leaf
    // functions.
    constexpr unsigned BranchOverhead = 4;
    for (outliner::Candidate &C : RepeatedSequenceLocs)
      C.setCallInfo(MachineOutlinerTailCall, BranchOverhead);

    return std::make_unique<outliner::OutlinedFunction>(
        RepeatedSequenceLocs, SequenceSize, 0, MachineOutlinerTailCall);
  }

  // BL @symbol is 4 bytes and writes only the TMS9900 link register R11.
  constexpr unsigned CallOverhead = 4;
  for (outliner::Candidate &C : RepeatedSequenceLocs)
    C.setCallInfo(MachineOutlinerDefault, CallOverhead);

  // Outlined functions return with B *R11, a 2-byte instruction.
  constexpr unsigned FrameOverhead = 2;
  return std::make_unique<outliner::OutlinedFunction>(
      RepeatedSequenceLocs, SequenceSize, FrameOverhead,
      MachineOutlinerDefault);
}

outliner::InstrType TMS9900InstrInfo::getOutliningTypeImpl(
    const MachineModuleInfo &MMI, MachineBasicBlock::iterator &MBBI,
    unsigned Flags) const {
  MachineInstr &MI = *MBBI;
  const MachineFunction *MF = MI.getMF();
  const TargetRegisterInfo *TRI = MF->getSubtarget().getRegisterInfo();

  if (MI.isDebugInstr() || MI.isPosition())
    return outliner::InstrType::Invisible;

  if (MI.getOpcode() == TMS9900::RET_REAL)
    return outliner::InstrType::LegalTerminator;

  if (MI.isCFIInstruction() || MI.isInlineAsm() || MI.isCall() ||
      MI.isBranch() || MI.isReturn() || MI.isTerminator() || MI.isBarrier())
    return outliner::InstrType::Illegal;

  if (MI.getDesc().isPseudo())
    return outliner::InstrType::Illegal;

  if ((MI.mayLoad() || MI.mayStore()) &&
      !isTMS9900OutlinerMemoryOpcode(MI.getOpcode()))
    return outliner::InstrType::Illegal;

  if (!isPureTMS9900OutlinerOpcode(MI.getOpcode()) &&
      !isTMS9900OutlinerMemoryOpcode(MI.getOpcode()))
    return outliner::InstrType::Illegal;

  // BL/B-based outlining leaves R10/SP alone, so stack-relative loads and
  // stores are safe. Do not outline instructions that change SP itself.
  if (MI.modifiesRegister(TMS9900::R10, TRI) ||
      MI.readsRegister(TMS9900::R11, TRI) ||
      MI.modifiesRegister(TMS9900::R11, TRI))
    return outliner::InstrType::Illegal;

  return outliner::InstrType::Legal;
}

void TMS9900InstrInfo::buildOutlinedFrame(
    MachineBasicBlock &MBB, MachineFunction &MF,
    const outliner::OutlinedFunction &OF) const {
  if (OF.FrameConstructionID == MachineOutlinerTailCall)
    return;

  assert(OF.FrameConstructionID == MachineOutlinerDefault &&
         "unexpected TMS9900 outliner frame construction ID");
  MBB.addLiveIn(TMS9900::R11);
  MBB.insert(MBB.end(), BuildMI(MF, DebugLoc(), get(TMS9900::RET_REAL)));
}

MachineBasicBlock::iterator TMS9900InstrInfo::insertOutlinedCall(
    Module &M, MachineBasicBlock &MBB, MachineBasicBlock::iterator &It,
    MachineFunction &MF, outliner::Candidate &C) const {
  if (C.CallConstructionID == MachineOutlinerTailCall)
    It = MBB.insert(It, BuildMI(*MBB.getParent(), DebugLoc(),
                                get(TMS9900::TAIL_B))
                            .addGlobalAddress(M.getNamedValue(MF.getName())));
  else
    It = MBB.insert(It, BuildMI(*MBB.getParent(), DebugLoc(),
                                get(TMS9900::BL_OUTLINE))
                            .addGlobalAddress(M.getNamedValue(MF.getName())));
  return It;
}

void TMS9900InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator I,
                                    const DebugLoc &DL, MCRegister DestReg,
                                    MCRegister SrcReg, bool KillSrc,
                                    bool RenamableDest,
                                    bool RenamableSrc) const {
  // Use MOV instruction for register copy
  // MOV Rs,Rd copies Rs to Rd
  BuildMI(MBB, I, DL, get(TMS9900::MOVrr), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc));
}

void TMS9900InstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
    bool isKill, int FrameIndex, const TargetRegisterClass *RC,
    const TargetRegisterInfo *TRI, Register VReg,
    MachineInstr::MIFlag Flags) const {
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
    const TargetRegisterInfo *TRI, Register VReg,
    MachineInstr::MIFlag Flags) const {
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

    // If we see a non-terminator, we're done
    if (!isUnpredicatedTerminator(*I))
      break;

    // A terminator that isn't a branch can't be handled.
    if (!I->isBranch())
      return true;

    // Cannot handle indirect branches.
    if (I->isIndirectBranch())
      return true;

    unsigned Opc = I->getOpcode();

    // Handle unconditional branch
    if (Opc == TMS9900::JMP) {
      if (!AllowModify) {
        TBB = I->getOperand(0).getMBB();
        continue;
      }

      // If the block has any instructions after a JMP, delete them.
      MBB.erase(std::next(I), MBB.end());
      Cond.clear();
      FBB = nullptr;

      // Delete the JMP if it's equivalent to a fall-through.
      if (MBB.isLayoutSuccessor(I->getOperand(0).getMBB())) {
        TBB = nullptr;
        I->eraseFromParent();
        I = MBB.end();
        continue;
      }

      // TBB is used to indicate the unconditional destination.
      TBB = I->getOperand(0).getMBB();
      continue;
    }

    if (Opc == TMS9900::CMPBRrr || Opc == TMS9900::CMPBRri) {
      if (!Cond.empty())
        return true;

      FBB = TBB;
      TBB = I->getOperand(3).getMBB();
      Cond.push_back(MachineOperand::CreateImm(Opc));
      Cond.push_back(I->getOperand(0));
      Cond.push_back(I->getOperand(1));
      Cond.push_back(I->getOperand(2));
      continue;
    }

    // Handle conditional branches
    if (!isCondBranchOpcode(Opc))
      return true;

    // Working from the bottom, handle the first conditional branch.
    if (Cond.empty()) {
      FBB = TBB;
      TBB = I->getOperand(0).getMBB();
      Cond.push_back(MachineOperand::CreateImm(Opc));
      continue;
    }

    // Handle subsequent conditional branches. Only handle the case where all
    // conditional branches branch to the same destination.
    if (TBB != I->getOperand(0).getMBB())
      return true;

    unsigned OldOpc = Cond[0].getImm();
    if (OldOpc == Opc)
      continue;

    return true;
  }

  bool CanInvert = false;
  if (!Cond.empty() && Cond[0].isImm()) {
    SmallVector<MachineOperand, 4> CondCopy(Cond.begin(), Cond.end());
    CanInvert = !reverseBranchCondition(CondCopy);
  }

  if (!AllowModify && !Cond.empty() && !FBB && TBB && MBB.succ_size() == 2 &&
      MBB.isLayoutSuccessor(TBB) && CanInvert) {
    for (MachineBasicBlock *Succ : MBB.successors()) {
      if (Succ != TBB) {
        FBB = Succ;
        break;
      }
    }
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
  assert((Cond.size() == 0 || Cond.size() == 1 || Cond.size() == 4) &&
         "TMS9900 branch conditions have zero, one, or four components");

  if (BytesAdded)
    *BytesAdded = 0;

  if (Cond.empty()) {
    // Unconditional branch
    MachineInstr &MI = *BuildMI(&MBB, DL, get(TMS9900::JMP)).addMBB(TBB);
    if (BytesAdded)
      *BytesAdded += getInstSizeInBytes(MI);
    return 1;
  }

  unsigned Opc = Cond[0].getImm();

  if (Opc == TMS9900::CMPBRrr || Opc == TMS9900::CMPBRri) {
    assert(Cond.size() == 4 && "CMPBR expects 4 condition operands");
    MachineInstrBuilder MIB = BuildMI(&MBB, DL, get(Opc));
    MIB.add(Cond[1]); // LHS
    MIB.add(Cond[2]); // RHS
    MIB.add(Cond[3]); // CC
    MIB.addMBB(TBB);
    unsigned Count = 1;
    if (BytesAdded)
      *BytesAdded += getInstSizeInBytes(*MIB);
    if (FBB) {
      MachineInstr &MI =
          *BuildMI(&MBB, DL, get(TMS9900::JMP)).addMBB(FBB);
      if (BytesAdded)
        *BytesAdded += getInstSizeInBytes(MI);
      ++Count;
    }
    return Count;
  }

  assert(isCondBranchOpcode(Opc) &&
         "invalid TMS9900 branch condition opcode");

  // Conditional branch
  unsigned Count = 0;
  MachineInstr &CondMI = *BuildMI(&MBB, DL, get(Opc)).addMBB(TBB);
  if (BytesAdded)
    *BytesAdded += getInstSizeInBytes(CondMI);
  ++Count;

  if (FBB) {
    // Two-way Conditional branch. Insert the second branch.
    MachineInstr &MI = *BuildMI(&MBB, DL, get(TMS9900::JMP)).addMBB(FBB);
    if (BytesAdded)
      *BytesAdded += getInstSizeInBytes(MI);
    ++Count;
  }

  return Count;
}

unsigned TMS9900InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                         int *BytesRemoved) const {
  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;

  if (BytesRemoved)
    *BytesRemoved = 0;

  while (I != MBB.begin()) {
    --I;

    if (I->isDebugInstr())
      continue;

    if (!I->isBranch())
      break;

    if (BytesRemoved)
      *BytesRemoved += getInstSizeInBytes(*I);

    // Remove the branch
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }

  return Count;
}

bool TMS9900InstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  if (Cond.empty() || !Cond[0].isImm())
    return true;

  unsigned Opc = Cond[0].getImm();
  if (Opc == TMS9900::CMPBRrr || Opc == TMS9900::CMPBRri) {
    if (Cond.size() != 4 || !Cond[3].isImm())
      return true;
    ISD::CondCode CC =
        static_cast<ISD::CondCode>(Cond[3].getImm());
    ISD::CondCode Inverted = ISD::getSetCCInverse(CC, MVT::i16);
    if (Inverted == ISD::SETCC_INVALID)
      return true;
    Cond[3].setImm(Inverted);
    return false;
  }

  if (Cond.size() != 1)
    return true;

  unsigned Inverted = getOppositeCondBranchOpcode(Opc);
  if (!Inverted)
    return true;

  Cond[0].setImm(Inverted);
  return false;
}

unsigned TMS9900InstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  case TargetOpcode::CFI_INSTRUCTION:
  case TargetOpcode::EH_LABEL:
  case TargetOpcode::IMPLICIT_DEF:
  case TargetOpcode::KILL:
  case TargetOpcode::DBG_VALUE:
    return 0;
  case TMS9900::CMPBRrr:
  case TMS9900::CMPBRri:
    assert(MI.getOperand(2).isImm() && "CMPBR condition must be immediate");
    return getCmpBrSizeInBytes(
        MI.getOpcode(), static_cast<ISD::CondCode>(MI.getOperand(2).getImm()));
  case TargetOpcode::INLINEASM:
  case TargetOpcode::INLINEASM_BR: {
    const MachineFunction &MF = *MI.getParent()->getParent();
    return getInlineAsmLength(MI.getOperand(0).getSymbolName(),
                              *MF.getTarget().getMCAsmInfo());
  }
  default:
    return get(MI.getOpcode()).getSize();
  }
}

bool TMS9900InstrInfo::isBranchOffsetInRange(unsigned BranchOpc,
                                              int64_t BrOffset) const {
  switch (BranchOpc) {
  case TMS9900::JMP:
  case TMS9900::JEQ:
  case TMS9900::JNE:
  case TMS9900::JGT:
  case TMS9900::JLT:
  case TMS9900::JH:
  case TMS9900::JHE:
  case TMS9900::JL:
  case TMS9900::JLE:
  case TMS9900::JOC:
  case TMS9900::JNC:
  case TMS9900::JNO:
  case TMS9900::JOP: {
    if (BrOffset & 1)
      return false;
    int64_t WordOffset = (BrOffset >> 1) - 1;
    return isInt<8>(WordOffset);
  }
  case TMS9900::B_sym:
    return true;
  default:
    llvm_unreachable("unknown branch opcode");
  }
}

MachineBasicBlock *
TMS9900InstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
  unsigned Opc = MI.getOpcode();
  if (Opc == TMS9900::CMPBRrr || Opc == TMS9900::CMPBRri)
    return MI.getOperand(3).getMBB();
  if (Opc == TMS9900::JMP || Opc == TMS9900::B_sym || isCondBranchOpcode(Opc))
    return MI.getOperand(0).getMBB();
  return nullptr;
}

void TMS9900InstrInfo::insertIndirectBranch(MachineBasicBlock &MBB,
                                            MachineBasicBlock &NewDestBB,
                                            MachineBasicBlock &RestoreBB,
                                            const DebugLoc &DL,
                                            int64_t BrOffset,
                                            RegScavenger *RS) const {
  (void)RestoreBB;
  (void)BrOffset;
  (void)RS;
  BuildMI(&MBB, DL, get(TMS9900::B_sym)).addMBB(&NewDestBB);
}

bool TMS9900InstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();

  auto CopyImplicitOperands = [&MI](MachineInstrBuilder &MIB) {
    for (const MachineOperand &MO : MI.implicit_operands())
      MIB.add(MO);
  };

  switch (MI.getOpcode()) {
  default:
    return false;
  case TMS9900::CMPBRrr:
  case TMS9900::CMPBRri: {
    bool IsImm = (MI.getOpcode() == TMS9900::CMPBRri);
    Register LHS = MI.getOperand(0).getReg();
    MachineBasicBlock *Target = MI.getOperand(3).getMBB();
    ISD::CondCode CC =
        static_cast<ISD::CondCode>(MI.getOperand(2).getImm());

    if (IsImm) {
      int64_t Imm = MI.getOperand(1).getImm();
      BuildMI(MBB, MI, DL, get(TMS9900::CI)).addReg(LHS).addImm(Imm);
    } else {
      Register RHS = MI.getOperand(1).getReg();
      BuildMI(MBB, MI, DL, get(TMS9900::Crr)).addReg(LHS).addReg(RHS);
    }

    if (CC == ISD::SETGE || CC == ISD::SETLE) {
      unsigned SecondOpc = (CC == ISD::SETGE) ? TMS9900::JGT : TMS9900::JLT;
      BuildMI(MBB, MI, DL, get(TMS9900::JEQ)).addMBB(Target);
      BuildMI(MBB, MI, DL, get(SecondOpc)).addMBB(Target);
    } else {
      unsigned JumpOpc = getJumpOpcodeForCC(CC);
      BuildMI(MBB, MI, DL, get(JumpOpc)).addMBB(Target);
    }

    MBB.erase(MI);
    return true;
  }
  case TMS9900::RET:
    // Expand RET pseudo to B *R11
    {
      MachineInstrBuilder MIB =
          BuildMI(MBB, MI, DL, get(TMS9900::RET_REAL));
      CopyImplicitOperands(MIB);
    }
    MBB.erase(MI);
    return true;
  case TMS9900::TCRETURN:
  case TMS9900::TCRETURN_ext: {
    // Expand TCRETURN/TCRETURN_ext pseudo to B @target (tail call)
    // Uses TAIL_B instruction which takes a calltarget operand.
    MachineOperand &Target = MI.getOperand(0);
    MachineInstrBuilder MIB =
        BuildMI(MBB, MI, DL, get(TMS9900::TAIL_B)).add(Target);
    CopyImplicitOperands(MIB);
    MBB.erase(MI);
    return true;
  }
  case TMS9900::TCRETURN_ind: {
    // Expand TCRETURN_ind pseudo to B *Rx (branch indirect through register)
    Register TargetReg = MI.getOperand(0).getReg();
    MachineInstrBuilder MIB =
        BuildMI(MBB, MI, DL, get(TMS9900::TAIL_B_IND)).addReg(TargetReg);
    CopyImplicitOperands(MIB);
    MBB.erase(MI);
    return true;
  }
  case TMS9900::ANDrr: {
    // Expand ANDrr pseudo to INV+SZC+INV sequence
    // AND rd, rs2 becomes:
    //   INV rs2      ; rs2 = NOT rs2
    //   SZC rs2, rd  ; rd = rd AND (NOT rs2) = rd AND (NOT (NOT original_rs2)) = rd AND original_rs2
    //   INV rs2      ; restore rs2 (skipped if rs2 is killed here)
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(2).getReg();

    // Coalescing identical inputs can make all three operands the same
    // physical register.  The normal INV+SZC sequence would then clear the
    // register instead of computing x & x.  A self-move preserves the value
    // and sets the comparison flags from the result, just like a logical AND.
    // The pre-emit peephole removes it when ST is dead.
    if (DstReg == SrcReg) {
      MachineInstrBuilder MIB =
          BuildMI(MBB, MI, DL, get(TMS9900::MOVrr), DstReg).addReg(SrcReg);
      const TargetRegisterInfo *TRI =
          MBB.getParent()->getSubtarget().getRegisterInfo();
      if (MI.registerDefIsDead(TMS9900::ST, TRI)) {
        if (int STIdx =
                MIB->findRegisterDefOperandIdx(TMS9900::ST, TRI);
            STIdx != -1)
          MIB->getOperand(STIdx).setIsDead();
      }
      MBB.erase(MI);
      return true;
    }

    // Use isKill() not isDead(): operand 2 is a use, not a def.
    // isKill() indicates the source register's last use is this instruction,
    // meaning no restore INV is needed. isDead() is for def operands only
    // and would always return false here.
    bool SrcIsKilled = MI.getOperand(2).isKill();

    // INV rs2
    BuildMI(MBB, MI, DL, get(TMS9900::INVr), SrcReg)
        .addReg(SrcReg);
    // SZC rs2, rd
    BuildMI(MBB, MI, DL, get(TMS9900::SZCrr), DstReg)
        .addReg(DstReg)
        .addReg(SrcReg);
    // INV rs2 (restore) — only needed if rs2 is still live after this AND
    if (!SrcIsKilled) {
      BuildMI(MBB, MI, DL, get(TMS9900::INVr), SrcReg)
          .addReg(SrcReg);
    }

    MBB.erase(MI);
    return true;
  }
  }
}
