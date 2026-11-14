//===-- I8085HLTracking.cpp - HL register tracking optimization -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// After pseudo expansion, the 8085 backend emits many LXI H,offset / DAD SP
// sequences to compute stack addresses. This pass tracks the known value of
// HL (as SP + constant) and eliminates redundant LXI+DAD pairs by replacing
// them with INX H / DCX H sequences when the delta is small enough.
//
// Tracking propagates across basic block boundaries: if all predecessors of a
// block agree on HL = SP + N, the block starts with that known state.
//
//===----------------------------------------------------------------------===//

#include "I8085.h"
#include "I8085InstrInfo.h"
#include "I8085Subtarget.h"
#include "MCTargetDesc/I8085MCTargetDesc.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "i8085-hl-tracking"

// Maximum delta for INX/DCX replacement. At delta=3, INX*3 = 18 cycles
// vs LXI+DAD = 20 cycles, so it's still a win.
static const int MaxDelta = 3;

namespace {

enum class HLBaseKind {
  Unknown,
  SP,
  GlobalAddress,
  ExternalSymbol,
  ConstantPoolIndex,
  JumpTableIndex,
  BlockAddress,
  MachineBasicBlock,
};

struct HLBase {
  HLBaseKind Kind = HLBaseKind::Unknown;
  const void *Ptr = nullptr;
  int Index = 0;
  unsigned TargetFlags = 0;
  StringRef SymbolName;

  bool isValid() const { return Kind != HLBaseKind::Unknown; }

  bool operator==(const HLBase &Other) const {
    if (Kind != Other.Kind || TargetFlags != Other.TargetFlags)
      return false;

    switch (Kind) {
    case HLBaseKind::Unknown:
    case HLBaseKind::SP:
      return true;
    case HLBaseKind::GlobalAddress:
    case HLBaseKind::BlockAddress:
    case HLBaseKind::MachineBasicBlock:
      return Ptr == Other.Ptr;
    case HLBaseKind::ExternalSymbol:
      return SymbolName == Other.SymbolName;
    case HLBaseKind::ConstantPoolIndex:
    case HLBaseKind::JumpTableIndex:
      return Index == Other.Index;
    }

    llvm_unreachable("unknown HL base kind");
  }
};

struct HLState {
  HLBase Base;
  int64_t Offset = 0;

  bool isTracked() const { return Base.isValid(); }
  bool isSPRelative() const { return Base.Kind == HLBaseKind::SP; }

  static HLState getSPRelative(int64_t Offset) {
    HLState State;
    State.Base.Kind = HLBaseKind::SP;
    State.Offset = Offset;
    return State;
  }

  bool operator==(const HLState &Other) const {
    if (!isTracked() || !Other.isTracked())
      return !isTracked() && !Other.isTracked();
    return Base == Other.Base && Offset == Other.Offset;
  }
};

class I8085HLTracking : public MachineFunctionPass {
public:
  static char ID;

  I8085HLTracking() : MachineFunctionPass(ID) {
    initializeI8085HLTrackingPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "I8085 HL register tracking optimization";
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
    const int BlockMaxDelta = MF.getFunction().hasMinSize() ? 4 : MaxDelta;
    bool Changed = false;

    ExitStates.assign(MF.getNumBlockIDs(), PairState());

    // Pass 1: process all blocks with no cross-BB info (populates exit states)
    for (MachineBasicBlock &MBB : MF) {
      PairState entry;
      ExitStates[MBB.getNumber()] =
          processBlock(MBB, entry, TII, BlockMaxDelta, Changed);
    }

    // Pass 2: re-process with predecessor exit states available
    for (MachineBasicBlock &MBB : MF) {
      PairState entry = mergeEntryState(MBB);
      if (entry.HL.isTracked() && !MBB.isLiveIn(I8085::HL))
        MBB.addLiveIn(I8085::HL);
      if (entry.DE.isTracked() && !MBB.isLiveIn(I8085::DE))
        MBB.addLiveIn(I8085::DE);
      ExitStates[MBB.getNumber()] =
          processBlock(MBB, entry, TII, BlockMaxDelta, Changed);
    }

    return Changed;
  }

private:
  struct PairState {
    HLState HL;
    HLState DE;
  };

  SmallVector<PairState> ExitStates;

  PairState mergeEntryState(const MachineBasicBlock &MBB) const {
    PairState result;
    bool first = true;

    for (const MachineBasicBlock *Pred : MBB.predecessors()) {
      const PairState &PredExit = ExitStates[Pred->getNumber()];
      if (first) {
        result = PredExit;
        first = false;
      } else {
        if (!(result.HL == PredExit.HL))
          result.HL = HLState();
        if (!(result.DE == PredExit.DE))
          result.DE = HLState();
      }
      if (!result.HL.isTracked() && !result.DE.isTracked())
        return PairState();
    }

    if (first)
      return PairState();

    return result;
  }

  PairState processBlock(MachineBasicBlock &MBB, PairState State,
                         const TargetInstrInfo *TII, int BlockMaxDelta,
                         bool &Changed) {
    HLState Current = State.HL;
    HLState CurrentDE = State.DE;

    for (auto MI = MBB.begin(); MI != MBB.end();) {
      if (MI->getOpcode() == I8085::LXI &&
          MI->getNumOperands() >= 2 &&
          MI->getOperand(0).isReg()) {
        Register DstPair = MI->getOperand(0).getReg();
        if (DstPair == I8085::HL) {
          auto Next = std::next(MI);

          // Check for LXI H, imm followed by DAD SP.
          if (MI->getOperand(1).isImm() && Next != MBB.end() &&
              Next->getOpcode() == I8085::DAD && Next->getNumOperands() >= 1 &&
              Next->getOperand(0).isReg() &&
              Next->getOperand(0).getReg() == I8085::SP) {
            HLState Target =
                HLState::getSPRelative(MI->getOperand(1).getImm());
            if (tryReuseHL(MBB, MI, Next, Current, Target, TII, BlockMaxDelta,
                           Changed))
              continue;

            Current = Target;
            MI = std::next(Next);
            continue;
          }

          HLState Target;
          if (getSymbolicLXIState(MI->getOperand(1), Target)) {
            if (tryReuseHL(MBB, MI, MI, Current, Target, TII, BlockMaxDelta,
                           Changed))
              continue;

            Current = Target;
            ++MI;
            continue;
          }
        } else if (DstPair == I8085::DE) {
          HLState Target;
          if (getSymbolicLXIState(MI->getOperand(1), Target)) {
            CurrentDE = Target;
            ++MI;
            continue;
          }
        }
      }

      // XCHG swaps HL and DE without touching SP or flags: swap tracked states!
      if (MI->getOpcode() == I8085::XCHG) {
        std::swap(Current, CurrentDE);
        ++MI;
        continue;
      }

      // Adjacent pair copies MOV D,H ; MOV E,L or MOV H,D ; MOV L,E propagate
      // tracked addresses between HL and DE.
      if (MI->getOpcode() == I8085::MOV && MI->getNumOperands() >= 2 &&
          MI->getOperand(0).isReg() && MI->getOperand(1).isReg()) {
        auto Next = std::next(MI);
        if (Next != MBB.end() && Next->getOpcode() == I8085::MOV &&
            Next->getNumOperands() >= 2 && Next->getOperand(0).isReg() &&
            Next->getOperand(1).isReg()) {
          Register D0 = MI->getOperand(0).getReg();
          Register S0 = MI->getOperand(1).getReg();
          Register D1 = Next->getOperand(0).getReg();
          Register S1 = Next->getOperand(1).getReg();
          if (((D0 == I8085::D && S0 == I8085::H &&
                D1 == I8085::E && S1 == I8085::L) ||
               (D0 == I8085::E && S0 == I8085::L &&
                D1 == I8085::D && S1 == I8085::H))) {
            CurrentDE = Current;
            MI = std::next(Next);
            continue;
          }
          if (((D0 == I8085::H && S0 == I8085::D &&
                D1 == I8085::L && S1 == I8085::E) ||
               (D0 == I8085::L && S0 == I8085::E &&
                D1 == I8085::H && S1 == I8085::D))) {
            Current = CurrentDE;
            MI = std::next(Next);
            continue;
          }
        }
      }

      // PUSH: SP -= 2, HL and DE unchanged. Adjust tracked SP-relative offsets.
      if (MI->getOpcode() == I8085::PUSH) {
        if (Current.isSPRelative())
          Current.Offset += 2;
        if (CurrentDE.isSPRelative())
          CurrentDE.Offset += 2;
        ++MI;
        continue;
      }

      // POP: SP += 2. POP H/D clobbers that pair; others adjust SP offset.
      if (MI->getOpcode() == I8085::POP) {
        unsigned Reg = MI->getOperand(0).getReg();
        if (Reg == I8085::HL) {
          Current = HLState();
        } else if (Current.isSPRelative()) {
          Current.Offset -= 2;
        }
        if (Reg == I8085::DE) {
          CurrentDE = HLState();
        } else if (CurrentDE.isSPRelative()) {
          CurrentDE.Offset -= 2;
        }
        ++MI;
        continue;
      }

      // INX/DCX H or D update tracked offsets rather than invalidating them.
      if (MI->getOpcode() == I8085::INX &&
          MI->getNumOperands() >= 1 &&
          MI->getOperand(0).isReg()) {
        Register Reg = MI->getOperand(0).getReg();
        if (Reg == I8085::HL) {
          if (Current.isTracked())
            Current.Offset++;
          ++MI;
          continue;
        }
        if (Reg == I8085::DE) {
          if (CurrentDE.isTracked())
            CurrentDE.Offset++;
          ++MI;
          continue;
        }
      }

      if (MI->getOpcode() == I8085::DCX &&
          MI->getNumOperands() >= 1 &&
          MI->getOperand(0).isReg()) {
        Register Reg = MI->getOperand(0).getReg();
        if (Reg == I8085::HL) {
          if (Current.isTracked())
            Current.Offset--;
          ++MI;
          continue;
        }
        if (Reg == I8085::DE) {
          if (CurrentDE.isTracked())
            CurrentDE.Offset--;
          ++MI;
          continue;
        }
      }

      if (clobbersHL(*MI))
        Current = HLState();
      if (clobbersDE(*MI))
        CurrentDE = HLState();

      ++MI;
    }

    return {Current, CurrentDE};
  }

  bool getSymbolicLXIState(const MachineOperand &MO, HLState &State) const {
    HLBase Base;
    int64_t Offset = 0;

    switch (MO.getType()) {
    case MachineOperand::MO_GlobalAddress:
      Base.Kind = HLBaseKind::GlobalAddress;
      Base.Ptr = MO.getGlobal();
      Base.TargetFlags = MO.getTargetFlags();
      Offset = MO.getOffset();
      break;
    case MachineOperand::MO_ExternalSymbol:
      Base.Kind = HLBaseKind::ExternalSymbol;
      Base.SymbolName = MO.getSymbolName();
      Base.TargetFlags = MO.getTargetFlags();
      Offset = MO.getOffset();
      break;
    case MachineOperand::MO_ConstantPoolIndex:
      Base.Kind = HLBaseKind::ConstantPoolIndex;
      Base.Index = MO.getIndex();
      Base.TargetFlags = MO.getTargetFlags();
      Offset = MO.getOffset();
      break;
    case MachineOperand::MO_JumpTableIndex:
      Base.Kind = HLBaseKind::JumpTableIndex;
      Base.Index = MO.getIndex();
      Base.TargetFlags = MO.getTargetFlags();
      // Jump-table operands do not carry an addend in MachineOperand.
      Offset = 0;
      break;
    case MachineOperand::MO_BlockAddress:
      Base.Kind = HLBaseKind::BlockAddress;
      Base.Ptr = MO.getBlockAddress();
      Base.TargetFlags = MO.getTargetFlags();
      Offset = MO.getOffset();
      break;
    case MachineOperand::MO_MachineBasicBlock:
      Base.Kind = HLBaseKind::MachineBasicBlock;
      Base.Ptr = MO.getMBB();
      Base.TargetFlags = MO.getTargetFlags();
      Offset = 0;
      break;
    default:
      return false;
    }

    State.Base = Base;
    State.Offset = Offset;
    return true;
  }

  void emitHLDelta(MachineBasicBlock &MBB, MachineBasicBlock::iterator InsertPt,
                   const DebugLoc &DL, int64_t Delta,
                   const TargetInstrInfo *TII) const {
    unsigned Opc = (Delta > 0) ? I8085::INX : I8085::DCX;
    int Count = static_cast<int>((Delta > 0) ? Delta : -Delta);

    for (int i = 0; i < Count; i++) {
      BuildMI(MBB, InsertPt, DL, TII->get(Opc))
          .addReg(I8085::HL, RegState::Define)
          .addReg(I8085::HL, RegState::Implicit);
    }
  }

  bool tryReuseHL(MachineBasicBlock &MBB, MachineBasicBlock::iterator &MI,
                  MachineBasicBlock::iterator LastToErase, HLState &Current,
                  const HLState &Target, const TargetInstrInfo *TII,
                  int BlockMaxDelta, bool &Changed) const {
    if (!Current.isTracked() || !(Current.Base == Target.Base))
      return false;

    int64_t Delta = Target.Offset - Current.Offset;
    // Symbolic LXI H, sym is 3 bytes (delta <= 2 saves bytes, == 3 ties);
    // SP-relative LXI H, imm ; DAD SP is 4 bytes (delta <= 3 saves bytes, == 4 ties).
    int EffectiveMax =
        (LastToErase->getOpcode() == I8085::DAD) ? BlockMaxDelta : MaxDelta;
    if (Delta < -EffectiveMax || Delta > EffectiveMax)
      return false;

    if (LastToErase->getOpcode() == I8085::DAD) {
      LivePhysRegs Live(*MBB.getParent()->getSubtarget().getRegisterInfo());
      Live.addLiveOuts(MBB);
      for (auto I = MBB.end(); I != std::next(LastToErase);)
        Live.stepBackward(*--I);
      if (Live.contains(I8085::SREG))
        return false;
    }

    if (Delta != 0)
      emitHLDelta(MBB, MI, MI->getDebugLoc(), Delta, TII);

    auto After = std::next(LastToErase);
    for (auto I = MI; I != After;) {
      auto ToErase = I++;
      ToErase->eraseFromParent();
    }

    Current = Target;
    Changed = true;
    MI = After;
    return true;
  }

  bool clobbersHL(const MachineInstr &MI) const {
    unsigned Opc = MI.getOpcode();

    // Instructions that always clobber HL or change SP
    switch (Opc) {
    case I8085::ARHL:
    case I8085::CALL:
    case I8085::DSUB:
    case I8085::LHLD:
    case I8085::LHLX:
    case I8085::RET:
    case I8085::SPHL:
    case I8085::XCHG:
    case I8085::XTHL:
      return true;
    default:
      break;
    }

    // PUSH/POP are handled explicitly in processBlock (offset adjustment).
    // They do NOT go through clobbersHL().

    // DAD with any register (B, D, SP, HL) modifies HL
    // (DAD SP as part of LXI+DAD is handled above, standalone DAD is a clobber)
    if (Opc == I8085::DAD)
      return true;

    // LXI H with a non-SP-relative use (standalone LXI H)
    if (Opc == I8085::LXI && MI.getNumOperands() >= 1 &&
        MI.getOperand(0).isReg() && MI.getOperand(0).getReg() == I8085::HL)
      return true;

    // Check explicit operand defs for H, L, or HL
    for (const MachineOperand &MO : MI.operands()) {
      if (!MO.isReg() || !MO.isDef())
        continue;
      Register Reg = MO.getReg();
      if (Reg == I8085::H || Reg == I8085::L || Reg == I8085::HL)
        return true;
    }

    return false;
  }

  bool clobbersDE(const MachineInstr &MI) const {
    unsigned Opc = MI.getOpcode();

    switch (Opc) {
    case I8085::CALL:
    case I8085::LDHI:
    case I8085::LDSI:
    case I8085::RDEL:
    case I8085::RET:
    case I8085::SPHL:
    case I8085::XCHG:
      return true;
    default:
      break;
    }

    if (Opc == I8085::LXI && MI.getNumOperands() >= 1 &&
        MI.getOperand(0).isReg() && MI.getOperand(0).getReg() == I8085::DE)
      return true;

    for (const MachineOperand &MO : MI.operands()) {
      if (!MO.isReg() || !MO.isDef())
        continue;
      Register Reg = MO.getReg();
      if (Reg == I8085::D || Reg == I8085::E || Reg == I8085::DE)
        return true;
    }

    return false;
  }
};
} // end anonymous namespace

char I8085HLTracking::ID = 0;

INITIALIZE_PASS(I8085HLTracking, "i8085-hl-tracking",
                "I8085 HL register tracking optimization", false, false)

FunctionPass *llvm::createI8085HLTrackingPass() {
  return new I8085HLTracking();
}
