//===-- TMS9900MCTargetDesc.h - TMS9900 Target Descriptions -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides TMS9900 specific target descriptions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_TMS9900_MCTARGETDESC_TMS9900MCTARGETDESC_H
#define LLVM_LIB_TARGET_TMS9900_MCTARGETDESC_TMS9900MCTARGETDESC_H

#include <memory>

namespace llvm {
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class MCTargetOptions;
class Target;

MCCodeEmitter *createTMS9900MCCodeEmitter(const MCInstrInfo &MCII,
                                          MCContext &Ctx);

MCAsmBackend *createTMS9900MCAsmBackend(const Target &T,
                                        const MCSubtargetInfo &STI,
                                        const MCRegisterInfo &MRI,
                                        const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter> createTMS9900ELFObjectWriter(uint8_t OSABI);

} // end namespace llvm

// Defines symbolic names for TMS9900 registers.
#define GET_REGINFO_ENUM
#include "TMS9900GenRegisterInfo.inc"

// Defines symbolic names for TMS9900 instructions.
#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "TMS9900GenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "TMS9900GenSubtargetInfo.inc"

#endif
