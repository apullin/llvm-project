// RUN: %clang -target tms9900 -O0 -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=DEFAULT
// RUN: %clang -target tms9900 -O2 -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=DEFAULT
// RUN: %clang -target tms9900 -O2 -fomit-frame-pointer -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=DEFAULT
// RUN: %clang -target tms9900 -O2 -fno-omit-frame-pointer -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=KEEP

// DEFAULT: "-mframe-pointer=none"
// KEEP: "-mframe-pointer=all"
