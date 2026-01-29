; RUN: llc -mtriple=i8085-unknown-elf < %s -o - | FileCheck %s

; Function Attrs: nounwind
define void @cfi_stack(i16 %x) #0 !dbg !3 {
; CHECK-LABEL: cfi_stack:
; CHECK: .cfi_startproc
; CHECK: .cfi_adjust_cfa_offset 2
entry:
  %slot = alloca i16, align 1
  call void @llvm.dbg.declare(metadata ptr %slot, metadata !9, metadata !DIExpression()), !dbg !11
  store volatile i16 %x, ptr %slot, align 1, !dbg !11
  call void @touch(ptr %slot), !dbg !12
  ret void, !dbg !13
}

declare void @llvm.dbg.declare(metadata, metadata, metadata)
declare void @touch(ptr)

attributes #0 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!6, !7}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "", isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "file.c", directory: "/tmp")
!2 = !{}
!3 = distinct !DISubprogram(name: "cfi_stack", scope: !1, file: !1, line: 1, type: !4, isLocal: false, isDefinition: true, scopeLine: 1, isOptimized: false, unit: !0, retainedNodes: !2)
!4 = !DISubroutineType(types: !5)
!5 = !{null, !8}
!6 = !{i32 2, !"Dwarf Version", i32 4}
!7 = !{i32 2, !"Debug Info Version", i32 3}
!8 = !DIBasicType(name: "short", size: 16, align: 8, encoding: DW_ATE_signed)
!9 = !DILocalVariable(name: "slot", scope: !3, file: !1, line: 2, type: !8)
!10 = !DIExpression()
!11 = !DILocation(line: 2, column: 3, scope: !3)
!12 = !DILocation(line: 3, column: 1, scope: !3)
!13 = !DILocation(line: 4, column: 1, scope: !3)
