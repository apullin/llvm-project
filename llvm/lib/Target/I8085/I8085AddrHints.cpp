//===-- I8085AddrHints.cpp - Address register allocation hints ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Seed regalloc hints for address-bearing pseudos based on the post-regalloc
// expansion cost of their address operands.
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
class I8085AddrHints : public MachineFunctionPass {
public:
  static char ID;
  I8085AddrHints() : MachineFunctionPass(ID) {
    initializeI8085AddrHintsPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    MachineRegisterInfo &MRI = MF.getRegInfo();
    bool Changed = false;

    auto addHintIfLegal = [&](Register VReg, MCRegister Pref) {
      if (!VReg.isVirtual())
        return false;
      const TargetRegisterClass *RC = MRI.getRegClass(VReg);
      if (!RC || !RC->contains(Pref))
        return false;
      const auto &Hints = MRI.getRegAllocationHints(VReg);
      for (Register Existing : Hints.second)
        if (Existing == Pref)
          return false;
      MRI.addRegAllocationHint(VReg, Pref);
      return true;
    };

    for (auto &MBB : MF) {
      for (auto &MI : MBB) {
        int AddrIdx = -1;
        SmallVector<MCRegister, 2> Prefs;

        switch (MI.getOpcode()) {
        case I8085::LOAD_8_ADDR_CONTENT:
        case I8085::LOAD_16_ADDR_CONTENT:
          AddrIdx = 1;
          Prefs.push_back(I8085::HL);
          Prefs.push_back(I8085::BC);
          break;
        case I8085::LOAD_32_ADDR_CONTENT:
          AddrIdx = 1;
          Prefs.push_back(I8085::BC);
          Prefs.push_back(I8085::DE);
          Prefs.push_back(I8085::HL);
          break;
        case I8085::STORE_8_ADDR_CONTENT:
        case I8085::STORE_16_ADDR_CONTENT:
          AddrIdx = 0;
          Prefs.push_back(I8085::DE);
          Prefs.push_back(I8085::BC);
          Prefs.push_back(I8085::HL);
          break;
        case I8085::STORE_32_ADDR_CONTENT:
          AddrIdx = 0;
          if (MI.getOperand(AddrIdx).isKill()) {
            Prefs.push_back(I8085::HL);
            Prefs.push_back(I8085::DE);
            Prefs.push_back(I8085::BC);
          } else {
            Prefs.push_back(I8085::DE);
            Prefs.push_back(I8085::BC);
            Prefs.push_back(I8085::HL);
          }
          break;
        default:
          break;
        }

        if (AddrIdx < 0)
          continue;
        const MachineOperand &MO = MI.getOperand(AddrIdx);
        if (!MO.isReg())
          continue;
        Register AddrReg = MO.getReg();
        for (MCRegister Pref : Prefs)
          Changed |= addHintIfLegal(AddrReg, Pref);
      }
    }

    return Changed;
  }
};
} // end anonymous namespace

char I8085AddrHints::ID = 0;

INITIALIZE_PASS(I8085AddrHints, "i8085-addr-hints",
                "I8085 Address Allocation Hints", false, false)

FunctionPass *llvm::createI8085AddrHintsPass() { return new I8085AddrHints(); }
