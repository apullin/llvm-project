//===-- I8085StoreRegClass.cpp - Constrain store source regs -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "I8085.h"
#include "I8085InstrInfo.h"
#include "I8085Subtarget.h"
#include "MCTargetDesc/I8085MCTargetDesc.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

using namespace llvm;

namespace {
class I8085StoreRegClass : public MachineFunctionPass {
public:
  static char ID;
  I8085StoreRegClass() : MachineFunctionPass(ID) {
    initializeI8085StoreRegClassPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
    const I8085InstrInfo &TII = *STI.getInstrInfo();
    MachineRegisterInfo &MRI = MF.getRegInfo();
    bool Changed = false;

    for (auto &MBB : MF) {
      for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
        int SrcIdx = -1;
        const TargetRegisterClass *RC = nullptr;
        switch (MI->getOpcode()) {
        case I8085::STORE_8_ADDR_CONTENT:
          SrcIdx = 0;
          RC = &I8085::GR16BDRegClass;
          break;
        case I8085::STORE_16_ADDR_CONTENT:
          SrcIdx = 1;
          RC = &I8085::GR16BDRegClass;
          break;
        default:
          break;
        }

        if (SrcIdx < 0)
          continue;

        MachineOperand &MO = MI->getOperand(SrcIdx);
        if (!MO.isReg())
          continue;

        Register Src = MO.getReg();
        bool IsKill = MO.isKill();

        if (Src.isVirtual()) {
          if (!MRI.constrainRegClass(Src, RC)) {
            Register Tmp = MRI.createVirtualRegister(RC);
            BuildMI(MBB, MI, MI->getDebugLoc(), TII.get(TargetOpcode::COPY), Tmp)
                .addReg(Src, getKillRegState(IsKill));
            MO.setReg(Tmp);
            MO.setIsKill(true);
            Changed = true;
          }
        } else if (!RC->contains(Src)) {
          Register Tmp = MRI.createVirtualRegister(RC);
          BuildMI(MBB, MI, MI->getDebugLoc(), TII.get(TargetOpcode::COPY), Tmp)
              .addReg(Src, getKillRegState(IsKill));
          MO.setReg(Tmp);
          MO.setIsKill(true);
          Changed = true;
        }
      }
    }

    return Changed;
  }
};
} // end anonymous namespace

char I8085StoreRegClass::ID = 0;

INITIALIZE_PASS(I8085StoreRegClass, "i8085-store-regclass",
                "I8085 Store Register Class Fixup", false, false)

FunctionPass *llvm::createI8085StoreRegClassPass() {
  return new I8085StoreRegClass();
}
