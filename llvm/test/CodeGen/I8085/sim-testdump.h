#ifndef I8085_SIM_TESTDUMP_H
#define I8085_SIM_TESTDUMP_H

#include <stdint.h>

#define TESTDUMP_SECTION __attribute__((section(".testdump")))
#define TESTDUMP_ALIGN __attribute__((aligned(2)))

#define TESTDUMP_U8(name, count) \
  volatile uint8_t name[count] TESTDUMP_SECTION TESTDUMP_ALIGN
#define TESTDUMP_I8(name, count) \
  volatile int8_t name[count] TESTDUMP_SECTION TESTDUMP_ALIGN
#define TESTDUMP_U16(name, count) \
  volatile uint16_t name[count] TESTDUMP_SECTION TESTDUMP_ALIGN
#define TESTDUMP_I16(name, count) \
  volatile int16_t name[count] TESTDUMP_SECTION TESTDUMP_ALIGN
#define TESTDUMP_U32(name, count) \
  volatile uint32_t name[count] TESTDUMP_SECTION TESTDUMP_ALIGN
#define TESTDUMP_I32(name, count) \
  volatile int32_t name[count] TESTDUMP_SECTION TESTDUMP_ALIGN
#define TESTDUMP_U64(name, count) \
  volatile uint64_t name[count] TESTDUMP_SECTION TESTDUMP_ALIGN
#define TESTDUMP_I64(name, count) \
  volatile int64_t name[count] TESTDUMP_SECTION TESTDUMP_ALIGN
#define TESTDUMP_F32(name, count) \
  volatile float name[count] TESTDUMP_SECTION TESTDUMP_ALIGN

#endif
