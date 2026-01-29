//===-- I8085ELFObjectWriter.cpp - I8085 ELF Writer ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/I8085FixupKinds.h"
#include "MCTargetDesc/I8085MCExpr.h"
#include "MCTargetDesc/I8085MCTargetDesc.h"

#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"

namespace llvm {

/// Writes I8085 machine code into an ELF32 object file.
class I8085ELFObjectWriter : public MCELFObjectTargetWriter {
public:
  I8085ELFObjectWriter(uint8_t OSABI);

  virtual ~I8085ELFObjectWriter() = default;

  unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                        const MCFixup &Fixup, bool IsPCRel) const override;
};

I8085ELFObjectWriter::I8085ELFObjectWriter(uint8_t OSABI)
    : MCELFObjectTargetWriter(false, OSABI, ELF::EM_I8085, true) {}

unsigned I8085ELFObjectWriter::getRelocType(MCContext &Ctx, const MCValue &Target,
                                          const MCFixup &Fixup,
                                          bool IsPCRel) const {
  (void)Target;
  switch (Fixup.getTargetKind()) {
  case FK_Data_1:
    return ELF::R_I8085_8;
  case FK_Data_2:
  case FK_Data_4:
  case FK_Data_8:
    // I8085 uses 16-bit addresses; map wider data fixups to 16-bit relocs.
    return ELF::R_I8085_16;
  case I8085::fixup_16:
    return ELF::R_I8085_16;
  case I8085::fixup_lo8:
    return ELF::R_I8085_LO8;
  case I8085::fixup_hi8:
    return ELF::R_I8085_HI8;
  case I8085::fixup_hh8:
    return ELF::R_I8085_HH8;
  case I8085::fixup_hhi8:
    return ELF::R_I8085_HHI8;
  case I8085::fixup_pm_lo8:
    return ELF::R_I8085_PM_LO8;
  case I8085::fixup_pm_hi8:
    return ELF::R_I8085_PM_HI8;
  case I8085::fixup_pm_hh8:
    return ELF::R_I8085_PM_HH8;
  case I8085::fixup_pm:
    return ELF::R_I8085_PM;
  case I8085::fixup_lo8_gs:
    return ELF::R_I8085_LO8_GS;
  case I8085::fixup_hi8_gs:
    return ELF::R_I8085_HI8_GS;
  case I8085::fixup_gs:
    return ELF::R_I8085_GS;
  default:
    llvm_unreachable("invalid fixup kind!");
  }
}

std::unique_ptr<MCObjectTargetWriter> createI8085ELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<I8085ELFObjectWriter>(OSABI);
}

} // end of namespace llvm
