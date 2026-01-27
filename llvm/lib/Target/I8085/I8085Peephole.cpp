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

    for (MachineBasicBlock &MBB : MF) {
      for (auto MI = MBB.begin(); MI != MBB.end(); ) {
        auto Next = std::next(MI);
        if (Next == MBB.end()) {
          ++MI;
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
