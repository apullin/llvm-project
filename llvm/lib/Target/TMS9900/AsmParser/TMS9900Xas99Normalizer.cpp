//===-- TMS9900Xas99Normalizer.cpp - In-memory xas99 pre-normalizer -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TMS9900Xas99Normalizer.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdlib>
#include <string>

using namespace llvm;

namespace {

static bool isXasMnemonic(StringRef S) {
  return StringSwitch<bool>(S.upper())
      .Cases("A", "AB", "C", "CB", "S", "SB", "SOC", "SOCB", "SZC", "SZCB", true)
      .Cases("MOV", "MOVB", "COC", "CZC", "XOR", "MPY", "DIV", "XOP", true)
      .Cases("B", "BL", "BLWP", "CLR", "SETO", "INV", "NEG", "ABS", "SWPB", true)
      .Cases("INC", "INCT", "DEC", "DECT", "X", true)
      .Cases("JEQ", "JNE", "JGT", "JLT", "JH", "JL", "JHE", "JLE", true)
      .Cases("JOC", "JNC", "JNO", "JOP", "JMP", true)
      .Cases("SBO", "SBZ", "TB", "LDCR", "STCR", true)
      .Cases("SLA", "SRL", "SRA", "SRC", true)
      .Cases("LI", "AI", "ANDI", "ORI", "CI", "LWPI", "LIMI", "STST", "STWP", true)
      .Cases("RTWP", "IDLE", "RSET", "CKON", "CKOF", "LREX", "NOP", "RT", true)
      .Default(false);
}

static bool isXasDirective(StringRef S) {
  return StringSwitch<bool>(S.upper())
      .Cases("DATA", "BYTE", "TEXT", "STRI", "FLOA", "BSS", "BES", "EVEN", true)
      .Cases("DEF", "REF", "AORG", "RORG", "DORG", "XORG", "AUTO", true)
      .Cases("SAVE", "BANK", "EQU", "WEQU", "REQU", "END", "IDT", true)
      .Cases("COPY", "BCOPY", "DXOP", true)
      .Cases("PSEG", "PEND", "CSEG", "CEND", "DSEG", "DEND", true)
      .Cases("LOAD", "SREF", "UNL", "LIST", "PAGE", "TITL", true)
      .Default(false);
}

// Strip ';' or xas99 trailing comment (tab or >=2 spaces after operand field),
// respecting single- and double-quoted strings.
static std::pair<StringRef, StringRef> splitOperandsAndComment(StringRef Rest) {
  bool InSQ = false, InDQ = false;
  bool SeenNonSpace = false;
  unsigned SpaceRun = 0;
  for (size_t I = 0; I < Rest.size(); ++I) {
    char C = Rest[I];
    if (C == '\'' && !InDQ) {
      if (InSQ && I + 1 < Rest.size() && Rest[I + 1] == '\'') {
        ++I;
        continue;
      }
      InSQ = !InSQ;
      SeenNonSpace = true;
      SpaceRun = 0;
      continue;
    }
    if (C == '"' && !InSQ) {
      InDQ = !InDQ;
      SeenNonSpace = true;
      SpaceRun = 0;
      continue;
    }
    if (InSQ || InDQ)
      continue;
    if (C == ';')
      return {Rest.slice(0, I).rtrim(" \t"), Rest.substr(I)};
    if (C == '\t') {
      if (SeenNonSpace)
        return {Rest.slice(0, I).rtrim(" \t"), Rest.substr(I)};
    } else if (C == ' ') {
      if (SeenNonSpace) {
        ++SpaceRun;
        if (SpaceRun >= 2)
          return {Rest.slice(0, I - SpaceRun + 1).rtrim(" \t"),
                  Rest.substr(I - SpaceRun + 1)};
      }
    } else {
      SeenNonSpace = true;
      SpaceRun = 0;
    }
  }
  return {Rest.trim(" \t"), StringRef()};
}

// Split top-level comma-separated operands respecting quotes and parentheses.
static void splitCommaOperands(StringRef Ops, SmallVectorImpl<StringRef> &Out) {
  Out.clear();
  if (Ops.empty())
    return;
  bool InSQ = false, InDQ = false;
  int Depth = 0;
  size_t Start = 0;
  for (size_t I = 0; I < Ops.size(); ++I) {
    char C = Ops[I];
    if (C == '\'' && !InDQ) {
      if (InSQ && I + 1 < Ops.size() && Ops[I + 1] == '\'') {
        ++I;
        continue;
      }
      InSQ = !InSQ;
    } else if (C == '"' && !InSQ) {
      InDQ = !InDQ;
    } else if (!InSQ && !InDQ) {
      if (C == '(')
        ++Depth;
      else if (C == ')' && Depth > 0)
        --Depth;
      else if (C == ',' && Depth == 0) {
        Out.push_back(Ops.slice(Start, I).trim(" \t"));
        Start = I + 1;
      }
    }
  }
  Out.push_back(Ops.substr(Start).trim(" \t"));
}

// Evaluate constant TI/xas99 left-to-right expression if all terms are known
// constants/EQU values; returns true on exact evaluation.
static bool evalXasConstExpr(StringRef Expr,
                             const StringMap<int64_t> &KnownConsts,
                             int64_t &Result);

static bool parseXasPrimary(StringRef &S, const StringMap<int64_t> &KnownConsts,
                            int64_t &Val) {
  S = S.ltrim(" \t");
  if (S.empty())
    return false;
  if (S.consume_front("+"))
    return parseXasPrimary(S, KnownConsts, Val);
  if (S.consume_front("-")) {
    if (S.starts_with("!"))
      return false; // local label ref
    int64_t Sub = 0;
    if (!parseXasPrimary(S, KnownConsts, Sub))
      return false;
    Val = (-Sub) & 0xFFFF;
    return true;
  }
  if (S.consume_front("~")) {
    int64_t Sub = 0;
    if (!parseXasPrimary(S, KnownConsts, Sub))
      return false;
    Val = (~Sub) & 0xFFFF;
    return true;
  }
  if (S.consume_front("(")) {
    int Depth = 1;
    bool InSQ = false, InDQ = false;
    for (size_t I = 0; I < S.size(); ++I) {
      char C = S[I];
      if (C == '\'' && !InDQ) {
        if (InSQ && I + 1 < S.size() && S[I + 1] == '\'') {
          ++I;
          continue;
        }
        InSQ = !InSQ;
      } else if (C == '"' && !InSQ) {
        InDQ = !InDQ;
      } else if (!InSQ && !InDQ) {
        if (C == '(')
          ++Depth;
        else if (C == ')') {
          if (--Depth == 0) {
            StringRef Inner = S.slice(0, I);
            S = S.substr(I + 1);
            return evalXasConstExpr(Inner, KnownConsts, Val);
          }
        }
      }
    }
    return false;
  }
  if (S.consume_front(">")) {
    size_t N = 0;
    while (N < S.size() && isHexDigit(S[N]))
      ++N;
    if (N == 0)
      return false;
    uint64_t U = 0;
    if (S.slice(0, N).getAsInteger(16, U))
      return false;
    S = S.substr(N);
    Val = static_cast<int64_t>(U & 0xFFFF);
    return true;
  }
  if (S.consume_front(":")) {
    size_t N = 0;
    while (N < S.size() && (S[N] == '0' || S[N] == '1'))
      ++N;
    if (N == 0)
      return false;
    uint64_t U = 0;
    if (S.slice(0, N).getAsInteger(2, U))
      return false;
    S = S.substr(N);
    Val = static_cast<int64_t>(U & 0xFFFF);
    return true;
  }
  if (S.starts_with("'")) {
    SmallString<8> Bytes;
    size_t I = 1;
    bool Closed = false;
    while (I < S.size()) {
      if (S[I] == '\'') {
        if (I + 1 < S.size() && S[I + 1] == '\'') {
          Bytes.push_back('\'');
          I += 2;
          continue;
        }
        Closed = true;
        ++I;
        break;
      }
      Bytes.push_back(S[I++]);
    }
    if (!Closed || Bytes.size() > 2)
      return false;
    uint16_t U = 0;
    for (unsigned char C : Bytes)
      U = static_cast<uint16_t>((U << 8) | C);
    S = S.substr(I);
    Val = U;
    return true;
  }
  if (isDigit(S[0])) {
    size_t N = 0;
    while (N < S.size() && isDigit(S[N]))
      ++N;
    uint64_t U = 0;
    if (S.slice(0, N).getAsInteger(10, U))
      return false;
    S = S.substr(N);
    Val = static_cast<int64_t>(U & 0xFFFF);
    return true;
  }
  // Identifier / R0..R15 / KnownConsts
  if (isAlpha(S[0]) || S[0] == '_' || S[0] == '$') {
    if (S[0] == '$' && (S.size() == 1 || !isAlnum(S[1])))
      return false; // PC '$' is not a static constant
    size_t N = 1;
    while (N < S.size() && (isAlnum(S[N]) || S[N] == '_' || S[N] == '$'))
      ++N;
    StringRef Sym = S.slice(0, N);
    std::string Upper = Sym.upper();
    if (Upper.size() >= 2 && Upper[0] == 'R') {
      unsigned RNum = 0;
      if (!StringRef(Upper).drop_front().getAsInteger(10, RNum) && RNum <= 15) {
        S = S.substr(N);
        Val = RNum;
        return true;
      }
    }
    auto It = KnownConsts.find(Upper);
    if (It == KnownConsts.end())
      return false;
    S = S.substr(N);
    Val = It->second & 0xFFFF;
    return true;
  }
  return false;
}

static bool evalXasConstExpr(StringRef Expr,
                             const StringMap<int64_t> &KnownConsts,
                             int64_t &Result) {
  StringRef S = Expr.trim(" \t");
  if (S.empty())
    return false;
  int64_t Acc = 0;
  if (!parseXasPrimary(S, KnownConsts, Acc))
    return false;

  while (true) {
    S = S.ltrim(" \t");
    if (S.empty())
      break;
    StringRef Op;
    for (StringRef Cand : {"//", "%%", "**", "<<", ">>", "+", "-", "*", "/", "%", "&", "|", "^"}) {
      if (S.starts_with(Cand)) {
        Op = Cand;
        S = S.drop_front(Cand.size());
        break;
      }
    }
    if (Op.empty())
      return false;
    int64_t RHS = 0;
    if (!parseXasPrimary(S, KnownConsts, RHS))
      return false;
    uint16_t UA = static_cast<uint16_t>(Acc);
    uint16_t UB = static_cast<uint16_t>(RHS);
    if (Op == "+")
      Acc = (UA + UB) & 0xFFFF;
    else if (Op == "-")
      Acc = (UA - UB) & 0xFFFF;
    else if (Op == "*") {
      int16_t SA = static_cast<int16_t>(UA), SB = static_cast<int16_t>(UB);
      Acc = static_cast<uint16_t>((int32_t(SA) * int32_t(SB)) & 0xFFFF);
    } else if (Op == "/" || Op == "%") {
      int16_t SA = static_cast<int16_t>(UA), SB = static_cast<int16_t>(UB);
      if (SB == 0)
        return false;
      int32_t Mag = (Op == "/") ? (std::abs(int32_t(SA)) / std::abs(int32_t(SB)))
                                : (std::abs(int32_t(SA)) % std::abs(int32_t(SB)));
      int32_t Sign = (Op == "%") ? (SB < 0 ? -1 : 1)
                                 : (((SA < 0) ^ (SB < 0)) ? -1 : 1);
      Acc = static_cast<uint16_t>((Sign < 0 ? -Mag : Mag) & 0xFFFF);
    } else if (Op == "//" || Op == "%%") {
      if (UB == 0)
        return false;
      Acc = (Op == "//") ? (UA / UB) : (UA % UB);
    } else if (Op == "&")
      Acc = UA & UB;
    else if (Op == "|")
      Acc = UA | UB;
    else if (Op == "^")
      Acc = UA ^ UB;
    else if (Op == "<<")
      Acc = (UB >= 16) ? 0 : ((UA << UB) & 0xFFFF);
    else if (Op == ">>")
      Acc = (UB >= 16) ? 0 : (UA >> UB);
    else if (Op == "**") {
      uint32_t P = 1;
      for (uint16_t I = 0; I < UB; ++I)
        P = (P * UA) & 0xFFFF;
      Acc = P;
    }
  }
  Result = static_cast<int16_t>(static_cast<uint16_t>(Acc));
  return true;
}

// Compute byte length of a single TEXT / STRI item for s# tracking & DORG.
static uint64_t measureTextItemBytes(StringRef Item) {
  Item = Item.trim(" \t");
  if (Item.starts_with("-"))
    Item = Item.drop_front().ltrim(" \t");
  if (Item.starts_with("'") && Item.size() >= 2 && Item.ends_with("'")) {
    StringRef Inner = Item.slice(1, Item.size() - 1);
    if (Inner.empty())
      return 1;
    uint64_t N = 0;
    for (size_t I = 0; I < Inner.size(); ++I) {
      if (Inner[I] == '\'' && I + 1 < Inner.size() && Inner[I + 1] == '\'')
        ++I;
      ++N;
    }
    return N;
  }
  if (Item.starts_with("\"") && Item.size() >= 2 && Item.ends_with("\"")) {
    StringRef Inner = Item.slice(1, Item.size() - 1);
    uint64_t N = 0;
    for (size_t I = 0; I < Inner.size(); ++I) {
      if (Inner[I] == '\\' && I + 1 < Inner.size()) {
        // Check octal \000 escape
        if (I + 3 < Inner.size() && Inner[I + 1] >= '0' && Inner[I + 1] <= '7' &&
            Inner[I + 2] >= '0' && Inner[I + 2] <= '7' &&
            Inner[I + 3] >= '0' && Inner[I + 3] <= '7')
          I += 3;
        else
          ++I;
      }
      ++N;
    }
    return N;
  }
  if (Item.starts_with(">")) {
    size_t HexDigits = Item.drop_front().size();
    return (HexDigits + 1) / 2;
  }
  return 0;
}

// Expand COPY directives recursively so included xas99 files also normalize.
static void collectLinesWithCopy(SourceMgr &SM, StringRef Text, SMLoc IncludeLoc,
                                 unsigned Depth,
                                 SmallVectorImpl<std::string> &Lines) {
  SmallVector<StringRef, 256> RawLines;
  Text.split(RawLines, '\n');
  bool InMacro = false;
  for (StringRef RawLine : RawLines) {
    StringRef Line = RawLine;
    Line.consume_back("\r");

    StringRef Trimmed = Line.ltrim(" \t");
    if (Trimmed.starts_with_insensitive(".DEFM"))
      InMacro = true;
    else if (Trimmed.starts_with_insensitive(".ENDM"))
      InMacro = false;

    if (!InMacro && !Trimmed.empty() && !Trimmed.starts_with("*") &&
        !Trimmed.starts_with(";")) {
      // Check for [label] COPY "file" / 'file'
      StringRef Scan = Line;
      StringRef MaybeLabel;
      if (!isSpace(Scan[0])) {
        size_t Sp = Scan.find_first_of(" \t");
        if (Sp != StringRef::npos) {
          StringRef Tok0 = Scan.slice(0, Sp);
          if (!Tok0.equals_insensitive("COPY")) {
            MaybeLabel = Tok0;
            Scan = Scan.substr(Sp);
          }
        }
      }
      Scan = Scan.ltrim(" \t");
      if (Scan.starts_with_insensitive("COPY") &&
          (Scan.size() == 4 || isSpace(Scan[4])) && Depth < 16) {
        auto [Arg, _] = splitOperandsAndComment(Scan.drop_front(4));
        if (Arg.size() >= 2 &&
            ((Arg.starts_with("\"") && Arg.ends_with("\"")) ||
             (Arg.starts_with("'") && Arg.ends_with("'")))) {
          std::string Filename = Arg.slice(1, Arg.size() - 1).str();
          std::string IncludedPath;
          if (ErrorOr<std::unique_ptr<MemoryBuffer>> IncBuf =
                  SM.OpenIncludeFile(Filename, IncludedPath)) {
            if (!MaybeLabel.empty())
              Lines.push_back(MaybeLabel.str());
            collectLinesWithCopy(SM, (*IncBuf)->getBuffer(), IncludeLoc,
                                 Depth + 1, Lines);
            continue;
          }
        }
      }
    }
    Lines.push_back(Line.str());
  }
}

struct AutoConstSpec {
  std::string Sym;
  std::string Expr;
  int64_t SortKey;
  bool HasConstKey;
  bool IsWord;
};

// Rewrite xas99 expression sugar inside a single operand:
//  - s#sym -> __XAS_SZ_<SYM>
//  - b#expr / w#expr -> @__XAS_AUTO_<N>
//  - @offset(expr) -> evaluate constant or rewrite; evaluate constant expressions
//    containing xas99-only '//', '%%', '**', '>>', '-''', '+>', or non-C precedence!
static std::string rewriteOperand(StringRef Op,
                                  const StringMap<int64_t> &KnownConsts,
                                  SmallVectorImpl<AutoConstSpec> &AutoPool,
                                  unsigned &AutoId) {
  Op = Op.trim(" \t");
  if (Op.empty())
    return "";

  // Handle b#expr / w#expr auto-constants
  if (Op.size() > 2 && (Op[0] == 'b' || Op[0] == 'B' || Op[0] == 'w' || Op[0] == 'W') &&
      Op[1] == '#') {
    bool IsWord = (Op[0] == 'w' || Op[0] == 'W');
    StringRef Inner = Op.drop_front(2);
    int64_t Val = 0;
    bool HasVal = evalXasConstExpr(Inner, KnownConsts, Val);
    std::string ValExpr = HasVal ? std::to_string(Val) : Inner.str();
    std::string Sym = "__XAS_AUTO_" + std::to_string(AutoId++);
    uint16_t Key = IsWord ? static_cast<uint16_t>(Val)
                          : static_cast<uint16_t>(Val & 0xFF);
    AutoPool.push_back({Sym, ValExpr, static_cast<int64_t>(Key), HasVal, IsWord});
    return "@" + Sym;
  }

  // Replace any s#ident occurrences outside quotes with __XAS_SZ_IDENT
  std::string Work;
  Work.reserve(Op.size() + 16);
  bool InSQ = false, InDQ = false;
  for (size_t I = 0; I < Op.size();) {
    char C = Op[I];
    if (C == '\'' && !InDQ) {
      Work.push_back(C);
      if (InSQ && I + 1 < Op.size() && Op[I + 1] == '\'') {
        Work.push_back(Op[I + 1]);
        I += 2;
        continue;
      }
      InSQ = !InSQ;
      ++I;
      continue;
    }
    if (C == '"' && !InSQ) {
      InDQ = !InDQ;
      Work.push_back(C);
      ++I;
      continue;
    }
    if (!InSQ && !InDQ && (C == 's' || C == 'S') && I + 1 < Op.size() &&
        Op[I + 1] == '#' &&
        (I == 0 || (!isAlnum(Op[I - 1]) && Op[I - 1] != '_'))) {
      size_t J = I + 2;
      while (J < Op.size() && (isAlnum(Op[J]) || Op[J] == '_' || Op[J] == '$'))
        ++J;
      if (J > I + 2) {
        Work += "__XAS_SZ_" + Op.slice(I + 2, J).upper();
        I = J;
        continue;
      }
    }
    Work.push_back(C);
    ++I;
  }

  // Handle @addr[(idx)] vs plain expression
  StringRef W(Work);
  if (W.starts_with("*"))
    return Work;

  if (W.starts_with("@")) {
    StringRef Body = W.drop_front(1);
    // Check if Body ends with (idx) that is NOT the entire parenthesized expression @(work)
    StringRef AddrPart = Body;
    StringRef IdxSuffix;
    if (Body.ends_with(")")) {
      // Find matching '(' for the final ')'
      int Depth = 0;
      bool SQ = false, DQ = false;
      size_t MatchLP = StringRef::npos;
      for (size_t I = Body.size(); I > 0; --I) {
        char C = Body[I - 1];
        if (C == '\'' && !DQ)
          SQ = !SQ;
        else if (C == '"' && !SQ)
          DQ = !DQ;
        else if (!SQ && !DQ) {
          if (C == ')')
            ++Depth;
          else if (C == '(') {
            if (--Depth == 0) {
              MatchLP = I - 1;
              break;
            }
          }
        }
      }
      if (MatchLP != StringRef::npos && MatchLP > 0) {
        // Check whether char before MatchLP (ignoring spaces) is a binary operator
        StringRef Before = Body.slice(0, MatchLP).rtrim(" \t");
        if (!Before.empty() &&
            StringRef("+-*/%&|^~<>").find(Before.back()) == StringRef::npos) {
          AddrPart = Before;
          IdxSuffix = Body.substr(MatchLP);
        }
      }
    }
    std::string IdxStr = IdxSuffix.str();
    if (IdxSuffix.size() >= 3 && IdxSuffix.starts_with("(") &&
        IdxSuffix.ends_with(")")) {
      int64_t IdxVal = 0;
      if (evalXasConstExpr(IdxSuffix.slice(1, IdxSuffix.size() - 1),
                           KnownConsts, IdxVal) &&
          IdxVal >= 0 && IdxVal <= 15) {
        IdxStr = "(" + std::to_string(IdxVal) + ")";
      }
    }
    int64_t V = 0;
    if (evalXasConstExpr(AddrPart, KnownConsts, V)) {
      return "@" + std::to_string(V) + IdxStr;
    }
    return "@" + AddrPart.str() + IdxStr;
  }

  // Plain operand: do NOT fold bare integer/register operands if Mnemonic expects a register!
  // Only fold if it contains operators/quotes/hex quirks where xas99 evaluation matters.
  bool HasQuirkOrOp = false;
  for (size_t I = 0; I < W.size(); ++I) {
    if (StringRef("+-*/%&|^~'()<").find(W[I]) != StringRef::npos ||
        (W[I] == '>' && I + 1 < W.size() && W[I + 1] == '>')) {
      HasQuirkOrOp = true;
      break;
    }
  }
  if (HasQuirkOrOp && !W.starts_with("!") && !W.starts_with("-!")) {
    int64_t V = 0;
    if (evalXasConstExpr(W, KnownConsts, V))
      return std::to_string(V);
  }
  return Work;
}

static void emitFlushedAutoPool(SmallVectorImpl<AutoConstSpec> &AutoPool,
                                raw_string_ostream &Out) {
  if (AutoPool.empty())
    return;
  SmallVector<AutoConstSpec, 16> Bytes, Words;
  for (const auto &E : AutoPool) {
    if (E.IsWord)
      Words.push_back(E);
    else
      Bytes.push_back(E);
  }
  auto Cmp = [](const AutoConstSpec &A, const AutoConstSpec &B) {
    if (A.HasConstKey != B.HasConstKey)
      return A.HasConstKey > B.HasConstKey;
    return A.SortKey < B.SortKey;
  };
  llvm::stable_sort(Bytes, Cmp);
  llvm::stable_sort(Words, Cmp);

  int64_t PrevKey = -1;
  std::string PrevSym;
  for (const auto &B : Bytes) {
    if (B.HasConstKey && B.SortKey == PrevKey && !PrevSym.empty()) {
      Out << B.Sym << " EQU " << PrevSym << "\n";
      continue;
    }
    Out << B.Sym << " BYTE " << B.Expr << "\n";
    if (B.HasConstKey) {
      PrevKey = B.SortKey;
      PrevSym = B.Sym;
    }
  }
  PrevKey = -1;
  PrevSym.clear();
  if (!Words.empty())
    Out << "        EVEN\n";
  for (const auto &W : Words) {
    if (W.HasConstKey && W.SortKey == PrevKey && !PrevSym.empty()) {
      Out << W.Sym << " EQU " << PrevSym << "\n";
      continue;
    }
    Out << W.Sym << " DATA " << W.Expr << "\n";
    if (W.HasConstKey) {
      PrevKey = W.SortKey;
      PrevSym = W.Sym;
    }
  }
  AutoPool.clear();
}

} // end anonymous namespace

void llvm::normalizeXas99SourceBuffer(MCAsmParser &Parser) {
  SourceMgr &SM = Parser.getSourceManager();
  unsigned MainID = SM.getMainFileID();
  const MemoryBuffer *MainBuf = SM.getMemoryBuffer(MainID);
  StringRef Orig = MainBuf->getBuffer();

  SmallVector<std::string, 512> Lines;
  collectLinesWithCopy(SM, Orig, SMLoc(), 0, Lines);

  // Quick scan: is s# used anywhere?
  bool HasSizeOp = false;
  for (const std::string &L : Lines) {
    if (StringRef(L).contains_insensitive("s#")) {
      HasSizeOp = true;
      break;
    }
  }

  std::string OutBuf;
  OutBuf.reserve(Orig.size() + 512);
  raw_string_ostream Out(OutBuf);

  StringMap<int64_t> KnownConsts;
  SmallVector<AutoConstSpec, 16> AutoPool;
  unsigned AutoId = 0;

  bool InMacro = false;
  bool InDorg = false;
  std::string DorgBase = "0";
  int64_t DorgOff = 0;

  std::string PendingLabel;
  std::string ActiveSizeSym;
  uint64_t ActiveSizeBytes = 0;

  auto FlushSizeSym = [&]() {
    if (HasSizeOp && !ActiveSizeSym.empty()) {
      Out << "        .set __XAS_SZ_" << ActiveSizeSym << ", "
          << ActiveSizeBytes << "\n";
    }
  };

  for (size_t LineIdx = 0; LineIdx < Lines.size(); ++LineIdx) {
    StringRef Line(Lines[LineIdx]);
    StringRef LTrim = Line.ltrim(" \t");

    // Pass blank and full-line comment lines straight through
    if (LTrim.empty() || LTrim.starts_with("*") || LTrim.starts_with(";")) {
      Out << Line << "\n";
      continue;
    }

    // Inside .DEFM ... .ENDM: preserve body, stripping trailing comments that
    // contain unmatched quotes (e.g. "byte 1   comment #1 '" in asmacs.asm).
    if (LTrim.starts_with_insensitive(".DEFM")) {
      if (!PendingLabel.empty()) {
        Out << PendingLabel << "\n";
        PendingLabel.clear();
      }
      InMacro = true;
      auto [Head, _] = splitOperandsAndComment(LTrim);
      Out << "        " << Head << "\n";
      continue;
    }
    if (InMacro) {
      if (LTrim.starts_with_insensitive(".ENDM")) {
        InMacro = false;
        auto [Head, _] = splitOperandsAndComment(LTrim);
        Out << "        " << Head << "\n";
        continue;
      }
      // Strip >=2-space trailing comments inside macro lines if they have quotes
      if (LTrim.contains('\'') || LTrim.contains('"')) {
        size_t Sp = LTrim.find_first_of(" \t");
        if (Sp != StringRef::npos) {
          auto [Ops, Cmt] = splitOperandsAndComment(LTrim.substr(Sp));
          if (!Cmt.empty() && (Cmt.contains('\'') || Cmt.contains('"'))) {
            Out << Line.slice(0, Line.size() - LTrim.size())
                << LTrim.slice(0, Sp) << " " << Ops << "\n";
            continue;
          }
        }
      }
      Out << Line << "\n";
      continue;
    }

    // Preserve GNU/ELF dot-directives (.section, .rodata, .text, .byte, etc.)
    if (LTrim.starts_with(".")) {
      if (!PendingLabel.empty()) {
        Out << PendingLabel << "\n";
        PendingLabel.clear();
      }
      Out << Line << "\n";
      continue;
    }

    // Parse optional column-0 label (or local !label)
    StringRef Rest = Line;
    std::string Label;
    bool HadCol0 = !isSpace(Rest[0]);
    if (HadCol0) {
      size_t Sp = Rest.find_first_of(" \t");
      StringRef Tok0 = (Sp == StringRef::npos) ? Rest : Rest.slice(0, Sp);
      Rest = (Sp == StringRef::npos) ? StringRef() : Rest.substr(Sp);

      // Check column-0 directive/mnemonic disambiguation!
      StringRef NextTrim = Rest.ltrim(" \t");
      size_t Sp2 = NextTrim.find_first_of(" \t");
      StringRef Tok1 = (Sp2 == StringRef::npos) ? NextTrim : NextTrim.slice(0, Sp2);
      StringRef Tok0Base = Tok0;
      Tok0Base.consume_back(":");

      bool Col0IsOp = false;
      if (!Tok0.ends_with(":") && !Tok0.starts_with("!") &&
          (isXasDirective(Tok0) || isXasMnemonic(Tok0))) {
        if (Tok0.equals_insensitive("END")) {
          Col0IsOp = true;
        } else if (!NextTrim.empty() && !NextTrim.starts_with("*") &&
                   !NextTrim.starts_with(";")) {
          if (!isXasDirective(Tok1) && !isXasMnemonic(Tok1) &&
              !Tok1.starts_with("."))
            Col0IsOp = true;
        }
      }

      if (Col0IsOp) {
        Rest = Line; // No label; Tok0 is the op!
      } else {
        Tok0.consume_back(":");
        Label = Tok0.str();
      }
    }

    Rest = Rest.ltrim(" \t");
    if (Rest.empty() || Rest.starts_with("*") || Rest.starts_with(";")) {
      // Label-only line! Defer so next-line EQU/WEQU/REQU/BES/DORG can bind to it.
      if (!PendingLabel.empty()) {
        if (InDorg) {
          Out << PendingLabel << " EQU (" << DorgBase << ") + " << DorgOff << "\n";
        } else {
          Out << PendingLabel << "\n";
        }
      }
      PendingLabel = Label;
      continue;
    }

    // Extract Op and Operands
    size_t OpEnd = Rest.find_first_of(" \t;");
    StringRef Op = (OpEnd == StringRef::npos) ? Rest : Rest.slice(0, OpEnd);
    StringRef AfterOp = (OpEnd == StringRef::npos) ? StringRef() : Rest.substr(OpEnd);
    std::string OpUpper = Op.upper();

    // Only attach a deferred standalone label to EQU/WEQU/REQU/BES/DORG (or
    // data-bearing directives for s# tracking when not EVEN!); otherwise emit it
    // on its own line BEFORE EVEN/instructions so odd-byte labels like MSG1E stay exact!
    bool JoinPending =
        (OpUpper == "EQU" || OpUpper == "WEQU" || OpUpper == "REQU" ||
         OpUpper == "BES" || OpUpper == "DORG" || OpUpper == "TEXT" ||
         OpUpper == "STRI" || OpUpper == "BYTE" || OpUpper == "BSS" ||
         OpUpper == "FLOA" || OpUpper == "BCOPY");
    if (Label.empty() && !PendingLabel.empty() && JoinPending) {
      Label = PendingLabel;
      PendingLabel.clear();
    } else if (!PendingLabel.empty()) {
      if (InDorg)
        Out << PendingLabel << " EQU (" << DorgBase << ") + " << DorgOff << "\n";
      else
        Out << PendingLabel << "\n";
      PendingLabel.clear();
    }
    bool ZeroOp = StringSwitch<bool>(OpUpper)
                      .Cases("RTWP", "IDLE", "RSET", "CKOF", true)
                      .Cases("CKON", "LREX", "NOP", "RT", true)
                      .Default(false);
    // On zero-operand instructions, >=2 spaces or '*' or ';' starts a comment
    StringRef OpsStr;
    if (ZeroOp) {
      if (AfterOp.starts_with("\t") || AfterOp.starts_with("  ") ||
          AfterOp.ltrim(" \t").starts_with("*") ||
          AfterOp.ltrim(" \t").starts_with(";"))
        OpsStr = StringRef();
      else
        OpsStr = splitOperandsAndComment(AfterOp).first;
    } else {
      OpsStr = splitOperandsAndComment(AfterOp).first;
    }

    // Handle END: flush auto-constants BEFORE END, and ignore lines after END!
    if (OpUpper == "END") {
      FlushSizeSym();
      ActiveSizeSym.clear();
      if (!Label.empty()) {
        if (InDorg)
          Out << Label << " EQU (" << DorgBase << ") + " << DorgOff << "\n";
        else
          Out << Label << "\n";
      }
      emitFlushedAutoPool(AutoPool, Out);
      Out << "        END" << (OpsStr.empty() ? "" : (" " + OpsStr.str())) << "\n";
      break;
    }

    // Handle AUTO
    if (OpUpper == "AUTO") {
      FlushSizeSym();
      ActiveSizeSym.clear();
      if (!Label.empty())
        Out << Label << "\n";
      emitFlushedAutoPool(AutoPool, Out);
      continue;
    }

    // Handle SAVE / BANK / XORG
    if (OpUpper == "SAVE" || OpUpper == "BANK" || OpUpper == "XORG") {
      FlushSizeSym();
      ActiveSizeSym.clear();
      if (OpUpper == "XORG")
        InDorg = false;
      if (!Label.empty())
        Out << Label << "\n";
      continue;
    }

    // Handle RORG / AORG / DORG
    if (OpUpper == "RORG") {
      FlushSizeSym();
      ActiveSizeSym.clear();
      InDorg = false;
      Out << "        .text\n";
      if (!OpsStr.empty()) {
        std::string Rew = rewriteOperand(OpsStr, KnownConsts, AutoPool, AutoId);
        Out << "        AORG " << Rew << "\n";
      }
      if (!Label.empty())
        Out << Label << "\n";
      continue;
    }
    if (OpUpper == "AORG") {
      FlushSizeSym();
      ActiveSizeSym.clear();
      InDorg = false;
      std::string Rew = rewriteOperand(OpsStr, KnownConsts, AutoPool, AutoId);
      Out << (Label.empty() ? "        " : (Label + " ")) << "AORG " << Rew << "\n";
      continue;
    }
    if (OpUpper == "DORG") {
      FlushSizeSym();
      ActiveSizeSym.clear();
      InDorg = true;
      int64_t V = 0;
      if (evalXasConstExpr(OpsStr, KnownConsts, V)) {
        DorgBase = std::to_string(static_cast<uint16_t>(V));
      } else {
        DorgBase = OpsStr.str();
      }
      DorgOff = 0;
      if (!Label.empty())
        Out << Label << " EQU (" << DorgBase << ")\n";
      continue;
    }

    // Handle EQU / WEQU
    if (OpUpper == "EQU" || OpUpper == "WEQU") {
      FlushSizeSym();
      ActiveSizeSym.clear();
      if (Label.empty() || StringRef(Label).starts_with("!")) {
        // Preserve original line so AsmParser emits the expected error!
        Out << Line << "\n";
        continue;
      }
      SmallVector<StringRef, 4> Items;
      splitCommaOperands(OpsStr, Items);
      if (Items.size() != 1) {
        Out << Label << " EQU " << OpsStr << "\n";
        continue;
      }
      int64_t V = 0;
      std::string ValStr;
      if (evalXasConstExpr(Items[0], KnownConsts, V)) {
        KnownConsts[StringRef(Label).upper()] = V;
        ValStr = std::to_string(V);
      } else {
        ValStr = rewriteOperand(Items[0], KnownConsts, AutoPool, AutoId);
      }
      if (OpUpper == "WEQU") {
        std::string Canon = StringRef(Label).upper();
        Out << "        .set " << Canon << ", " << ValStr << "\n";
      } else {
        Out << Label << " EQU " << ValStr << "\n";
      }
      continue;
    }

    // Handle DORG dummy layout mode (advances DorgOff without emitting bytes!)
    if (InDorg) {
      if (OpUpper == "EVEN") {
        DorgOff = alignTo(DorgOff, 2);
        if (!Label.empty())
          Out << Label << " EQU (" << DorgBase << ") + " << DorgOff << "\n";
        continue;
      }
      if (OpUpper == "BES") {
        int64_t Sz = 0;
        evalXasConstExpr(OpsStr, KnownConsts, Sz);
        DorgOff += Sz;
        if (!Label.empty())
          Out << Label << " EQU (" << DorgBase << ") + " << DorgOff << "\n";
        continue;
      }
      if (OpUpper == "DATA" || isXasMnemonic(OpUpper))
        DorgOff = alignTo(DorgOff, 2);
      if (!Label.empty())
        Out << Label << " EQU (" << DorgBase << ") + " << DorgOff << "\n";
      SmallVector<StringRef, 8> Items;
      splitCommaOperands(OpsStr, Items);
      if (OpUpper == "DATA")
        DorgOff += 2 * Items.size();
      else if (OpUpper == "BYTE")
        DorgOff += Items.size();
      else if (OpUpper == "FLOA")
        DorgOff += 8 * Items.size();
      else if (OpUpper == "BSS") {
        int64_t Sz = 0;
        evalXasConstExpr(OpsStr, KnownConsts, Sz);
        DorgOff += Sz;
      } else if (OpUpper == "TEXT") {
        for (StringRef It : Items)
          DorgOff += measureTextItemBytes(It);
      } else if (OpUpper == "STRI") {
        uint64_t N = 1;
        for (StringRef It : Items)
          N += measureTextItemBytes(It);
        DorgOff += N;
      }
      continue;
    }

    // Normal emission mode: track s#sym byte sizes across consecutive data lines!
    bool IsDataBearing =
        (OpUpper == "DATA" || OpUpper == "BYTE" || OpUpper == "TEXT" ||
         OpUpper == "STRI" || OpUpper == "FLOA" || OpUpper == "BSS" ||
         OpUpper == "BES" || OpUpper == "EVEN" || OpUpper == "BCOPY");
    if (!Label.empty() && !StringRef(Label).starts_with("!")) {
      FlushSizeSym();
      if (IsDataBearing) {
        ActiveSizeSym = StringRef(Label).upper();
        ActiveSizeBytes = 0;
      } else {
        ActiveSizeSym.clear();
        ActiveSizeBytes = 0;
      }
    } else if (!IsDataBearing) {
      FlushSizeSym();
      ActiveSizeSym.clear();
      ActiveSizeBytes = 0;
    }

    SmallVector<StringRef, 8> Items;
    splitCommaOperands(OpsStr, Items);

    if (IsDataBearing && !ActiveSizeSym.empty()) {
      if (OpUpper == "DATA")
        ActiveSizeBytes += 2 * Items.size();
      else if (OpUpper == "BYTE")
        ActiveSizeBytes += Items.size();
      else if (OpUpper == "FLOA")
        ActiveSizeBytes += 8 * Items.size();
      else if (OpUpper == "BSS" || OpUpper == "BES") {
        int64_t Sz = 0;
        if (evalXasConstExpr(OpsStr, KnownConsts, Sz) && Sz > 0)
          ActiveSizeBytes += static_cast<uint64_t>(Sz);
      } else if (OpUpper == "TEXT") {
        for (StringRef It : Items)
          ActiveSizeBytes += measureTextItemBytes(It);
      } else if (OpUpper == "STRI") {
        uint64_t N = 1;
        for (StringRef It : Items)
          N += measureTextItemBytes(It);
        ActiveSizeBytes += N;
      }
    }

    // Handle TEXT -'str' negated-last-byte string items!
    if (OpUpper == "TEXT") {
      bool HasNegQuote = false;
      for (StringRef It : Items)
        if (It.ltrim(" \t").starts_with("-'") || It.ltrim(" \t").starts_with("-\""))
          HasNegQuote = true;
      if (HasNegQuote) {
        if (!Label.empty())
          Out << Label << " ";
        else
          Out << "        ";
        Out << "BYTE ";
        bool FirstB = true;
        for (StringRef It : Items) {
          StringRef T = It.trim(" \t");
          bool Neg = T.consume_front("-");
          T = T.ltrim(" \t");
          if (T.starts_with("'") && T.size() >= 2 && T.ends_with("'")) {
            StringRef Inner = T.slice(1, T.size() - 1);
            SmallVector<uint8_t, 32> Bs;
            for (size_t I = 0; I < Inner.size(); ++I) {
              if (Inner[I] == '\'' && I + 1 < Inner.size() && Inner[I + 1] == '\'')
                ++I;
              Bs.push_back(static_cast<uint8_t>(Inner[I]));
            }
            if (Bs.empty())
              Bs.push_back(0);
            if (Neg)
              Bs.back() = static_cast<uint8_t>(0U - Bs.back());
            for (uint8_t B : Bs) {
              if (!FirstB)
                Out << ",";
              FirstB = false;
              Out << unsigned(B);
            }
          }
        }
        Out << "\n";
        continue;
      }
    }

    // Rewrite operands ONLY on instructions and numeric-expression directives
    bool RewriteOps =
        isXasMnemonic(OpUpper) || OpUpper == "DATA" || OpUpper == "BYTE" ||
        OpUpper == "BSS" || OpUpper == "BES";
    // Preserve intentional error cases (e.g. "DATA >1234 trailing" with single space!)
    std::string FinalOps;
    if (RewriteOps && !Items.empty()) {
      raw_string_ostream OS(FinalOps);
      for (size_t I = 0; I < Items.size(); ++I) {
        if (I > 0)
          OS << ", ";
        // Only rewrite if operand does not contain an internal space separating garbage tokens
        OS << rewriteOperand(Items[I], KnownConsts, AutoPool, AutoId);
      }
    } else {
      FinalOps = OpsStr.str();
    }

    if (!Label.empty())
      Out << Label << " ";
    else
      Out << "        ";
    Out << Op;
    if (!FinalOps.empty())
      Out << " " << FinalOps;
    Out << "\n";
  }

  if (!PendingLabel.empty()) {
    if (InDorg)
      Out << PendingLabel << " EQU (" << DorgBase << ") + " << DorgOff << "\n";
    else
      Out << PendingLabel << "\n";
  }
  FlushSizeSym();
  emitFlushedAutoPool(AutoPool, Out);

  Parser.replaceBuffer(Out.str(), MainBuf->getBufferIdentifier());
}
