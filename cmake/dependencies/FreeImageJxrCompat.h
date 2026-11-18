#pragma once

#include <wchar.h>

// The bundled codec assumes these Windows SAL annotations exist even when
// FreeImage disables Windows headers. They do not affect the codec ABI.
#ifndef __in_ecount
#define __in_ecount(size)
#endif
#ifndef __out_ecount
#define __out_ecount(size)
#endif
#ifndef __in_win
#define __in_win
#endif
#ifndef __out_win
#define __out_win
#endif

// Defined in LibJXR/image/sys/strcodec.c, but used by segdec.c without a
// declaration. Modern Clang rejects that implicit C declaration.
#if !defined(_WIN32)
#ifdef __cplusplus
extern "C" {
#endif
unsigned int _byteswap_ulong(unsigned int bits);
#ifdef __cplusplus
}
#endif
#endif
