//===- TMS9900.cpp --------------------------------------------------------===//
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
// TMS9900 ABI Implementation
//===----------------------------------------------------------------------===//

namespace {

class TMS9900ABIInfo : public DefaultABIInfo {
  static ABIArgInfo complexArgInfo() {
    ABIArgInfo Info = ABIArgInfo::getDirect();
    Info.setCanBeFlattened(false);
    return Info;
  }

public:
  TMS9900ABIInfo(CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

  ABIArgInfo classifyReturnType(QualType RetTy) const {
    if (RetTy->isAnyComplexType())
      return complexArgInfo();

    return DefaultABIInfo::classifyReturnType(RetTy);
  }

  ABIArgInfo classifyArgumentType(QualType RetTy) const {
    if (RetTy->isAnyComplexType())
      return complexArgInfo();

    return DefaultABIInfo::classifyArgumentType(RetTy);
  }

  void computeInfo(CGFunctionInfo &FI) const override {
    if (!getCXXABI().classifyReturnType(FI))
      FI.getReturnInfo() = classifyReturnType(FI.getReturnType());
    for (auto &I : FI.arguments())
      I.info = classifyArgumentType(I.type);
  }

  Address EmitVAArg(CodeGenFunction &CGF, Address VAListAddr,
                    QualType Ty) const override {
    return EmitVAArgInstr(CGF, VAListAddr, Ty, classifyArgumentType(Ty));
  }
};

class TMS9900TargetCodeGenInfo : public TargetCodeGenInfo {
public:
  TMS9900TargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<TMS9900ABIInfo>(CGT)) {}

  void setTargetAttributes(const Decl *D, llvm::GlobalValue *GV,
                           CodeGen::CodeGenModule &M) const override;
};

} // namespace

void TMS9900TargetCodeGenInfo::setTargetAttributes(
    const Decl *D, llvm::GlobalValue *GV, CodeGen::CodeGenModule &M) const {
  // No target-specific attributes to set for now
}

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createTMS9900TargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<TMS9900TargetCodeGenInfo>(CGM.getTypes());
}
