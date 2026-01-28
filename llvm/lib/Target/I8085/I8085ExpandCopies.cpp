//===-- I8085ExpandCopies.cpp - Expand physical COPYs ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "I8085.h"
#include "I8085InstrInfo.h"
#include "I8085Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {
class I8085ExpandCopies : public MachineFunctionPass {
public:
  static char ID;
  I8085ExpandCopies() : MachineFunctionPass(ID) {
    initializeI8085ExpandCopiesPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
    const I8085InstrInfo &TII = *STI.getInstrInfo();
    bool Changed = false;

    for (auto &MBB : MF) {
      for (auto MI = MBB.begin(), ME = MBB.end(); MI != ME; ) {
        MachineInstr &Instr = *MI++;
        if (Instr.getOpcode() != TargetOpcode::COPY)
          continue;

        if (Instr.getNumOperands() < 2)
          continue;

        Register Dest = Instr.getOperand(0).getReg();
        Register Src = Instr.getOperand(1).getReg();

        if (!Dest || !Src)
          continue;

        if (Dest == Src) {
          Instr.eraseFromParent();
          Changed = true;
          continue;
        }

        TII.copyPhysReg(MBB, Instr.getIterator(), Instr.getDebugLoc(), Dest,
                        Src, Instr.getOperand(1).isKill());
        Instr.eraseFromParent();
        Changed = true;
      }
    }

    return Changed;
  }
};
} // end anonymous namespace

char I8085ExpandCopies::ID = 0;

INITIALIZE_PASS(I8085ExpandCopies, "i8085-expand-copies",
                "I8085 Expand physical COPY instructions", false, false)

FunctionPass *llvm::createI8085ExpandCopiesPass() {
  return new I8085ExpandCopies();
}
