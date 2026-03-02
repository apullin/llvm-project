//===-- I8085CallingConv.td - Calling Conventions for I8085 ----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This describes the calling conventions for I8085 architecture.
// Normal functions use a special calling convention, solved in code.
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// I8085 Return Value Calling Convention
//===----------------------------------------------------------------------===//

// Special return value calling convention for runtime functions.
def RetCC_I8085_BUILTIN : CallingConv<[

  /*

      Goal - make them to return via stack

  */

  // CCIfType<[i8], CCAssignToStack<1, 1>>,
  // CCIfType<[i16], CCAssignToStack<2, 1>>

  CCIfType<[i8], CCAssignToReg<[A, C]>>,
  CCIfType<[i16], CCAssignToReg<[BC,DE]>>,
  CCIfType<[i32], CCAssignToReg<[IAX]>>,
  CCIfType<[i64], CCAssignToReg<[IAX,IBX]>>,
  CCIfType<[f32], CCAssignToReg<[IAX]>>,
  CCIfType<[f64], CCAssignToReg<[IAX,IBX]>>

  // CCIfType<[i8], CCAssignToReg<[R24, R25]>>,
  // CCIfType<[i16], CCAssignToReg<[R23R22, R25R24]>>
]>;

//===----------------------------------------------------------------------===//
// I8085 Argument Calling Conventions
//===----------------------------------------------------------------------===//

// The calling conventions are implemented in custom C++ code

// Calling convention for variadic functions.
def ArgCC_I8085_Vararg : CallingConv<[
  CCIfByVal<CCPassByVal<1, 1>>,
  // i8 are always passed through the stack with a byte slot and byte alignment.
  CCIfType<[i8], CCAssignToStack<1, 1>>,
  CCIfType<[i16], CCAssignToStack<2, 1>>,
  CCIfType<[i32], CCAssignToStack<4, 1>>,
  CCIfType<[i64], CCAssignToStack<8, 1>>,
  CCIfType<[f32], CCAssignToStack<4, 1>>,
  CCIfType<[f64], CCAssignToStack<8, 1>>
  // i16 are always passed through the stack with an alignment of 1.
]>;

// Fast calling convention for internal (static) functions.
// First i8 args go in C, E; first i16 args go in BC, DE; rest on stack.
// LLVM's GlobalOpt promotes internal functions to fastcc at -O2+.
def ArgCC_I8085_Fast : CallingConv<[
  CCIfType<[i8],  CCAssignToReg<[C, E]>>,
  CCIfType<[i16], CCAssignToReg<[BC, DE]>>,
  CCDelegateTo<ArgCC_I8085_Vararg>
]>;

//===----------------------------------------------------------------------===//
// Callee-saved register lists.
//===----------------------------------------------------------------------===//

def CSR_Normal : CalleeSavedRegs<(add)>;
