//===-- I8085SelectToBranch.cpp - Convert select+add to branch for i8085 ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass converts patterns like:
//   %sel = select i1 %cond, i32 0, i32 %Y
//   %result = add i32 %sel, %X
//
// into branch-based code:
//   br i1 %cond, label %skip, label %doadd
// doadd:
//   %addval = add i32 %Y, %X
//   br label %merge
// skip:
//   br label %merge
// merge:
//   %result = phi i32 [%X, %skip], [%addval, %doadd]
//
// On the 8085, this is critical because:
// - i32 operations are extremely expensive (~200 instructions)
// - The backend's SELECT_32 pseudo evaluates both arms eagerly
// - A branch can skip the expensive 32-bit ADD entirely
//
// This undoes an InstCombine transform that converts branch-based conditional
// addition into branchless select+add arithmetic.
//
//===----------------------------------------------------------------------===//

#include "I8085.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Pass.h"

using namespace llvm;
using namespace llvm::PatternMatch;

#define DEBUG_TYPE "i8085-select-to-branch"

namespace {

class I8085SelectToBranch : public FunctionPass {
public:
  static char ID;
  I8085SelectToBranch() : FunctionPass(ID) {
    initializeI8085SelectToBranchPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override {
    return "I8085 select-to-branch conversion";
  }

private:
  bool tryConvertSelectAdd(Instruction *I);
};

} // end anonymous namespace

char I8085SelectToBranch::ID = 0;

INITIALIZE_PASS(I8085SelectToBranch, DEBUG_TYPE,
                "I8085 select-to-branch conversion", false, false)

/// Try to convert an add(select(cond, 0, Y), X) pattern to a branch.
/// Returns true if the instruction was transformed.
bool I8085SelectToBranch::tryConvertSelectAdd(Instruction *AddInst) {
  // Match: add(select(cond, 0, Y), X) or add(X, select(cond, 0, Y))
  // Also: add(select(cond, Y, 0), X) or add(X, select(cond, Y, 0))
  if (AddInst->getOpcode() != Instruction::Add)
    return false;

  Type *Ty = AddInst->getType();
  if (!Ty->isIntegerTy())
    return false;

  // Only transform types larger than 16 bits (i32, i64).
  // On the 8085, operations on i8 and i16 are cheap enough that
  // branchless select+add is acceptable.
  unsigned BitWidth = Ty->getIntegerBitWidth();
  if (BitWidth <= 16)
    return false;

  // Try both operand orderings.
  for (unsigned SelIdx = 0; SelIdx < 2; ++SelIdx) {
    Value *SelOp = AddInst->getOperand(SelIdx);
    Value *OtherOp = AddInst->getOperand(1 - SelIdx);

    auto *Sel = dyn_cast<SelectInst>(SelOp);
    if (!Sel || !Sel->hasOneUse())
      continue;

    Value *Cond = Sel->getCondition();
    Value *TVal = Sel->getTrueValue();
    Value *FVal = Sel->getFalseValue();

    // Match select(cond, 0, Y) -> when cond is true, result = 0 + X = X
    //   (skip the add)
    // Match select(cond, Y, 0) -> when cond is false, result = 0 + X = X
    //   (skip the add)
    Value *NonZeroVal = nullptr;
    bool ZeroOnTrue = false;

    if (auto *C = dyn_cast<ConstantInt>(TVal)) {
      if (C->isZero()) {
        NonZeroVal = FVal;
        ZeroOnTrue = true;
      }
    }
    if (!NonZeroVal) {
      if (auto *C = dyn_cast<ConstantInt>(FVal)) {
        if (C->isZero()) {
          NonZeroVal = TVal;
          ZeroOnTrue = false;
        }
      }
    }

    if (!NonZeroVal)
      continue;

    // Found the pattern! Now split into a branch.
    //
    // Original:
    //   %sel = select i1 %cond, i32 0, i32 %Y    (ZeroOnTrue case)
    //   %result = add i32 %sel, %X
    //
    // Transformed:
    //   br i1 %cond, label %skipBB, label %addBB
    // addBB:
    //   %addval = add i32 %Y, %X
    //   br label %mergeBB
    // skipBB:
    //   br label %mergeBB
    // mergeBB:
    //   %result = phi i32 [%X, %skipBB], [%addval, %addBB]

    BasicBlock *OrigBB = AddInst->getParent();
    BasicBlock *MergeBB =
        OrigBB->splitBasicBlock(AddInst->getIterator(), "sel.merge");

    // Create the two new blocks.
    BasicBlock *AddBB =
        BasicBlock::Create(AddInst->getContext(), "sel.add", OrigBB->getParent(), MergeBB);
    BasicBlock *SkipBB =
        BasicBlock::Create(AddInst->getContext(), "sel.skip", OrigBB->getParent(), MergeBB);

    // Replace the unconditional branch that splitBasicBlock created with
    // a conditional branch.
    OrigBB->getTerminator()->eraseFromParent();

    IRBuilder<> Builder(OrigBB);
    if (ZeroOnTrue) {
      // cond==true -> skip (result = X), cond==false -> add
      Builder.CreateCondBr(Cond, SkipBB, AddBB);
    } else {
      // cond==true -> add, cond==false -> skip (result = X)
      Builder.CreateCondBr(Cond, AddBB, SkipBB);
    }

    // Build the add block.
    Builder.SetInsertPoint(AddBB);
    Value *AddVal = Builder.CreateAdd(NonZeroVal, OtherOp, "sel.addval",
                                      AddInst->hasNoUnsignedWrap(),
                                      AddInst->hasNoSignedWrap());
    Builder.CreateBr(MergeBB);

    // Build the skip block.
    Builder.SetInsertPoint(SkipBB);
    Builder.CreateBr(MergeBB);

    // Build the PHI in the merge block.
    Builder.SetInsertPoint(&MergeBB->front());
    PHINode *Phi = Builder.CreatePHI(Ty, 2, AddInst->getName());
    Phi->addIncoming(AddVal, AddBB);
    Phi->addIncoming(OtherOp, SkipBB);

    // Replace uses of the original add with the PHI.
    AddInst->replaceAllUsesWith(Phi);
    AddInst->eraseFromParent();
    Sel->eraseFromParent();

    return true;
  }

  return false;
}

bool I8085SelectToBranch::runOnFunction(Function &F) {
  bool Changed = false;

  // Collect instructions first to avoid iterator invalidation.
  SmallVector<Instruction *, 16> Worklist;
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (I.getOpcode() == Instruction::Add)
        Worklist.push_back(&I);
    }
  }

  for (Instruction *I : Worklist) {
    Changed |= tryConvertSelectAdd(I);
  }

  return Changed;
}

namespace llvm {

FunctionPass *createI8085SelectToBranchPass() {
  return new I8085SelectToBranch();
}

} // end namespace llvm
