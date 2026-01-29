#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TOP="$(cd "${ROOT}/.." && pwd)"
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

CLANG="${CLANG:-$ROOT/build-clang-8085/bin/clang}"
SYSROOT="${SYSROOT:-$TOP/sysroot}"
LINKER="${LINKER:-$SYSROOT/ldscripts/i8085-32kram-32krom.ld}"
OBJCOPY_DEFAULT="$ROOT/build-clang-8085/bin/llvm-objcopy"
OBJCOPY="${OBJCOPY:-${OBJCOPY_DEFAULT}}"
TRACE="${TRACE:-$TOP/i8085-trace/build/i8085-trace}"
OUTDIR="${OUTDIR:-$TOP/tooling/smoke-build}"

CFLAGS_BASE=(--target=i8085-unknown-elf --sysroot="${SYSROOT}" -ffreestanding -fno-builtin -Oz)
LDFLAGS_BASE=(-fuse-ld=lld -Wl,-T,"${LINKER}")
TRACE_COMMON=(-q -S)

if [[ ! -x "${CLANG}" ]]; then
  echo "Missing clang at ${CLANG}" >&2
  exit 1
fi
if [[ ! -f "${LINKER}" ]]; then
  echo "Missing linker script at ${LINKER}" >&2
  exit 1
fi
if [[ ! -x "${OBJCOPY}" ]]; then
  echo "Missing llvm-objcopy at ${OBJCOPY}" >&2
  exit 1
fi

mkdir -p "${OUTDIR}"

run_test() {
  local name="$1"
  local trace_args="$2"
  local extra_cflags="$3"
  local expected="$4"
  shift 4
  local map_file="${OUTDIR}/${name}.map"
  local elf_file="${OUTDIR}/${name}.elf"
  local bin_file="${OUTDIR}/${name}.bin"

  # shellcheck disable=SC2086
  "${CLANG}" "${CFLAGS_BASE[@]}" ${extra_cflags} \
    "${LDFLAGS_BASE[@]}" -Wl,-Map,"${map_file}" \
    "$@" -o "${elf_file}"

  "${OBJCOPY}" -O binary "${elf_file}" "${bin_file}"

  echo "ELF: ${elf_file}"

  if [[ -x "${TRACE}" ]]; then
    # shellcheck disable=SC2086
    local trace_output
    trace_output=$("${TRACE}" "${TRACE_COMMON[@]}" -e 0x0000 -l 0x0000 ${trace_args} "${bin_file}" 2>&1)
    echo "${trace_output}"
    if [[ -n "${expected}" ]]; then
      local dump_line
      dump_line=$(echo "${trace_output}" | rg -m1 "^  0100:")
      if [[ -z "${dump_line}" ]]; then
        echo "Missing memory dump for ${name}" >&2
        exit 1
      fi
      local got
      got=$(echo "${dump_line}" | awk '{print $2" "$3" "$4" "$5}')
      if [[ "${got}" != "${expected}" ]]; then
        echo "Mismatch for ${name}: expected ${expected}, got ${got}" >&2
        exit 1
      fi
    fi
  else
    echo "Missing i8085-trace at ${TRACE}; skipping sim run" >&2
  fi
}

run_test "smoke" "-n 2000 -d 0x0100:4" "" "2A 55 7F 00" "${DIR}/smoke.c"
run_test "init" "-n 2000 -d 0x0100:4" "" "AA 04 EF BE" "${DIR}/init.c"
run_test "recursion" "-n 40000 -d 0x0100:4" "-fno-optimize-sibling-calls" "4F 00 00 00" "${DIR}/recursion.c"
run_test "heap" "-n 10000 -d 0x0100:4" "" "4A 08 1C 00" "${DIR}/heap.c"
run_test "fib" "-n 4000 -d 0x0100:4" "" "DB 03 10 00" "${DIR}/fib.c"
run_test "shift" "-n 2000 -d 0x0100:4" "" "12 40 23 A5" "${DIR}/shift.c"
run_test "irq" "--irq=55@120 -n 4000 -d 0x0100:4" "" "5A 00 00 00" "${DIR}/irq.c" "${DIR}/irq.S"
