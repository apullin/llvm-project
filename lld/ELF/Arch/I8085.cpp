//===- I8085.cpp ---------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Symbols.h"
#include "Target.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {
class I8085 final : public TargetInfo {
public:
  I8085();
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};
} // namespace

I8085::I8085() {
  trapInstr = {0x00, 0x00, 0x00, 0x00};
}

RelExpr I8085::getRelExpr(RelType type, const Symbol &,
                          const uint8_t *) const {
  switch (type) {
  case R_I8085_16:
    return R_ABS;
  default:
    return R_ABS;
  }
}

void I8085::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_I8085_NONE:
    return;
  case R_I8085_16:
    checkIntUInt(loc, val, 16, rel);
    write16le(loc, val);
    return;
  default:
    error(getErrorLocation(loc) + "unrecognized relocation " +
          toString(rel.type));
  }
}

TargetInfo *elf::getI8085TargetInfo() {
  static I8085 target;
  return &target;
}
