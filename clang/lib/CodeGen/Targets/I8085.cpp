//===- I8085.cpp ---------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"

using namespace clang;
using namespace clang::CodeGen;

//===----------------------------------------------------------------------===//
// I8085 ABI Implementation
//===----------------------------------------------------------------------===//

namespace {

class I8085ABIInfo : public DefaultABIInfo {
public:
  I8085ABIInfo(CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

  ABIArgInfo classifyReturnType(QualType RetTy) const {
    // Avoid non-reentrant GR32 pseudo returns: return >32-bit integers via sret.
    if (RetTy->isIntegralOrEnumerationType() &&
        getContext().getTypeSize(RetTy) > 32) {
      return getNaturalAlignIndirect(RetTy, /*ByVal=*/false);
    }
    return DefaultABIInfo::classifyReturnType(RetTy);
  }

  ABIArgInfo classifyArgumentType(QualType Ty) const {
    return DefaultABIInfo::classifyArgumentType(Ty);
  }

  // DefaultABIInfo::classify{Return,Argument}Type() are not virtual.
  void computeInfo(CGFunctionInfo &FI) const override {
    if (!getCXXABI().classifyReturnType(FI))
      FI.getReturnInfo() = classifyReturnType(FI.getReturnType());
    for (auto &I : FI.arguments())
      I.info = classifyArgumentType(I.type);
  }
};

class I8085TargetCodeGenInfo : public TargetCodeGenInfo {
public:
  I8085TargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<I8085ABIInfo>(CGT)) {}
};

} // namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createI8085TargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<I8085TargetCodeGenInfo>(CGM.getTypes());
}
