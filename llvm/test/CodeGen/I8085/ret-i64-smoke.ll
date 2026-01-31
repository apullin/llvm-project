; RUN: llc -mtriple=i8085-unknown-elf -O0 -o /dev/null %s

define i64 @ret_i64_const() {
  ret i64 1
}
