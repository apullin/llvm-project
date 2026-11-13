//===-- TMS9900Xas99Normalizer.h - In-memory xas99 pre-normalizer -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Isolates xas99 3.x source-level sugar (standalone labels before EQU/BES,
// column-0 directives, DORG dummy sections, s#sym byte sizes, b#/w# auto
// constant pools, WEQU, SAVE/BANK/XORG, and TI left-to-right / unsigned
// '//', '%%', '>>', '**' constant expressions) in a small in-memory prepass
// invoked from onBeginOfFile() ONLY when -tms9900-asm-dialect=xas99 is active.
// Keeps TMS9900AsmParser.cpp and the GNU/LLVM .S path completely clean.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_TMS9900_ASMPARSER_TMS9900XAS99NORMALIZER_H
#define LLVM_LIB_TARGET_TMS9900_ASMPARSER_TMS9900XAS99NORMALIZER_H

namespace llvm {
class MCAsmParser;

void normalizeXas99SourceBuffer(MCAsmParser &Parser);

} // namespace llvm

#endif
