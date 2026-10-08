//===-- I8085ForcedHints.cpp - Experimental forced allocation hints -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Apply exact, user-specified simple allocation hints to selected virtual
// registers in one function. This is an experimental bridge from offline
// solver results to the normal LLVM pipeline.
//
//===----------------------------------------------------------------------===//

#include "I8085.h"
#include "MCTargetDesc/I8085MCTargetDesc.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

static cl::opt<std::string> I8085ForcedHintFunc(
    "i8085-force-hint-func",
    cl::desc("Apply exact forced allocation hints only in this function"),
    cl::init(""));

static cl::opt<int> I8085ForcedHintBlock(
    "i8085-force-hint-block",
    cl::desc("Restrict forced allocation hints to one machine basic block number"),
    cl::init(-1));

static cl::opt<std::string> I8085ForcedHintList(
    "i8085-force-hints",
    cl::desc("Comma-separated vreg=physreg list, e.g. 12=BC,14=DE,29=HL"),
    cl::init(""));

namespace {
class I8085ForcedHints : public MachineFunctionPass {
public:
  static char ID;
  I8085ForcedHints() : MachineFunctionPass(ID) {
    initializeI8085ForcedHintsPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    if (I8085ForcedHintFunc.empty() || I8085ForcedHintList.empty())
      return false;
    if (MF.getName() != I8085ForcedHintFunc)
      return false;

    MachineRegisterInfo &MRI = MF.getRegInfo();
    SmallSet<Register, 16> BlockRegs;
    if (I8085ForcedHintBlock >= 0) {
      MachineBasicBlock *TargetMBB = nullptr;
      for (MachineBasicBlock &MBB : MF) {
        if (MBB.getNumber() == I8085ForcedHintBlock) {
          TargetMBB = &MBB;
          break;
        }
      }
      if (!TargetMBB)
        return false;
      for (MachineInstr &MI : *TargetMBB) {
        for (MachineOperand &MO : MI.operands()) {
          if (MO.isReg() && MO.getReg().isVirtual())
            BlockRegs.insert(MO.getReg());
        }
      }
    }

    auto parsePhysReg = [](StringRef Name) -> MCRegister {
      return StringSwitch<MCRegister>(Name)
          .Case("BC", I8085::BC)
          .Case("DE", I8085::DE)
          .Case("HL", I8085::HL)
          .Default(MCRegister());
    };

    bool Changed = false;
    for (StringRef Entry : split(I8085ForcedHintList, ',')) {
      Entry = Entry.trim();
      if (Entry.empty())
        continue;
      auto Parts = Entry.split('=');
      if (Parts.first.empty() || Parts.second.empty())
        continue;
      unsigned VRegId = 0;
      if (Parts.first.getAsInteger(10, VRegId))
        continue;
      Register VReg = Register::index2VirtReg(VRegId);
      if (!VReg.isVirtual() || VReg >= Register::index2VirtReg(MRI.getNumVirtRegs()))
        continue;
      if (I8085ForcedHintBlock >= 0 && !BlockRegs.contains(VReg))
        continue;
      MCRegister Phys = parsePhysReg(Parts.second.trim());
      if (!Phys)
        continue;
      const TargetRegisterClass *RC = MRI.getRegClass(VReg);
      if (!RC || !RC->contains(Phys))
        continue;
      MRI.setSimpleHint(VReg, Phys);
      Changed = true;
    }
    return Changed;
  }

private:
  static SmallVector<StringRef, 8> split(StringRef S, char Delim) {
    SmallVector<StringRef, 8> Parts;
    S.split(Parts, Delim, /*MaxSplit=*/-1, /*KeepEmpty=*/false);
    return Parts;
  }
};
} // end anonymous namespace

char I8085ForcedHints::ID = 0;

INITIALIZE_PASS(I8085ForcedHints, "i8085-forced-hints",
                "I8085 Experimental Forced Allocation Hints", false, false)

FunctionPass *llvm::createI8085ForcedHintsPass() {
  return new I8085ForcedHints();
}
