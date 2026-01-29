# RUN: llvm-mc -triple=i8085 -filetype=obj < %s | llvm-readobj -r - | FileCheck %s

  .text
  .extern ext

  mvi a, lo8(ext)
  mvi b, hi8(ext)
  mvi c, hh8(ext)
  mvi d, hhi8(ext)
  lxi h, pm(ext)
  mvi e, pm_lo8(ext)
  mvi l, pm_hi8(ext)
  mvi h, pm_hh8(ext)
  lxi d, gs(ext)
  mvi a, lo8_gs(ext)
  mvi b, hi8_gs(ext)

# CHECK: Relocations [
# CHECK: R_I8085_LO8{{.*}}ext
# CHECK: R_I8085_HI8{{.*}}ext
# CHECK: R_I8085_HH8{{.*}}ext
# CHECK: R_I8085_HHI8{{.*}}ext
# CHECK: R_I8085_PM{{.*}}ext
# CHECK: R_I8085_PM_LO8{{.*}}ext
# CHECK: R_I8085_PM_HI8{{.*}}ext
# CHECK: R_I8085_PM_HH8{{.*}}ext
# CHECK: R_I8085_GS{{.*}}ext
# CHECK: R_I8085_LO8_GS{{.*}}ext
# CHECK: R_I8085_HI8_GS{{.*}}ext
