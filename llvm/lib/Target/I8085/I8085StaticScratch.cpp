//===-- I8085StaticScratch.cpp - Static scratch spill slots ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Redirect selected spill stack slots to private absolute RAM symbols.
//
// This is a conservative prototype for CP/M-style fixed workspace locations.
// It is gated behind -i8085-static-scratch-bytes=N and currently handles 8-bit
// and 16-bit spill slots.  The scratch pseudos preserve HL during late
// expansion when it is live, matching the safety contract of normal stack
// pseudos.
//
//===----------------------------------------------------------------------===//

#include "I8085.h"
#include "I8085InstrInfo.h"
#include "MCTargetDesc/I8085MCTargetDesc.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include <limits>
#include <tuple>

using namespace llvm;

#define I8085_STATIC_SCRATCH_NAME "I8085 static scratch spill pass"

static cl::opt<unsigned> I8085StaticScratchBytes(
    "i8085-static-scratch-bytes", cl::Hidden, cl::init(0),
    cl::desc("Maximum bytes of private static RAM to use per function for "
             "i8085 spill slots"));

static cl::opt<bool> I8085StaticScratchUnsafeRecursive(
    "i8085-static-scratch-unsafe-recursive", cl::Hidden, cl::init(false),
    cl::desc("Allow i8085 static scratch spill slots that are live across "
             "calls in functions not proven norecurse, excluding direct "
             "recursive SCCs"));

static cl::opt<bool> I8085StaticScratchAllowKnownRecursive(
    "i8085-static-scratch-allow-known-recursive", cl::Hidden, cl::init(false),
    cl::desc("Allow i8085 static scratch spill slots that are live across "
             "calls even in functions found in a direct recursive SCC"));

static cl::list<std::string> I8085StaticScratchAssumeNonReentrantFunctions(
    "i8085-static-scratch-assume-nonreentrant-function", cl::Hidden,
    cl::ZeroOrMore,
    cl::desc("Assume this function cannot be re-entered while active, allowing "
             "i8085 static scratch spill slots that are live across calls"));

static cl::list<std::string> I8085StaticScratchSkipFunctions(
    "i8085-static-scratch-skip-function", cl::Hidden, cl::ZeroOrMore,
    cl::desc("Do not use i8085 static scratch spill slots in this function"));

static cl::list<std::string> I8085StaticScratchOnlyFunctions(
    "i8085-static-scratch-only-function", cl::Hidden, cl::ZeroOrMore,
    cl::desc("Only use i8085 static scratch spill slots in this function"));

static cl::opt<int> I8085StaticScratchOnlyFrameIndex(
    "i8085-static-scratch-only-frame-index", cl::Hidden,
    cl::init(std::numeric_limits<int>::min()),
    cl::desc("Only use i8085 static scratch for this machine frame index"));

static cl::opt<bool> I8085StaticScratchDump(
    "i8085-static-scratch-dump", cl::Hidden, cl::init(false),
    cl::desc("Dump i8085 static scratch frame-object conversions"));

namespace {

struct SlotInfo {
  unsigned Size = 0;
  unsigned Accesses = 0;
  bool LiveAcrossCall = false;
  bool Supported = true;
  SmallVector<MachineInstr *, 4> Instrs;
};

class I8085StaticScratch : public MachineFunctionPass {
public:
  static char ID;

  I8085StaticScratch() : MachineFunctionPass(ID) {
    initializeI8085StaticScratchPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return I8085_STATIC_SCRATCH_NAME; }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  const TargetInstrInfo *TII = nullptr;
  DenseMap<const Function *, bool> DirectRecursiveSCC;

  bool isSupportedAccess(const MachineInstr &MI, int &FrameIndex) const;
  bool isInDirectRecursiveSCC(const Function &F);
  bool mayBeLiveAcrossCall(const MachineFunction &MF, int FrameIndex) const;
  bool hasNonReentrantContract(const MachineFunction &MF) const;
  void dumpConversion(const MachineFunction &MF, int FrameIndex,
                      const SlotInfo &Info, const GlobalVariable &GV) const;
  GlobalVariable *createScratchGlobal(MachineFunction &MF, int FrameIndex,
                                      unsigned Size) const;
  bool rewriteAccess(MachineInstr &MI, GlobalVariable *GV) const;
};

} // end anonymous namespace

char I8085StaticScratch::ID = 0;

INITIALIZE_PASS(I8085StaticScratch, "i8085-static-scratch",
                "I8085 Static Scratch Spill Slots", false, false)

bool I8085StaticScratch::isSupportedAccess(const MachineInstr &MI,
                                           int &FrameIndex) const {
  switch (MI.getOpcode()) {
  case I8085::LOAD_8_WITH_ADDR:
  case I8085::LOAD_16_WITH_ADDR:
    if (MI.getOperand(1).isFI() && MI.getOperand(2).isImm() &&
        MI.getOperand(2).getImm() == 0 &&
        MI.getOperand(0).getReg() != I8085::SP) {
      FrameIndex = MI.getOperand(1).getIndex();
      return true;
    }
    return false;
  case I8085::STORE_8:
  case I8085::STORE_16:
    if (MI.getOperand(0).isFI() && MI.getOperand(1).isImm() &&
        MI.getOperand(1).getImm() == 0 &&
        MI.getOperand(2).getReg() != I8085::SP) {
      FrameIndex = MI.getOperand(0).getIndex();
      return true;
    }
    return false;
  default:
    return false;
  }
}

static const Function *getDirectCallee(const Instruction &I) {
  const auto *CB = dyn_cast<CallBase>(&I);
  if (!CB)
    return nullptr;

  return dyn_cast<Function>(CB->getCalledOperand()->stripPointerCasts());
}

bool I8085StaticScratch::isInDirectRecursiveSCC(const Function &F) {
  auto It = DirectRecursiveSCC.find(&F);
  if (It != DirectRecursiveSCC.end())
    return It->second;

  SmallPtrSet<const Function *, 16> Visited;
  SmallVector<const Function *, 16> Worklist;

  for (const Instruction &I : instructions(F)) {
    const Function *Callee = getDirectCallee(I);
    if (!Callee || Callee->isDeclaration())
      continue;
    if (Callee == &F)
      return DirectRecursiveSCC[&F] = true;
    Worklist.push_back(Callee);
  }

  while (!Worklist.empty()) {
    const Function *Cur = Worklist.pop_back_val();
    if (!Visited.insert(Cur).second)
      continue;

    for (const Instruction &I : instructions(Cur)) {
      const Function *Callee = getDirectCallee(I);
      if (!Callee || Callee->isDeclaration())
        continue;
      if (Callee == &F)
        return DirectRecursiveSCC[&F] = true;
      Worklist.push_back(Callee);
    }
  }

  return DirectRecursiveSCC[&F] = false;
}

bool I8085StaticScratch::mayBeLiveAcrossCall(const MachineFunction &MF,
                                             int FrameIndex) const {
  DenseMap<const MachineBasicBlock *, bool> LiveIn;
  DenseMap<const MachineBasicBlock *, bool> LiveOut;
  SmallVector<const MachineBasicBlock *, 16> Blocks;

  for (const MachineBasicBlock &MBB : MF) {
    Blocks.push_back(&MBB);
    LiveIn[&MBB] = false;
    LiveOut[&MBB] = false;
  }

  bool Changed = true;
  while (Changed) {
    Changed = false;

    for (const MachineBasicBlock *MBB : llvm::reverse(Blocks)) {
      bool Out = false;
      for (const MachineBasicBlock *Succ : MBB->successors())
        Out |= LiveIn.lookup(Succ);

      bool Live = Out;
      for (const MachineInstr &MI : llvm::reverse(*MBB)) {
        if (Live && MI.isCall())
          return true;

        int FI = 0;
        if (isSupportedAccess(MI, FI) && FI == FrameIndex) {
          switch (MI.getOpcode()) {
          case I8085::STORE_8:
          case I8085::STORE_16:
            Live = false;
            break;
          case I8085::LOAD_8_WITH_ADDR:
          case I8085::LOAD_16_WITH_ADDR:
            Live = true;
            break;
          default:
            break;
          }
          continue;
        }

        for (const MachineOperand &MO : MI.operands()) {
          if (MO.isFI() && MO.getIndex() == FrameIndex)
            return true;
        }
      }

      if (LiveIn.lookup(MBB) != Live || LiveOut.lookup(MBB) != Out) {
        LiveIn[MBB] = Live;
        LiveOut[MBB] = Out;
        Changed = true;
      }
    }
  }

  return false;
}

bool I8085StaticScratch::hasNonReentrantContract(
    const MachineFunction &MF) const {
  return MF.getFunction().hasFnAttribute("i8085-nonreentrant") ||
         llvm::is_contained(I8085StaticScratchAssumeNonReentrantFunctions,
                            MF.getName());
}

void I8085StaticScratch::dumpConversion(const MachineFunction &MF,
                                        int FrameIndex, const SlotInfo &Info,
                                        const GlobalVariable &GV) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  errs() << "i8085-static-scratch: function=" << MF.getName()
         << " frame-index=" << FrameIndex << " size=" << Info.Size
         << " accesses=" << Info.Accesses
         << " offset=" << MFI.getObjectOffset(FrameIndex)
         << " spill=" << MFI.isSpillSlotObjectIndex(FrameIndex)
         << " aliased=" << MFI.isAliasedObjectIndex(FrameIndex)
         << " immutable=" << MFI.isImmutableObjectIndex(FrameIndex)
         << " stack-id=" << unsigned(MFI.getStackID(FrameIndex))
         << " has-calls=" << MFI.hasCalls()
         << " live-across-call=" << Info.LiveAcrossCall
         << " symbol=" << GV.getName() << " address=<linker-assigned>\n";
}

GlobalVariable *I8085StaticScratch::createScratchGlobal(MachineFunction &MF,
                                                        int FrameIndex,
                                                        unsigned Size) const {
  Module *M = const_cast<Module *>(MF.getFunction().getParent());
  LLVMContext &Ctx = M->getContext();
  auto *Ty = ArrayType::get(Type::getInt8Ty(Ctx), Size);
  auto *GV = new GlobalVariable(*M, Ty, false, GlobalValue::PrivateLinkage,
                                Constant::getNullValue(Ty),
                                (Twine("__i8085_static_scratch.") +
                                 MF.getName() + "." + Twine(FrameIndex))
                                    .str());
  GV->setAlignment(Align(1));
  return GV;
}

bool I8085StaticScratch::rewriteAccess(MachineInstr &MI,
                                       GlobalVariable *GV) const {
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();
  MachineBasicBlock::iterator InsertPt(MI);

  switch (MI.getOpcode()) {
  case I8085::LOAD_8_WITH_ADDR: {
    Register Dst = MI.getOperand(0).getReg();
    MachineInstrBuilder MIB =
        BuildMI(MBB, InsertPt, DL, TII->get(I8085::LOAD_8_STATIC_ADDR), Dst)
            .addGlobalAddress(GV);
    if (MI.getOperand(0).isDead())
      MIB->getOperand(0).setIsDead();
    MIB.cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }
  case I8085::LOAD_16_WITH_ADDR: {
    Register Dst = MI.getOperand(0).getReg();
    MachineInstrBuilder MIB =
        BuildMI(MBB, InsertPt, DL, TII->get(I8085::LOAD_16_STATIC_ADDR), Dst)
            .addGlobalAddress(GV);
    if (MI.getOperand(0).isDead())
      MIB->getOperand(0).setIsDead();
    MIB.cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }
  case I8085::STORE_8: {
    const MachineOperand &Src = MI.getOperand(2);
    MachineInstrBuilder MIB =
        BuildMI(MBB, InsertPt, DL, TII->get(I8085::STORE_8_STATIC_ADDR))
            .addGlobalAddress(GV)
            .addReg(Src.getReg(), getKillRegState(Src.isKill()));
    MIB.cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }
  case I8085::STORE_16: {
    const MachineOperand &Src = MI.getOperand(2);
    MachineInstrBuilder MIB =
        BuildMI(MBB, InsertPt, DL, TII->get(I8085::STORE_16_STATIC_ADDR))
            .addGlobalAddress(GV)
            .addReg(Src.getReg(), getKillRegState(Src.isKill()));
    MIB.cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }
  default:
    return false;
  }
}

bool I8085StaticScratch::runOnMachineFunction(MachineFunction &MF) {
  if (I8085StaticScratchBytes == 0)
    return false;
  if (!I8085StaticScratchOnlyFunctions.empty() &&
      !llvm::is_contained(I8085StaticScratchOnlyFunctions, MF.getName()))
    return false;
  if (llvm::is_contained(I8085StaticScratchSkipFunctions, MF.getName()))
    return false;
  if (MF.getTarget().getOptLevel() == CodeGenOptLevel::None)
    return false;

  const auto &ST = MF.getSubtarget();
  TII = ST.getInstrInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  DenseMap<int, SlotInfo> Slots;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      int FI = 0;
      if (isSupportedAccess(MI, FI)) {
        SlotInfo &Info = Slots[FI];
        Info.Accesses++;
        Info.Instrs.push_back(&MI);
        Info.Size = MFI.getObjectSize(FI);
        if (!MFI.isSpillSlotObjectIndex(FI) ||
            (Info.Size != 1 && Info.Size != 2))
          Info.Supported = false;
        continue;
      }

      for (const MachineOperand &MO : MI.operands()) {
        if (!MO.isFI())
          continue;
        int FI = MO.getIndex();
        if (MFI.isSpillSlotObjectIndex(FI))
          Slots[FI].Supported = false;
      }
    }
  }

  SmallVector<int, 16> Candidates;
  for (auto &KV : Slots) {
    int FI = KV.first;
    SlotInfo &Info = KV.second;
    if (!Info.Supported || Info.Accesses == 0)
      continue;
    Info.LiveAcrossCall = mayBeLiveAcrossCall(MF, FI);
    Candidates.push_back(FI);
  }

  llvm::sort(Candidates, [&](int A, int B) {
    const SlotInfo &LA = Slots[A];
    const SlotInfo &LB = Slots[B];
    return std::tie(LA.Accesses, A) > std::tie(LB.Accesses, B);
  });

  unsigned UsedBytes = 0;
  bool Changed = false;
  bool CheckedDirectRecursive = false;
  bool IsDirectRecursive = false;
  for (int FI : Candidates) {
    SlotInfo &Info = Slots[FI];
    if (I8085StaticScratchOnlyFrameIndex != std::numeric_limits<int>::min() &&
        FI != I8085StaticScratchOnlyFrameIndex)
      continue;
    if (Info.LiveAcrossCall && !MF.getFunction().doesNotRecurse()) {
      if (!I8085StaticScratchUnsafeRecursive && !hasNonReentrantContract(MF))
        continue;
      if (!I8085StaticScratchAllowKnownRecursive) {
        if (!CheckedDirectRecursive) {
          IsDirectRecursive = isInDirectRecursiveSCC(MF.getFunction());
          CheckedDirectRecursive = true;
        }
        if (IsDirectRecursive)
          continue;
      }
    }
    if (UsedBytes + Info.Size > I8085StaticScratchBytes)
      continue;

    GlobalVariable *GV = createScratchGlobal(MF, FI, Info.Size);
    if (I8085StaticScratchDump)
      dumpConversion(MF, FI, Info, *GV);
    for (MachineInstr *MI : Info.Instrs)
      Changed |= rewriteAccess(*MI, GV);
    MFI.RemoveStackObject(FI);
    UsedBytes += Info.Size;
  }

  return Changed;
}

FunctionPass *llvm::createI8085StaticScratchPass() {
  return new I8085StaticScratch();
}
