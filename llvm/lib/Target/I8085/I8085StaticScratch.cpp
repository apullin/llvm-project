//===-- I8085StaticScratch.cpp - Static scratch spill slots ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Redirect selected 8-bit, 16-bit, and 32-bit spill stack slots to static RAM
// symbols in .bss.i8085.scratch.
//
// Supports three storage modes:
//   1. Per-function private globals (default with -i8085-static-scratch-bytes=N)
//   2. Canonical shared non-call-live virtual register pool (@__i8085_vreg_spill,
//      via -i8085-static-scratch-share or -i8085-scratchpad): all non-call-live
//      spills across the program share O(1) static RAM and emit identical symbol
//      references that MachineOutliner can deduplicate across functions.
//   3. Automatic closed-call-cone depth overlay (@__i8085_vreg_overlay_d<k>,
//      via -i8085-static-scratch-overlay or -i8085-scratchpad): functions whose
//      transitive call cone is exact, internal to the module, and acyclic get
//      call-live spills overlaid by call-DAG height (caller depth > callee depth,
//      so active frames never alias).
//
// Combined with -i8085-static-gr32 (enabled under -i8085-scratchpad), the 32-bit
// IAX/IBX pseudo-registers also reside in shared 8-byte .bss.i8085.scratch RAM.
// Interrupt/signal handlers are automatically excluded unless explicitly named.
//
//===----------------------------------------------------------------------===//

#include "I8085.h"
#include "I8085InstrInfo.h"
#include "I8085MachineFunctionInfo.h"
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
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include <algorithm>
#include <limits>
#include <tuple>

using namespace llvm;

#define I8085_STATIC_SCRATCH_NAME "I8085 static scratch spill pass"

static cl::opt<bool> I8085Scratchpad(
    "i8085-scratchpad", cl::Hidden, cl::init(false),
    cl::desc("Enable shared static GR32 registers, shared non-call-live spill "
             "pool, and closed-call-cone overlaid call-live spills for "
             "single-threaded i8085 firmware"));

static cl::opt<bool> I8085StaticGR32(
    "i8085-static-gr32", cl::Hidden, cl::init(false),
    cl::desc("Place 32-bit IAX/IBX pseudo-registers in shared 8-byte static "
             ".bss.i8085.scratch RAM instead of per-function stack frames"));

static cl::opt<unsigned> I8085StaticScratchBytes(
    "i8085-static-scratch-bytes", cl::Hidden, cl::init(0),
    cl::desc("Maximum bytes of static RAM to use per function for "
             "i8085 spill slots"));

static cl::opt<bool> I8085StaticScratchShare(
    "i8085-static-scratch-share", cl::Hidden, cl::init(false),
    cl::desc("Share one canonical static virtual-register pool across "
             "functions for non-call-live spill slots"));

static cl::opt<bool> I8085StaticScratchOverlay(
    "i8085-static-scratch-overlay", cl::Hidden, cl::init(false),
    cl::desc("Overlay call-live spill slots by call-DAG height for functions "
             "with a closed, acyclic module call cone"));

static cl::opt<bool> I8085StaticScratchI32(
    "i8085-static-scratch-i32", cl::Hidden, cl::init(true),
    cl::desc("Allow converting 32-bit (4-byte) spill slots to static scratch"));

static cl::opt<bool> I8085StaticScratchUnsafeRecursive(
    "i8085-static-scratch-unsafe-recursive", cl::Hidden, cl::init(false),
    cl::desc("Allow i8085 static scratch spill slots that are live across "
             "calls without an explicit nonreentrant contract, excluding "
             "direct recursive SCCs"));

static cl::opt<bool> I8085StaticScratchAssumeNonReentrant(
    "i8085-static-scratch-assume-nonreentrant", cl::Hidden, cl::init(false),
    cl::desc("Assume all optimized functions cannot be re-entered while "
             "active, allowing i8085 static scratch spill slots that are live "
             "across calls"));

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

bool llvm::isI8085StaticGR32Enabled() {
  return I8085StaticGR32 || I8085Scratchpad;
}

namespace {

static constexpr unsigned SharedPoolBytes = 32;

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
  DenseMap<const Function *, int> ClosedConeDepthCache;

  bool isSupportedAccess(const MachineInstr &MI, int &FrameIndex) const;
  bool isInDirectRecursiveSCC(const Function &F);
  int getClosedAcyclicCallConeDepth(const Function &F,
                                    SmallPtrSetImpl<const Function *> &Active);
  bool mayBeLiveAcrossCall(const MachineFunction &MF, int FrameIndex) const;
  bool hasNonReentrantContract(const MachineFunction &MF) const;
  void dumpConversion(const MachineFunction &MF, int FrameIndex,
                      const SlotInfo &Info, const GlobalVariable &GV,
                      int64_t Offset) const;
  GlobalVariable *createPrivateScratchGlobal(MachineFunction &MF,
                                             int FrameIndex,
                                             unsigned Size) const;
  GlobalVariable *getOrCreateSharedPool(Module &M, StringRef Name,
                                        unsigned PoolSize) const;
  bool rewriteAccess(MachineInstr &MI, GlobalVariable *GV,
                     int64_t Offset) const;
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
  case I8085::LOAD_32_WITH_ADDR:
    if (MI.getOperand(1).isFI() && MI.getOperand(2).isImm() &&
        MI.getOperand(2).getImm() == 0 &&
        MI.getOperand(0).getReg() != I8085::SP) {
      FrameIndex = MI.getOperand(1).getIndex();
      return true;
    }
    return false;
  case I8085::STORE_8:
  case I8085::STORE_16:
  case I8085::STORE_32:
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

// Compute the call-DAG height >= 0 if F's entire transitive call cone consists
// of exact definitions in this Module with no cycles and no indirect calls;
// return -1 otherwise. Note: compiler-emitted arithmetic/memory libcalls (such
// as __mul16, __udiv32, __addsf3, memcpy, memset) do not exist as IR CallBase
// instructions unless explicitly called in IR, and never call back into user C
// functions!
int I8085StaticScratch::getClosedAcyclicCallConeDepth(
    const Function &F, SmallPtrSetImpl<const Function *> &Active) {
  auto Cached = ClosedConeDepthCache.find(&F);
  if (Cached != ClosedConeDepthCache.end())
    return Cached->second;

  if (F.isDeclaration() || !F.hasExactDefinition())
    return -1;
  if (!Active.insert(&F).second)
    return -1; // Cycle detected!

  int MaxChildDepth = -1;
  for (const Instruction &I : instructions(F)) {
    const auto *CB = dyn_cast<CallBase>(&I);
    if (!CB)
      continue;
    if (CB->isInlineAsm())
      continue;
    const Function *Callee = getDirectCallee(I);
    if (!Callee) {
      Active.erase(&F);
      return ClosedConeDepthCache[&F] = -1; // Indirect call!
    }
    if (Callee->isIntrinsic())
      continue;
    // Allow pure SDK runtime helpers that never call back into user functions.
    StringRef Name = Callee->getName();
    if (Callee->isDeclaration() &&
        (Name == "memcpy" || Name == "memset" || Name == "memmove" ||
         Name == "memcmp" || Name == "memchr" || Name == "strlen" ||
         Name == "strcmp" || Name == "strncmp" || Name == "strchr" ||
         Name == "strrchr")) {
      MaxChildDepth = std::max(MaxChildDepth, 0);
      continue;
    }
    int ChildDepth = getClosedAcyclicCallConeDepth(*Callee, Active);
    if (ChildDepth < 0) {
      Active.erase(&F);
      return ClosedConeDepthCache[&F] = -1;
    }
    MaxChildDepth = std::max(MaxChildDepth, ChildDepth);
  }

  Active.erase(&F);
  int Depth = (MaxChildDepth < 0) ? 0 : (MaxChildDepth + 1);
  return ClosedConeDepthCache[&F] = Depth;
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
          case I8085::STORE_32:
            Live = false;
            break;
          case I8085::LOAD_8_WITH_ADDR:
          case I8085::LOAD_16_WITH_ADDR:
          case I8085::LOAD_32_WITH_ADDR:
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
  return I8085StaticScratchAssumeNonReentrant ||
         MF.getFunction().hasFnAttribute("i8085-nonreentrant") ||
         llvm::is_contained(I8085StaticScratchAssumeNonReentrantFunctions,
                            MF.getName());
}

void I8085StaticScratch::dumpConversion(const MachineFunction &MF,
                                        int FrameIndex, const SlotInfo &Info,
                                        const GlobalVariable &GV,
                                        int64_t Offset) const {
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
         << " symbol=" << GV.getName() << "+" << Offset
         << " address=<linker-assigned>\n";
}

GlobalVariable *I8085StaticScratch::createPrivateScratchGlobal(
    MachineFunction &MF, int FrameIndex, unsigned Size) const {
  Module *M = const_cast<Module *>(MF.getFunction().getParent());
  LLVMContext &Ctx = M->getContext();
  auto *Ty = ArrayType::get(Type::getInt8Ty(Ctx), Size);
  auto *GV = new GlobalVariable(*M, Ty, false, GlobalValue::PrivateLinkage,
                                Constant::getNullValue(Ty),
                                (Twine("__i8085_static_scratch.") +
                                 MF.getName() + "." + Twine(FrameIndex))
                                    .str());
  GV->setAlignment(Align(1));
  GV->setSection(".bss.i8085.scratch");
  return GV;
}

GlobalVariable *I8085StaticScratch::getOrCreateSharedPool(
    Module &M, StringRef Name, unsigned PoolSize) const {
  if (GlobalVariable *Existing = M.getNamedGlobal(Name))
    return Existing;
  LLVMContext &Ctx = M.getContext();
  auto *Ty = ArrayType::get(Type::getInt8Ty(Ctx), PoolSize);
  auto *GV = new GlobalVariable(M, Ty, false, GlobalValue::LinkOnceODRLinkage,
                                Constant::getNullValue(Ty), Name);
  GV->setVisibility(GlobalValue::HiddenVisibility);
  GV->setAlignment(Align(1));
  GV->setSection(".bss.i8085.scratch");
  return GV;
}

bool I8085StaticScratch::rewriteAccess(MachineInstr &MI, GlobalVariable *GV,
                                       int64_t Offset) const {
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();
  MachineBasicBlock::iterator InsertPt(MI);

  switch (MI.getOpcode()) {
  case I8085::LOAD_8_WITH_ADDR: {
    Register Dst = MI.getOperand(0).getReg();
    MachineInstrBuilder MIB =
        BuildMI(MBB, InsertPt, DL, TII->get(I8085::LOAD_8_STATIC_ADDR), Dst)
            .addGlobalAddress(GV, Offset);
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
            .addGlobalAddress(GV, Offset);
    if (MI.getOperand(0).isDead())
      MIB->getOperand(0).setIsDead();
    MIB.cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }
  case I8085::LOAD_32_WITH_ADDR: {
    Register Dst = MI.getOperand(0).getReg();
    MachineInstrBuilder MIB =
        BuildMI(MBB, InsertPt, DL, TII->get(I8085::LOAD_32_WITH_IMM_ADDR), Dst)
            .addGlobalAddress(GV, Offset);
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
            .addGlobalAddress(GV, Offset)
            .addReg(Src.getReg(), getKillRegState(Src.isKill()));
    MIB.cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }
  case I8085::STORE_16: {
    const MachineOperand &Src = MI.getOperand(2);
    MachineInstrBuilder MIB =
        BuildMI(MBB, InsertPt, DL, TII->get(I8085::STORE_16_STATIC_ADDR))
            .addGlobalAddress(GV, Offset)
            .addReg(Src.getReg(), getKillRegState(Src.isKill()));
    MIB.cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }
  case I8085::STORE_32: {
    const MachineOperand &Src = MI.getOperand(2);
    MachineInstrBuilder MIB =
        BuildMI(MBB, InsertPt, DL, TII->get(I8085::STORE_32_STATIC_ADDR))
            .addGlobalAddress(GV, Offset)
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
  const unsigned EffectiveBudget =
      (I8085StaticScratchBytes > 0)
          ? unsigned(I8085StaticScratchBytes)
          : (I8085Scratchpad ? 32u : 0u);
  if (EffectiveBudget == 0)
    return false;

  const bool ExplicitlySelected =
      !I8085StaticScratchOnlyFunctions.empty() &&
      llvm::is_contained(I8085StaticScratchOnlyFunctions, MF.getName());
  if (!I8085StaticScratchOnlyFunctions.empty() && !ExplicitlySelected)
    return false;
  if (llvm::is_contained(I8085StaticScratchSkipFunctions, MF.getName()))
    return false;
  if (MF.getTarget().getOptLevel() == CodeGenOptLevel::None)
    return false;

  auto *FuncInfo = MF.getInfo<I8085MachineFunctionInfo>();
  if (FuncInfo->isInterruptOrSignalHandler() && !ExplicitlySelected)
    return false;

  const bool ShareNonCallPool = I8085StaticScratchShare || I8085Scratchpad;
  const bool EnableOverlay = I8085StaticScratchOverlay || I8085Scratchpad;
  const bool AllowI32 = I8085StaticScratchI32;

  const auto &ST = MF.getSubtarget();
  TII = ST.getInstrInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  Module &Mod = *const_cast<Module *>(MF.getFunction().getParent());

  DenseMap<int, SlotInfo> Slots;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      int FI = 0;
      if (isSupportedAccess(MI, FI)) {
        SlotInfo &Info = Slots[FI];
        Info.Accesses++;
        Info.Instrs.push_back(&MI);
        Info.Size = MFI.getObjectSize(FI);
        const bool SizeOK =
            (Info.Size == 1 || Info.Size == 2 || (AllowI32 && Info.Size == 4));
        if (!MFI.isSpillSlotObjectIndex(FI) || !SizeOK)
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

  int ConeDepth = -2; // -2 = uncomputed
  auto getConeDepth = [&]() -> int {
    if (ConeDepth == -2) {
      SmallPtrSet<const Function *, 16> Active;
      ConeDepth = getClosedAcyclicCallConeDepth(MF.getFunction(), Active);
    }
    return ConeDepth;
  };

  unsigned UsedBytes = 0;
  unsigned SharedCursor = 0;
  unsigned OverlayCursor = 0;
  bool Changed = false;
  bool CheckedDirectRecursive = false;
  bool IsDirectRecursive = false;
  for (int FI : Candidates) {
    SlotInfo &Info = Slots[FI];
    if (I8085StaticScratchOnlyFrameIndex != std::numeric_limits<int>::min() &&
        FI != I8085StaticScratchOnlyFrameIndex)
      continue;
    if (UsedBytes + Info.Size > EffectiveBudget)
      continue;

    GlobalVariable *GV = nullptr;
    int64_t Offset = 0;

    if (Info.LiveAcrossCall) {
      const bool HasNonReentrantContract = hasNonReentrantContract(MF);
      int Depth = EnableOverlay ? getConeDepth() : -1;
      if (Depth > 0 && Depth <= 12 &&
          OverlayCursor + Info.Size <= SharedPoolBytes) {
        std::string PoolName =
            ("__i8085_vreg_overlay_d" + Twine(Depth)).str();
        GV = getOrCreateSharedPool(Mod, PoolName, SharedPoolBytes);
        Offset = OverlayCursor;
        OverlayCursor += Info.Size;
      } else {
        if (!HasNonReentrantContract && !I8085StaticScratchUnsafeRecursive)
          continue;
        if (!I8085StaticScratchAllowKnownRecursive) {
          if (!CheckedDirectRecursive) {
            IsDirectRecursive = isInDirectRecursiveSCC(MF.getFunction());
            CheckedDirectRecursive = true;
          }
          if (IsDirectRecursive)
            continue;
        }
        GV = createPrivateScratchGlobal(MF, FI, Info.Size);
      }
    } else if (ShareNonCallPool &&
               SharedCursor + Info.Size <= SharedPoolBytes) {
      GV = getOrCreateSharedPool(Mod, "__i8085_vreg_spill", SharedPoolBytes);
      Offset = SharedCursor;
      SharedCursor += Info.Size;
    } else {
      GV = createPrivateScratchGlobal(MF, FI, Info.Size);
    }

    if (I8085StaticScratchDump)
      dumpConversion(MF, FI, Info, *GV, Offset);
    for (MachineInstr *MI : Info.Instrs)
      Changed |= rewriteAccess(*MI, GV, Offset);
    MFI.RemoveStackObject(FI);
    UsedBytes += Info.Size;
  }

  // If every non-fixed stack object has been converted and removed, clear
  // HasSpills so leaf functions omit frame setup/teardown completely.
  if (Changed && !FuncInfo->hasStackRealign() && !FuncInfo->hasGR32ScratchFI()) {
    bool AnyLiveStackObj = false;
    for (int I = 0, E = MFI.getObjectIndexEnd(); I < E; ++I) {
      if (!MFI.isDeadObjectIndex(I)) {
        AnyLiveStackObj = true;
        break;
      }
    }
    if (!AnyLiveStackObj)
      FuncInfo->setHasSpills(false);
  }

  return Changed;
}

FunctionPass *llvm::createI8085StaticScratchPass() {
  return new I8085StaticScratch();
}
