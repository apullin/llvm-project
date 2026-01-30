; RUN: llc -mtriple=i8085-unknown-elf -filetype=asm < %s | FileCheck %s

source_filename = "dbg.c"
target triple = "i8085-unknown-elf"

; Function Attrs: nounwind
define i16 @foo(i16 %x) #0 !dbg !5 {
entry:
  %add = add i16 %x, 1, !dbg !9
  ret i16 %add, !dbg !10
}

attributes #0 = { nounwind }

declare void @llvm.dbg.value(metadata, i64, metadata, metadata)

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "", isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "dbg.c", directory: "/tmp")
!2 = !{}
!3 = !{i32 2, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = distinct !DISubprogram(name: "foo", scope: !1, file: !1, line: 1, type: !6, isLocal: false, isDefinition: true, scopeLine: 1, isOptimized: false, unit: !0, retainedNodes: !2)
!6 = !DISubroutineType(types: !7)
!7 = !{!8, !8}
!8 = !DIBasicType(name: "short", size: 16, align: 8, encoding: DW_ATE_signed)
!9 = !DILocation(line: 2, column: 3, scope: !5)
!10 = !DILocation(line: 3, column: 3, scope: !5)

; CHECK: .file	"dbg.c"
; CHECK: .loc	1 2 3
; CHECK: .loc	1 3 3
; CHECK: .section	.debug_line
