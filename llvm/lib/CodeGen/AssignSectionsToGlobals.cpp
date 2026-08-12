//===- AssignSectionsToGlobals.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// For each global without an explicit section, set the default section computed
// by the backend. Also annotate globals with their input section so clang and
// LTO optimizations can preserve linker-script placement constraints.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/AssignSectionsToGlobals.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;

namespace {

void setStringAttribute(GlobalVariable &GV, StringRef Kind, StringRef Value) {
  if (GV.hasAttribute(Kind)) {
    AttrBuilder Attrs(GV.getContext(), GV.getAttributes());
    Attrs.removeAttribute(Kind);
    GV.setAttributes(AttributeSet::get(GV.getContext(), Attrs));
  }
  GV.addAttribute(Kind, Value);
}

void setStringAttribute(Function &F, StringRef Kind, StringRef Value) {
  if (F.hasFnAttribute(Kind))
    F.removeFnAttr(Kind);
  F.addFnAttr(Kind, Value);
}

void setInputSectionAttribute(GlobalObject &GO, StringRef Section) {
  if (auto *F = dyn_cast<Function>(&GO))
    setStringAttribute(*F, "linker_input_section", Section);
  else if (auto *GV = dyn_cast<GlobalVariable>(&GO))
    setStringAttribute(*GV, "linker_input_section", Section);
}

class AssignSections {
public:
  bool run(Module &M, TargetMachine *TM);

private:
  Module *M = nullptr;
  TargetMachine *TM = nullptr;

  StringRef getDefaultSectionNameForGlobal(const GlobalObject &GO);
};

} // namespace

StringRef
AssignSections::getDefaultSectionNameForGlobal(const GlobalObject &GO) {
  TargetLoweringObjectFile &TLOF = *TM->getObjFileLowering();
  SectionKind GVKind = TargetLoweringObjectFile::getKindForGlobal(&GO, *TM);

  // Section selection may depend on module flags such as small-data settings.
  TLOF.getModuleMetadata(*M);

  auto *Section =
      dyn_cast_or_null<MCSectionELF>(TLOF.SectionForGlobal(&GO, GVKind, *TM));
  return Section ? Section->getName() : StringRef();
}

bool AssignSections::run(Module &Mod, TargetMachine *TMIn) {
  M = &Mod;
  TM = TMIn;

  const Triple TT(Mod.getTargetTriple());
  if (!TM || !TT.isOSBinFormatELF())
    return false;

  if (Mod.alias_empty() && Mod.global_empty() && Mod.empty())
    return false;

  std::string TripleName = TT.str();
  const Target &T = TM->getTarget();
  std::unique_ptr<MCRegisterInfo> MRI(T.createMCRegInfo(TripleName));
  if (!MRI)
    return false;
  MCTargetOptions MCOptions;
  std::unique_ptr<MCAsmInfo> MAI(
      T.createMCAsmInfo(*MRI, TripleName, MCOptions));
  if (!MAI)
    return false;
  std::unique_ptr<MCSubtargetInfo> STI(
      T.createMCSubtargetInfo(TripleName, "", ""));
  if (!STI)
    return false;

  MCObjectFileInfo MOFI;
  MCContext MCCtx(TT, MAI.get(), MRI.get(), STI.get());
  MCCtx.setObjectFileInfo(&MOFI);
  MOFI.initMCObjectFileInfo(MCCtx, /*PIC=*/false);
  TM->getObjFileLowering()->Initialize(MCCtx, *TM);

  bool Changed = false;
  for (GlobalObject &GO : M->global_objects()) {
    if (GO.isDeclarationForLinker() ||
        GO.hasCommonLinkage() || GO.getName().starts_with("llvm."))
      continue;

    StringRef Section =
        GO.hasSection() ? GO.getSection() : getDefaultSectionNameForGlobal(GO);
    if (Section.empty())
      continue;
    if (!GO.hasSection()) {
      GO.setSection(Section);
      Changed = true;
    }
    setInputSectionAttribute(GO, Section);
    Changed = true;
  }

  return Changed;
}

PreservedAnalyses AssignSectionsToGlobalsPass::run(
    Module &M, ModuleAnalysisManager &MAM) {
  AssignSections Pass;
  return Pass.run(M, TM) ? PreservedAnalyses::none()
                         : PreservedAnalyses::all();
}
