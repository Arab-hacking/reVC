// minimal stand-in for reVC's src/core/common.h, so the custom archive reader
// can be built and tested outside of the game
#ifndef __TEST_COMMON_H__
#define __TEST_COMMON_H__
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
#define nil NULL
#endif
#define debug(...) do { } while(0)
#define Const const
