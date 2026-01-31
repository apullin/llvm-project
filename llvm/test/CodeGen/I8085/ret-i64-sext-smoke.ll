; RUN: llc -mtriple=i8085-unknown-elf -O0 -o /dev/null %s

define i64 @ret_sext_i16_i64(i16 %x) {
  %ext = sext i16 %x to i64
  ret i64 %ext
}
