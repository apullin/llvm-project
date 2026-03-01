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
//===----------------------------------------------------------------------===//

#include "I8085.h"
#include "I8085InstrInfo.h"
#include "I8085Subtarget.h"
#include "MCTargetDesc/I8085MCTargetDesc.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "i8085-hl-tracking"

// Maximum delta for INX/DCX replacement. At delta=3, INX*3 = 18 cycles
// vs LXI+DAD = 20 cycles, so it's still a win.
static const int MaxDelta = 3;

namespace {
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
    bool Changed = false;

    for (MachineBasicBlock &MBB : MF) {
      // tracked: whether we know HL = SP + offset
      bool tracked = false;
      int trackedOffset = 0;

      for (auto MI = MBB.begin(); MI != MBB.end();) {
        // Check for LXI H, imm followed by DAD SP
        if (MI->getOpcode() == I8085::LXI &&
            MI->getNumOperands() >= 2 &&
            MI->getOperand(0).isReg() &&
            MI->getOperand(0).getReg() == I8085::HL &&
            MI->getOperand(1).isImm()) {
          auto Next = std::next(MI);
          if (Next != MBB.end() &&
              Next->getOpcode() == I8085::DAD &&
              Next->getNumOperands() >= 1 &&
              Next->getOperand(0).isReg() &&
              Next->getOperand(0).getReg() == I8085::SP) {
            int targetOffset = static_cast<int>(MI->getOperand(1).getImm());

            if (tracked) {
              int delta = targetOffset - trackedOffset;

              if (delta == 0) {
                // HL already has the right value -- delete both instructions
                auto AfterDAD = std::next(Next);
                Next->eraseFromParent();
                MI->eraseFromParent();
                Changed = true;
                MI = AfterDAD;
                continue;
              }

              if (delta >= -MaxDelta && delta <= MaxDelta) {
                // Replace LXI+DAD with INX/DCX sequence
                DebugLoc DL = MI->getDebugLoc();
                unsigned Opc = (delta > 0) ? I8085::INX : I8085::DCX;
                int count = (delta > 0) ? delta : -delta;
                auto InsertPt = MI;

                for (int i = 0; i < count; i++) {
                  BuildMI(MBB, InsertPt, DL, TII->get(Opc))
                      .addReg(I8085::HL);
                }

                auto AfterDAD = std::next(Next);
                Next->eraseFromParent();
                MI->eraseFromParent();
                trackedOffset = targetOffset;
                Changed = true;
                MI = AfterDAD;
                continue;
              }
            }

            // Can't optimize, but update tracking
            trackedOffset = targetOffset;
            tracked = true;
            MI = std::next(Next);
            continue;
          }
        }

        // PUSH: SP -= 2, HL unchanged. Adjust tracked offset.
        if (MI->getOpcode() == I8085::PUSH) {
          if (tracked)
            trackedOffset += 2;
          ++MI;
          continue;
        }

        // POP: SP += 2. POP H clobbers HL; others just adjust offset.
        if (MI->getOpcode() == I8085::POP) {
          unsigned Reg = MI->getOperand(0).getReg();
          if (Reg == I8085::HL) {
            tracked = false;
          } else if (tracked) {
            trackedOffset -= 2;
          }
          ++MI;
          continue;
        }

        // Check if this instruction invalidates HL tracking
        if (clobbersHL(*MI)) {
          tracked = false;
        } else if (MI->getOpcode() == I8085::INX &&
                   MI->getNumOperands() >= 1 &&
                   MI->getOperand(0).isReg() &&
                   MI->getOperand(0).getReg() == I8085::HL) {
          if (tracked)
            trackedOffset++;
        } else if (MI->getOpcode() == I8085::DCX &&
                   MI->getNumOperands() >= 1 &&
                   MI->getOperand(0).isReg() &&
                   MI->getOperand(0).getReg() == I8085::HL) {
          if (tracked)
            trackedOffset--;
        }

        ++MI;
      }
    }

    return Changed;
  }

private:
  bool clobbersHL(const MachineInstr &MI) const {
    unsigned Opc = MI.getOpcode();

    // Instructions that always clobber HL or change SP
    switch (Opc) {
    case I8085::CALL:
    case I8085::RET:
    case I8085::XTHL:
    case I8085::SPHL:
    case I8085::LHLD:
    case I8085::XCHG:
      return true;
    default:
      break;
    }

    // PUSH/POP are handled explicitly in the main loop (offset adjustment).
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
};
} // end anonymous namespace

char I8085HLTracking::ID = 0;

INITIALIZE_PASS(I8085HLTracking, "i8085-hl-tracking",
                "I8085 HL register tracking optimization", false, false)

FunctionPass *llvm::createI8085HLTrackingPass() {
  return new I8085HLTracking();
}
