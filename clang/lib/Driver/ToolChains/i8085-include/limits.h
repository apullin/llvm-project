#ifndef _I8085_LIMITS_H_
#define _I8085_LIMITS_H_

#define CHAR_BIT __CHAR_BIT__

#define SCHAR_MAX __SCHAR_MAX__
#define SCHAR_MIN (-SCHAR_MAX - 1)
#define UCHAR_MAX __UCHAR_MAX__

#define CHAR_MIN __CHAR_MIN__
#define CHAR_MAX __CHAR_MAX__

#define SHRT_MAX __SHRT_MAX__
#define SHRT_MIN (-SHRT_MAX - 1)
#define USHRT_MAX __USHRT_MAX__

#define INT_MAX __INT_MAX__
#define INT_MIN (-INT_MAX - 1)
#ifdef __UINT_MAX__
#define UINT_MAX __UINT_MAX__
#else
#define UINT_MAX __UINT16_MAX__
#endif

#define LONG_MAX __LONG_MAX__
#define LONG_MIN (-LONG_MAX - 1)
#ifdef __ULONG_MAX__
#define ULONG_MAX __ULONG_MAX__
#else
#define ULONG_MAX __UINT32_MAX__
#endif

#define LLONG_MAX __LONG_LONG_MAX__
#define LLONG_MIN (-LLONG_MAX - 1)
#define ULLONG_MAX __ULLONG_MAX__

#endif // _I8085_LIMITS_H_
