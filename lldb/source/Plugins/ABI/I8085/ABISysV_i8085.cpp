//===-- ABISysV_i8085.cpp ----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABISysV_i8085.h"

#include "lldb/Core/Module.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/Value.h"
#include "lldb/Core/ValueObjectConstResult.h"
#include "lldb/Core/ValueObjectMemory.h"
#include "lldb/Core/ValueObjectRegister.h"
#include "lldb/Symbol/UnwindPlan.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/RegisterValue.h"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/TargetParser/Triple.h"

using namespace lldb;
using namespace lldb_private;

LLDB_PLUGIN_DEFINE_ADV(ABISysV_i8085, ABII8085)

// DWARF register numbers from I8085RegisterInfo.td
enum dwarf_regnums {
  dwarf_B = 0,
  dwarf_C = 1,
  dwarf_D = 2,
  dwarf_E = 3,
  dwarf_H = 4,
  dwarf_L = 5,
  dwarf_M = 6,
  dwarf_A = 7,
  dwarf_SP = 8,
  dwarf_FLAGS = 9,
  dwarf_PSW = 10,
  dwarf_PC = 11,
  dwarf_BC = 12,
  dwarf_DE = 13,
  dwarf_HL = 14,
};

// Register info array: 15 entries matching DWARF numbers.
// byte_offset is cumulative based on register sizes:
//   B(0,1), C(1,1), D(2,1), E(3,1), H(4,1), L(5,1), M(6,1), A(7,1),
//   SP(8,2), FLAGS(10,1), PSW(11,2), PC(13,2), BC(15,2), DE(17,2), HL(19,2)
static const RegisterInfo g_register_infos[] = {
    {"B",
     nullptr,
     1,
     0,
     eEncodingUint,
     eFormatHex,
     {dwarf_B, dwarf_B, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,
      LLDB_INVALID_REGNUM},
     nullptr,
     nullptr,
     nullptr,
    },
    {"C",
     nullptr,
     1,
     1,
     eEncodingUint,
     eFormatHex,
     {dwarf_C, dwarf_C, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,
      LLDB_INVALID_REGNUM},
     nullptr,
     nullptr,
     nullptr,
    },
    {"D",
     nullptr,
     1,
     2,
     eEncodingUint,
     eFormatHex,
     {dwarf_D, dwarf_D, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,
      LLDB_INVALID_REGNUM},
     nullptr,
     nullptr,
     nullptr,
    },
    {"E",
     nullptr,
     1,
     3,
     eEncodingUint,
     eFormatHex,
     {dwarf_E, dwarf_E, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,
      LLDB_INVALID_REGNUM},
     nullptr,
     nullptr,
     nullptr,
    },
    {"H",
     nullptr,
     1,
     4,
     eEncodingUint,
     eFormatHex,
     {dwarf_H, dwarf_H, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,
      LLDB_INVALID_REGNUM},
     nullptr,
     nullptr,
     nullptr,
    },
    {"L",
     nullptr,
     1,
     5,
     eEncodingUint,
     eFormatHex,
     {dwarf_L, dwarf_L, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,
      LLDB_INVALID_REGNUM},
     nullptr,
     nullptr,
     nullptr,
    },
    {"M",
     nullptr,
     1,
     6,
     eEncodingUint,
     eFormatHex,
     {dwarf_M, dwarf_M, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,
      LLDB_INVALID_REGNUM},
     nullptr,
     nullptr,
     nullptr,
    },
    {"A",
     nullptr,
     1,
     7,
     eEncodingUint,
     eFormatHex,
     {dwarf_A, dwarf_A, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,
      LLDB_INVALID_REGNUM},
     nullptr,
     nullptr,
     nullptr,
    },
    {"SP",
     "sp",
     2,
     8,
     eEncodingUint,
     eFormatHex,
     {dwarf_SP, dwarf_SP, LLDB_REGNUM_GENERIC_SP, LLDB_INVALID_REGNUM,
      LLDB_INVALID_REGNUM},
     nullptr,
     nullptr,
     nullptr,
    },
    {"FLAGS",
     nullptr,
     1,
     10,
     eEncodingUint,
     eFormatHex,
     {dwarf_FLAGS, dwarf_FLAGS, LLDB_REGNUM_GENERIC_FLAGS, LLDB_INVALID_REGNUM,
      LLDB_INVALID_REGNUM},
     nullptr,
     nullptr,
     nullptr,
    },
    {"PSW",
     nullptr,
     2,
     11,
     eEncodingUint,
     eFormatHex,
     {dwarf_PSW, dwarf_PSW, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,
      LLDB_INVALID_REGNUM},
     nullptr,
     nullptr,
     nullptr,
    },
    {"PC",
     "pc",
     2,
     13,
     eEncodingUint,
     eFormatHex,
     {dwarf_PC, dwarf_PC, LLDB_REGNUM_GENERIC_PC, LLDB_INVALID_REGNUM,
      LLDB_INVALID_REGNUM},
     nullptr,
     nullptr,
     nullptr,
    },
    {"BC",
     nullptr,
     2,
     15,
     eEncodingUint,
     eFormatHex,
     {dwarf_BC, dwarf_BC, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,
      LLDB_INVALID_REGNUM},
     nullptr,
     nullptr,
     nullptr,
    },
    {"DE",
     nullptr,
     2,
     17,
     eEncodingUint,
     eFormatHex,
     {dwarf_DE, dwarf_DE, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,
      LLDB_INVALID_REGNUM},
     nullptr,
     nullptr,
     nullptr,
    },
    {"HL",
     nullptr,
     2,
     19,
     eEncodingUint,
     eFormatHex,
     {dwarf_HL, dwarf_HL, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,
      LLDB_INVALID_REGNUM},
     nullptr,
     nullptr,
     nullptr,
    },
};

static const uint32_t k_num_register_infos =
    sizeof(g_register_infos) / sizeof(RegisterInfo);

const lldb_private::RegisterInfo *
ABISysV_i8085::GetRegisterInfoArray(uint32_t &count) {
  count = k_num_register_infos;
  return g_register_infos;
}

size_t ABISysV_i8085::GetRedZoneSize() const { return 0; }

//------------------------------------------------------------------
// Static Functions
//------------------------------------------------------------------

ABISP
ABISysV_i8085::CreateInstance(lldb::ProcessSP process_sp,
                              const ArchSpec &arch) {
  if (arch.GetTriple().getArch() == llvm::Triple::i8085) {
    return ABISP(
        new ABISysV_i8085(std::move(process_sp), MakeMCRegisterInfo(arch)));
  }
  return ABISP();
}

bool ABISysV_i8085::PrepareTrivialCall(Thread &thread, lldb::addr_t sp,
                                       lldb::addr_t pc, lldb::addr_t ra,
                                       llvm::ArrayRef<addr_t> args) const {
  // We don't support JIT or trivial call injection on i8085
  return false;
}

bool ABISysV_i8085::GetArgumentValues(Thread &thread,
                                      ValueList &values) const {
  return false;
}

Status ABISysV_i8085::SetReturnValueObject(lldb::StackFrameSP &frame_sp,
                                           lldb::ValueObjectSP &new_value_sp) {
  return Status();
}

ValueObjectSP ABISysV_i8085::GetReturnValueObjectImpl(
    Thread &thread, CompilerType &return_compiler_type) const {
  ValueObjectSP return_valobj_sp;
  return return_valobj_sp;
}

// Called when we are on the first instruction of a new function.
// i8085 CALL pushes a 16-bit return address onto the stack.
// At function entry: SP points to the return address.
// CFA = SP + 2 (the caller's SP before the CALL).
// PC is saved at [CFA - 2].
bool ABISysV_i8085::CreateFunctionEntryUnwindPlan(UnwindPlan &unwind_plan) {
  unwind_plan.Clear();
  unwind_plan.SetRegisterKind(eRegisterKindDWARF);

  uint32_t sp_reg_num = dwarf_SP;
  uint32_t pc_reg_num = dwarf_PC;

  UnwindPlan::RowSP row(new UnwindPlan::Row);
  row->GetCFAValue().SetIsRegisterPlusOffset(sp_reg_num, 2);
  row->SetRegisterLocationToAtCFAPlusOffset(pc_reg_num, -2, true);
  row->SetRegisterLocationToIsCFAPlusOffset(sp_reg_num, 0, true);

  unwind_plan.AppendRow(row);
  unwind_plan.SetSourceName("i8085 at-func-entry default");
  unwind_plan.SetSourcedFromCompiler(eLazyBoolNo);
  return true;
}

bool ABISysV_i8085::CreateDefaultUnwindPlan(UnwindPlan &unwind_plan) {
  unwind_plan.Clear();
  unwind_plan.SetRegisterKind(eRegisterKindDWARF);

  uint32_t sp_reg_num = dwarf_SP;
  uint32_t pc_reg_num = dwarf_PC;

  UnwindPlan::RowSP row(new UnwindPlan::Row);
  row->GetCFAValue().SetIsRegisterPlusOffset(sp_reg_num, 2);
  row->SetRegisterLocationToAtCFAPlusOffset(pc_reg_num, -2, true);
  row->SetRegisterLocationToIsCFAPlusOffset(sp_reg_num, 0, true);

  unwind_plan.AppendRow(row);
  unwind_plan.SetSourceName("i8085 default unwind plan");
  unwind_plan.SetSourcedFromCompiler(eLazyBoolNo);
  unwind_plan.SetUnwindPlanValidAtAllInstructions(eLazyBoolNo);
  return true;
}

// i8085 has no callee-saved registers in our ABI -- all registers are volatile
bool ABISysV_i8085::RegisterIsVolatile(const RegisterInfo *reg_info) {
  return true;
}

bool ABISysV_i8085::RegisterIsCalleeSaved(const RegisterInfo *reg_info) {
  return false;
}

void ABISysV_i8085::Initialize(void) {
  PluginManager::RegisterPlugin(
      GetPluginNameStatic(), "System V ABI for i8085 targets", CreateInstance);
}

void ABISysV_i8085::Terminate(void) {
  PluginManager::UnregisterPlugin(CreateInstance);
}
