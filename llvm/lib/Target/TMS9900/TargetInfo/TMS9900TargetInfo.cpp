//===-- TMS9900TargetInfo.cpp - TMS9900 Target Implementation -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/TMS9900TargetInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

// First include the ENUM definitions so TMS9900::R11 etc are defined
#define GET_REGINFO_ENUM
#include "TMS9900GenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#include "TMS9900GenInstrInfo.inc"

// Then include the MC_DESC implementations
#define GET_INSTRINFO_MC_DESC
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "TMS9900GenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "TMS9900GenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "TMS9900GenRegisterInfo.inc"

Target &llvm::getTheTMS9900Target() {
  static Target TheTMS9900Target;
  return TheTMS9900Target;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeTMS9900TargetInfo() {
  RegisterTarget<Triple::tms9900, /*HasJIT=*/false> X(
      getTheTMS9900Target(), "tms9900", "TMS9900 [experimental]", "TMS9900");
}

namespace {
//===----------------------------------------------------------------------===//
// TMS9900MCAsmInfo - xas99-compatible assembly syntax
//
// This class configures LLVM's assembly output to be compatible with xas99,
// the cross-assembler from the xdt99 toolkit (https://github.com/endlos99/xdt99).
//
// xas99 uses TI Editor/Assembler syntax which differs from GNU as/ELF:
//   - Comments: * at start of line, or ; anywhere
//   - Directives: DATA, BYTE, TEXT, BSS, EQU, DEF, REF, EVEN, AORG, etc.
//   - No ELF directives: .type, .size, .section, .globl, .file, .p2align
//   - Labels: start in column 1, no colon required
//   - Hex: >XXXX format (though xas99 also accepts 0xXXXX)
//
// To use a different assembler in the future, create a new MCAsmInfo subclass.
//===----------------------------------------------------------------------===//
class TMS9900MCAsmInfo : public MCAsmInfo {
public:
  explicit TMS9900MCAsmInfo(const Triple &TT) {
    //=== Basic Properties ===//

    // Code pointer size is 16 bits (2 bytes)
    CodePointerSize = 2;
    CalleeSaveStackSlotSize = 2;

    // TMS9900 is big-endian
    IsLittleEndian = false;

    //=== xas99 Comment Syntax ===//
    // xas99 accepts ; for comments (also * at line start, but ; is safer)
    CommentString = ";";

    //=== xas99 Data Directives ===//
    // DATA for 16-bit words (xas99 native)
    Data16bitsDirective = "\tDATA ";
    // BYTE for 8-bit values
    Data8bitsDirective = "\tBYTE ";
    // No 32-bit directive - will emit as two DATA statements
    Data32bitsDirective = nullptr;

    // xas99 TEXT directive for strings (though we may not use it)
    AsciiDirective = "\tTEXT '";
    AscizDirective = nullptr;  // xas99 doesn't have null-terminated string directive

    //=== xas99 Symbol/Label Handling ===//
    // xas99 uses DEF to export symbols (like .globl)
    GlobalDirective = "\tDEF ";

    // xas99 doesn't support .type or .size directives (ELF-specific)
    HasDotTypeDotSizeDirective = false;

    // xas99 doesn't have .file directive
    HasSingleParameterDotFile = false;

    // xas99 doesn't support quoted symbol names
    SupportsQuotedNames = false;

    // Private labels (internal symbols) - use L prefix
    PrivateGlobalPrefix = "L";
    PrivateLabelPrefix = "L";

    //=== xas99 Alignment ===//
    // xas99 uses EVEN directive, not .align or .p2align
    // We'll use EVEN as the alignment directive
    AlignmentIsInBytes = true;
    // Disable function alignment directives
    HasFunctionAlignment = false;

    //=== xas99 Section Handling ===//
    // xas99 doesn't use ELF sections - code/data placement is via AORG/RORG
    UsesELFSectionDirectiveForBSS = false;

    //=== Assembler Integration ===//
    // We don't have an integrated assembler/parser for TMS9900.
    // This allows inline assembly to be emitted verbatim without parsing.
    UseIntegratedAssembler = false;

    // No debug info support yet
    SupportsDebugInformation = false;
  }

  //=== Override section directive handling ===//
  // xas99 doesn't understand .text/.data/.bss directives
  // We emit them as comments so they're informational but don't break assembly
  bool shouldOmitSectionDirective(StringRef SectionName) const override {
    // We want to suppress ALL section directives for xas99
    // Return false here, then printSwitchToSection will be called
    // but we need a custom streamer to really fix this...
    // For now, let's just say we always omit the directive (emit bare name)
    // and we'll post-process or fix in AsmPrinter
    return true;
  }
};

static MCAsmInfo *createTMS9900MCAsmInfo(const MCRegisterInfo &MRI,
                                          const Triple &TT,
                                          const MCTargetOptions &Options) {
  return new TMS9900MCAsmInfo(TT);
}

static MCInstrInfo *createTMS9900MCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitTMS9900MCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createTMS9900MCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitTMS9900MCRegisterInfo(X, TMS9900::R11);  // R11 is link register
  return X;
}

static MCSubtargetInfo *createTMS9900MCSubtargetInfo(const Triple &TT,
                                                      StringRef CPU,
                                                      StringRef FS) {
  return createTMS9900MCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, FS);
}

// Simple MCInstPrinter for TMS9900
class TMS9900InstPrinter : public MCInstPrinter {
public:
  TMS9900InstPrinter(const MCAsmInfo &MAI, const MCInstrInfo &MII,
                      const MCRegisterInfo &MRI)
      : MCInstPrinter(MAI, MII, MRI) {}

  void printInst(const MCInst *MI, uint64_t Address, StringRef Annot,
                 const MCSubtargetInfo &STI, raw_ostream &O) override {
    printInstruction(MI, Address, O);
    printAnnotation(O, Annot);
  }

  void printInstruction(const MCInst *MI, uint64_t Address, raw_ostream &O);
  static const char *getRegisterName(MCRegister Reg);
  bool printAliasInstr(const MCInst *MI, uint64_t Address, raw_ostream &OS);

  std::pair<const char *, uint64_t> getMnemonic(const MCInst *MI) override;

  void printOperand(const MCInst *MI, unsigned OpNo, raw_ostream &O);
  void printRegName(raw_ostream &OS, MCRegister Reg) const override {
    OS << getRegisterName(Reg);
  }
};

// Implement printOperand before including generated code
void TMS9900InstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                       raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    O << getRegisterName(Op.getReg());
  } else if (Op.isImm()) {
    O << Op.getImm();
  } else {
    assert(Op.isExpr() && "unknown operand kind in printOperand");
    Op.getExpr()->print(O, &MAI);
  }
}

// Include auto-generated printer
#define GET_INSTRUCTION_NAME
#define PRINT_ALIAS_INSTR
#include "TMS9900GenAsmWriter.inc"

static MCInstPrinter *createTMS9900MCInstPrinter(const Triple &T,
                                                  unsigned SyntaxVariant,
                                                  const MCAsmInfo &MAI,
                                                  const MCInstrInfo &MII,
                                                  const MCRegisterInfo &MRI) {
  return new TMS9900InstPrinter(MAI, MII, MRI);
}
} // end anonymous namespace

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeTMS9900TargetMC() {
  Target &T = getTheTMS9900Target();

  TargetRegistry::RegisterMCAsmInfo(T, createTMS9900MCAsmInfo);
  TargetRegistry::RegisterMCInstrInfo(T, createTMS9900MCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createTMS9900MCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createTMS9900MCSubtargetInfo);
  TargetRegistry::RegisterMCInstPrinter(T, createTMS9900MCInstPrinter);
}

