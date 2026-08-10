//===--- TMS9900.h - Declare TMS9900 target feature support -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares TMS9900 TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_TMS9900_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_TMS9900_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY TMS9900TargetInfo : public TargetInfo {
  static const char *const GCCRegNames[];

public:
  TMS9900TargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    // TMS9900 has no TLS support
    TLSSupported = false;

    // Basic integer types
    IntWidth = 16;
    IntAlign = 16;
    LongWidth = 32;
    LongLongWidth = 64;
    LongAlign = LongLongAlign = 16;

    // Floating point (software emulation)
    FloatWidth = 32;
    FloatAlign = 16;
    DoubleWidth = LongDoubleWidth = 64;
    DoubleAlign = LongDoubleAlign = 16;

    // Pointers are 16-bit
    PointerWidth = 16;
    PointerAlign = 16;
    SuitableAlign = 16;
    // Match bare __attribute__((aligned)) to the strongest alignment required
    // by any C scalar type instead of inheriting Clang's generic 16-byte
    // default. The backend's four-byte stack boundary is a separate internal
    // legalization invariant, not a stronger C object-alignment guarantee.
    DefaultAlignForAttributeAligned = 16;

    // Type definitions
    SizeType = UnsignedInt;
    IntMaxType = SignedLongLong;
    IntPtrType = SignedInt;
    PtrDiffType = SignedInt;
    // unsigned int is only 16 bits, so it cannot be char32_t's underlying
    // type. unsigned long is the target's 32-bit unsigned integer type.
    Char32Type = UnsignedLong;
    // A single 16-bit access is indivisible with respect to interrupt entry.
    // This also matches picolibc's sig_atomic_t typedef.
    SigAtomicType = SignedInt;

    // TMS9900 is big-endian
    // Data layout must match the LLVM backend (TMS9900TargetMachine.cpp)
    // E = big endian, p:16:16 = 16-bit pointers
    // i8:8:8 = 8-bit ints with 8-bit alignment
    // i16:16:16 = 16-bit ints with 16-bit alignment
    // i32:16:32 = 32-bit ints with 16-bit ABI/32-bit preferred alignment
    // i64/f32/f64 use the 16-bit scalar ABI alignment declared above
    // n16 = native integer width is 16 bits
    // S32 = 32-bit stack alignment (required: LLVM's type legalizer uses
    //   OR-instead-of-ADD for i32 split address computation, which needs
    //   4-byte-aligned base addresses)
    resetDataLayout("E-p:16:16-i8:8:8-i16:16:16-i32:16:32-i64:16:16-"
                    "f32:16:16-f64:16:16-n16-S32");
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  ArrayRef<Builtin::Info> getTargetBuiltins() const override {
    return {};
  }

  bool allowsLargerPreferedTypeAlignment() const override { return false; }

  bool hasFeature(StringRef Feature) const override {
    return Feature == "tms9900";
  }

  ArrayRef<const char *> getGCCRegNames() const override;

  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override {
    // TMS9900 register aliases
    // R10 = SP (stack pointer), R11 = LR (link register)
    static const TargetInfo::GCCRegAlias GCCRegAliases[] = {
        {{"sp"}, "r10"},
        {{"lr"}, "r11"},
    };
    return llvm::ArrayRef(GCCRegAliases);
  }

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &info) const override {
    // No target-specific constraints for now
    return false;
  }

  std::string_view getClobbers() const override {
    return "";
  }

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::CharPtrBuiltinVaList;
  }
};

} // namespace targets
} // namespace clang
#endif // LLVM_CLANG_LIB_BASIC_TARGETS_TMS9900_H
