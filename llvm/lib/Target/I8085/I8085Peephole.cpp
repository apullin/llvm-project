//===-- I8085Peephole.cpp - I8085 peephole optimizations -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains simple I8085 peephole optimizations.
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

#define DEBUG_TYPE "i8085-peephole"

namespace {
class I8085Peephole : public MachineFunctionPass {
public:
  static char ID;

  I8085Peephole() : MachineFunctionPass(ID) {
    initializeI8085PeepholePass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return "I8085 peephole optimizations"; }

  bool runOnMachineFunction(MachineFunction &MF) override {
    bool Changed = false;
    MachineRegisterInfo &MRI = MF.getRegInfo();

    for (MachineBasicBlock &MBB : MF) {
      for (auto MI = MBB.begin(); MI != MBB.end();) {
        auto Next = std::next(MI);
        if (MI->getOpcode() == I8085::MOV && MI->getNumOperands() >= 2 &&
            MI->getOperand(0).isReg() && MI->getOperand(1).isReg()) {
          Register Dst = MI->getOperand(0).getReg();
          Register Src = MI->getOperand(1).getReg();
          if (Dst == Src) {
            MI = MBB.erase(MI);
            Changed = true;
            continue;
          }
        }

        if (Next != MBB.end() && MI->getOpcode() == I8085::MOV &&
            Next->getOpcode() == I8085::MOV) {
          if (MI->getNumOperands() >= 2 && Next->getNumOperands() >= 2 &&
              MI->getOperand(0).isReg() && MI->getOperand(1).isReg() &&
              Next->getOperand(0).isReg() && Next->getOperand(1).isReg()) {
            Register Tmp = MI->getOperand(0).getReg();
            Register Src = MI->getOperand(1).getReg();
            Register Dst = Next->getOperand(0).getReg();
            Register Use = Next->getOperand(1).getReg();

            // Reverse-pair elimination: MOV X,Y ; MOV Y,X -> MOV X,Y
            // After "MOV X,Y", register X already holds Y's value and Y is
            // unchanged, so "MOV Y,X" is redundant.
            if (Use == Tmp && Dst == Src &&
                Tmp != I8085::M && Src != I8085::M) {
              bool CanElim = true;
              if (Tmp.isVirtual()) {
                if (!MRI.hasOneUse(Tmp))
                  CanElim = false;
              }
              if (CanElim) {
                auto NextAfter = std::next(Next);
                Next->eraseFromParent();
                Changed = true;
                MI = NextAfter;
                continue;
              }
            }

            bool CanFold = (Use == Tmp) && (Dst != Tmp);
            if (CanFold) {
              if (Tmp.isVirtual()) {
                if (!MRI.hasOneUse(Tmp))
                  CanFold = false;
              } else if (!Next->getOperand(1).isKill()) {
                CanFold = false;
              }
            }

            if (CanFold && Src == I8085::M && Dst == I8085::M)
              CanFold = false;

            if (CanFold) {
              if (Dst == Src) {
                auto NextAfter = std::next(Next);
                Next->eraseFromParent();
                MI->eraseFromParent();
                Changed = true;
                MI = NextAfter;
                continue;
              }

              Next->getOperand(1).setReg(Src);
              Next->getOperand(1).setIsKill(false);
              MI->eraseFromParent();
              Changed = true;
              MI = Next;
              continue;
            }
          }
        }

        if (Next == MBB.end()) {
          ++MI;
          continue;
        }

        if ((MI->getOpcode() == I8085::INX && Next->getOpcode() == I8085::DCX) ||
            (MI->getOpcode() == I8085::DCX && Next->getOpcode() == I8085::INX)) {
          if (MI->getNumOperands() >= 1 && Next->getNumOperands() >= 1 &&
              MI->getOperand(0).isReg() && Next->getOperand(0).isReg()) {
            if (MI->getOperand(0).getReg() == Next->getOperand(0).getReg()) {
              auto NextAfter = std::next(Next);
              Next->eraseFromParent();
              MI->eraseFromParent();
              Changed = true;
              MI = NextAfter;
              continue;
            }
          }
        }

        if (MI->getOpcode() == I8085::XCHG && Next->getOpcode() == I8085::XCHG) {
          auto NextAfter = std::next(Next);
          Next->eraseFromParent();
          MI->eraseFromParent();
          Changed = true;
          MI = NextAfter;
          continue;
        }

        // ORI 0 -> ORA A (1 byte shorter, equivalent flag-setting).
        if (MI->getOpcode() == I8085::ORI && MI->getNumOperands() >= 1 &&
            MI->getOperand(0).isImm() && MI->getOperand(0).getImm() == 0) {
          const MCInstrDesc &Desc = MF.getSubtarget().getInstrInfo()->get(I8085::ORA);
          MI->setDesc(Desc);
          MI->removeOperand(0);
          MI->addOperand(MachineOperand::CreateReg(I8085::A, false));
          Changed = true;
          MI = Next;
          continue;
        }

        if (MI->getOpcode() != I8085::LXI || Next->getNumOperands() == 0) {
          ++MI;
          continue;
        }

        unsigned NextOpc = Next->getOpcode();
        if (NextOpc != I8085::INX && NextOpc != I8085::DCX) {
          ++MI;
          continue;
        }

        if (!MI->getOperand(0).isReg() || !MI->getOperand(1).isImm() ||
            !Next->getOperand(0).isReg()) {
          ++MI;
          continue;
        }

        Register LXIReg = MI->getOperand(0).getReg();
        Register StepReg = Next->getOperand(0).getReg();
        if (LXIReg != StepReg) {
          ++MI;
          continue;
        }

        int64_t Imm = MI->getOperand(1).getImm();
        uint16_t Val = static_cast<uint16_t>(Imm);
        if (NextOpc == I8085::INX)
          Val = static_cast<uint16_t>(Val + 1);
        else
          Val = static_cast<uint16_t>(Val - 1);

        MI->getOperand(1).setImm(Val);
        Next->eraseFromParent();
        Changed = true;
        ++MI;
        continue;
      }
    }

    return Changed;
  }
};
} // end anonymous namespace

char I8085Peephole::ID = 0;

INITIALIZE_PASS(I8085Peephole, "i8085-peephole",
                "I8085 peephole optimizations", false, false)

FunctionPass *llvm::createI8085PeepholePass() { return new I8085Peephole(); }
