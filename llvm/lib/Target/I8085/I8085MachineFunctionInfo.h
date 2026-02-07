//===-- I8085MachineFuctionInfo.h - I8085 machine function info -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares I8085-specific per-machine-function information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_I8085_MACHINE_FUNCTION_INFO_H
#define LLVM_I8085_MACHINE_FUNCTION_INFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

/// Contains I8085-specific information for each MachineFunction.
class I8085MachineFunctionInfo : public MachineFunctionInfo {
  /// Indicates if a register has been spilled by the register
  /// allocator.
  bool HasSpills;

  /// Indicates if there are any fixed size allocas present.
  /// Note that if there are only variable sized allocas this is set to false.
  bool HasAllocas;

  /// Indicates if arguments passed using the stack are being
  /// used inside the function.
  bool HasStackArgs;

  /// Whether or not the function is an interrupt handler.
  bool IsInterruptHandler;

  /// Whether or not the function is an non-blocking interrupt handler.
  bool IsSignalHandler;

  /// Size of the callee-saved register portion of the
  /// stack frame in bytes.
  unsigned CalleeSavedFrameSize;

  /// FrameIndex for start of varargs area.
  int VarArgsFrameIndex;
  /// FrameIndex for per-function GR32 scratch storage (IAX/IBX bytes).
  int GR32ScratchFI;
  /// When only IBX is used (no IAX), the scratch slot is only 4 bytes
  /// and IBX is remapped to offset 0 instead of its usual offset 4.
  bool IBXRemappedToZero;
  /// FrameIndex for saved original SP when stack realignment is used.
  int StackRealignSaveFI;
  /// Indicates whether this function realigns the stack.
  bool HasStackRealign;

  public:
  I8085MachineFunctionInfo(const Function &F, const TargetSubtargetInfo *STI)
      : HasSpills(false), HasAllocas(false), HasStackArgs(false),
        CalleeSavedFrameSize(0), VarArgsFrameIndex(0), GR32ScratchFI(-1),
        IBXRemappedToZero(false), StackRealignSaveFI(-1),
        HasStackRealign(false) {
    CallingConv::ID CallConv = F.getCallingConv();

    this->IsInterruptHandler =
        CallConv == CallingConv::I8085_INTR || F.hasFnAttribute("interrupt");
    this->IsSignalHandler =
        CallConv == CallingConv::I8085_SIGNAL || F.hasFnAttribute("signal");
  }

  bool getHasSpills() const { return HasSpills; }
  void setHasSpills(bool B) { HasSpills = B; }

  bool getHasAllocas() const { return HasAllocas; }
  void setHasAllocas(bool B) { HasAllocas = B; }

  bool getHasStackArgs() const { return HasStackArgs; }
  void setHasStackArgs(bool B) { HasStackArgs = B; }

  /// Checks if the function is some form of interrupt service routine.
  bool isInterruptOrSignalHandler() const {
    return isInterruptHandler() || isSignalHandler();
  }

  bool isInterruptHandler() const { return IsInterruptHandler; }
  bool isSignalHandler() const { return IsSignalHandler; }

  unsigned getCalleeSavedFrameSize() const { return CalleeSavedFrameSize; }
  void setCalleeSavedFrameSize(unsigned Bytes) { CalleeSavedFrameSize = Bytes; }

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int Idx) { VarArgsFrameIndex = Idx; }

  int getGR32ScratchFI() const { return GR32ScratchFI; }
  void setGR32ScratchFI(int Idx) { GR32ScratchFI = Idx; }
  bool hasGR32ScratchFI() const { return GR32ScratchFI >= 0; }

  bool isIBXRemappedToZero() const { return IBXRemappedToZero; }
  void setIBXRemappedToZero(bool B) { IBXRemappedToZero = B; }

  int getStackRealignSaveFI() const { return StackRealignSaveFI; }
  void setStackRealignSaveFI(int Idx) { StackRealignSaveFI = Idx; }
  bool hasStackRealignSaveFI() const { return StackRealignSaveFI >= 0; }

  bool hasStackRealign() const { return HasStackRealign; }
  void setHasStackRealign(bool B) { HasStackRealign = B; }
};

} // namespace llvm

#endif // LLVM_I8085_MACHINE_FUNCTION_INFO_H
