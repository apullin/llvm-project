//===- llvm/unittest/MC/MCExprTest.cpp ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/MCExpr.h"
#include "gtest/gtest.h"

using namespace llvm;

namespace {

TEST(MCSymbolRefExprTest, I8085VariantKindNames) {
  static constexpr struct {
    MCSymbolRefExpr::VariantKind Kind;
    StringLiteral Name;
  } Cases[] = {
      {MCSymbolRefExpr::VK_I8085_NONE, "none"},
      {MCSymbolRefExpr::VK_I8085_LO8, "lo8"},
      {MCSymbolRefExpr::VK_I8085_HI8, "hi8"},
      {MCSymbolRefExpr::VK_I8085_HLO8, "hlo8"},
      {MCSymbolRefExpr::VK_I8085_DIFF8, "diff8"},
      {MCSymbolRefExpr::VK_I8085_DIFF16, "diff16"},
      {MCSymbolRefExpr::VK_I8085_DIFF32, "diff32"},
      {MCSymbolRefExpr::VK_I8085_PM, "pm"},
  };

  for (const auto &Case : Cases)
    EXPECT_EQ(Case.Name, MCSymbolRefExpr::getVariantKindName(Case.Kind));
}

} // namespace
