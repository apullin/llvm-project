//===-- I8085MCInstLower.cpp - Convert I8085 MachineInstr to an MCInst --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains code to lower I8085 MachineInstrs to their corresponding
// MCInst records.
//
//===----------------------------------------------------------------------===//

#include "I8085MCInstLower.h"

#include "I8085InstrInfo.h"
#include "MCTargetDesc/I8085MCExpr.h"
#include "MCTargetDesc/I8085MCTargetDesc.h"

#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/IR/Mangler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Support/ErrorHandling.h"

namespace llvm {

MCOperand I8085MCInstLower::lowerSymbolOperand(const MachineOperand &MO,
                                             MCSymbol *Sym) const {
  unsigned char TF = MO.getTargetFlags();
  const MCExpr *Expr = MCSymbolRefExpr::create(Sym, Ctx);

  bool IsNegated = false;
  if (TF & I8085II::MO_NEG) {
    IsNegated = true;
  }

  if (!MO.isJTI() && MO.getOffset()) {
    Expr = MCBinaryExpr::createAdd(
        Expr, MCConstantExpr::create(MO.getOffset(), Ctx), Ctx);
  }

  bool IsFunction = MO.isGlobal() && isa<Function>(MO.getGlobal());

  if (TF & I8085II::MO_LO) {
    if (IsFunction) {
      // N.B. Should we use _GS fixups here to cope with >128k progmem?
      Expr = I8085MCExpr::create(I8085MCExpr::VK_I8085_PM_LO8, Expr, IsNegated, Ctx);
    } else {
      Expr = I8085MCExpr::create(I8085MCExpr::VK_I8085_LO8, Expr, IsNegated, Ctx);
    }
  } else if (TF & I8085II::MO_HI) {
    if (IsFunction) {
      // N.B. Should we use _GS fixups here to cope with >128k progmem?
      Expr = I8085MCExpr::create(I8085MCExpr::VK_I8085_PM_HI8, Expr, IsNegated, Ctx);
    } else {
      Expr = I8085MCExpr::create(I8085MCExpr::VK_I8085_HI8, Expr, IsNegated, Ctx);
    }
  } else if (TF != 0) {
    llvm_unreachable("Unknown target flag on symbol operand");
  }

  return MCOperand::createExpr(Expr);
}

void I8085MCInstLower::lowerInstruction(const MachineInstr &MI,
                                      MCInst &OutMI) const {
  auto lowerSingleOperand = [&](const MachineOperand &MO) -> MCOperand {
    switch (MO.getType()) {
    default:
      MI.print(errs());
      llvm_unreachable("unknown operand type");
    case MachineOperand::MO_Register:
      if (MO.isImplicit())
        return MCOperand();
      return MCOperand::createReg(MO.getReg());
    case MachineOperand::MO_Immediate:
      return MCOperand::createImm(MO.getImm());
    case MachineOperand::MO_GlobalAddress:
      return lowerSymbolOperand(MO, Printer.getSymbol(MO.getGlobal()));
    case MachineOperand::MO_ExternalSymbol:
      return lowerSymbolOperand(
          MO, Printer.GetExternalSymbolSymbol(MO.getSymbolName()));
    case MachineOperand::MO_MachineBasicBlock: {
      return MCOperand::createExpr(
          MCSymbolRefExpr::create(MO.getMBB()->getSymbol(), Ctx));
    }
    case MachineOperand::MO_BlockAddress:
      return lowerSymbolOperand(
          MO, Printer.GetBlockAddressSymbol(MO.getBlockAddress()));
    case MachineOperand::MO_JumpTableIndex:
      return lowerSymbolOperand(MO, Printer.GetJTISymbol(MO.getIndex()));
    case MachineOperand::MO_ConstantPoolIndex:
      return lowerSymbolOperand(MO, Printer.GetCPISymbol(MO.getIndex()));
    case MachineOperand::MO_RegisterMask:
      return MCOperand();
    }
  };

  if (MI.getOpcode() == I8085::TCRETURN) {
    OutMI.setOpcode(I8085::JMP);
    MCOperand MCOp = lowerSingleOperand(MI.getOperand(0));
    if (MCOp.isValid())
      OutMI.addOperand(MCOp);
    return;
  }

  OutMI.setOpcode(MI.getOpcode());

  for (MachineOperand const &MO : MI.operands()) {
    MCOperand MCOp;

    MCOp = lowerSingleOperand(MO);
    if (!MCOp.isValid())
      continue;

    OutMI.addOperand(MCOp);
  }
}

} // end of namespace llvm
