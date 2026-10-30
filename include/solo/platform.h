/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOLO_PLATFORM_H
#define SOLO_PLATFORM_H

#include <solo/types.h>

/* =============================================================================
 * Compiler Identification
 * ============================================================================= */
#if defined(__clang__)
#define SOLO_CC_CLANG 1
#define SOLO_CC_NAME  "clang"
#elif defined(__GNUC__)
#define SOLO_CC_GCC  1
#define SOLO_CC_NAME "gcc"
#elif defined(_MSC_VER)
#define SOLO_CC_MSVC 1
#define SOLO_CC_NAME "msvc"
#elif defined(__WATCOMC__)
#define SOLO_CC_WATCOM 1
#define SOLO_CC_NAME   "watcom"
#elif defined(__BORLANDC__)
#define SOLO_CC_BORLAND 1
#define SOLO_CC_NAME    "borland"
#elif defined(__VBCC__)
#define SOLO_CC_VBCC 1
#define SOLO_CC_NAME "vbcc"
#elif defined(__SASC) || defined(_AMIGA_SASC)
#define SOLO_CC_SAS  1
#define SOLO_CC_NAME "sasc"
#elif defined(__TINYC__)
#define SOLO_CC_TCC  1
#define SOLO_CC_NAME "tcc"
#else
#define SOLO_CC_UNKNOWN 1
#define SOLO_CC_NAME    "unknown"
#endif

/* =============================================================================
 * Portability & C89 Vintage Compiler Compatibility Helpers
 * ============================================================================= */
#ifndef SOLO_UNUSED
#define SOLO_UNUSED(x) ((void)(x))
#endif

#ifndef SOLO_INLINE
#if defined(__cplusplus)
#define SOLO_INLINE inline
#elif defined(__GNUC__) || defined(__clang__)
#define SOLO_INLINE static __inline__
#elif defined(_MSC_VER) || defined(__WATCOMC__)
#define SOLO_INLINE static __inline
#else
#define SOLO_INLINE static
#endif
#endif

#ifndef SOLO_UNUSED_FN
#if defined(__GNUC__) || defined(__clang__)
#define SOLO_UNUSED_FN __attribute__((unused))
#else
#define SOLO_UNUSED_FN
#endif
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * Portable safe strdup for environments without standard POSIX strdup.
 * Allocates a duplicate of s using malloc and memcpy. Returns NULL if s is NULL
 * or if memory allocation fails.
 */
SOLO_UNUSED_FN SOLO_INLINE char *solo_strdup(const char *s)
{
    size_t len;
    char *copy;
    if (s == NULL)
        return NULL;
    len = strlen(s);
    copy = (char *)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

#ifndef safe_strdup
#define safe_strdup solo_strdup
#endif

/* =============================================================================
 * Operating System Identification & Normalization (SOLO_IS_*)
 * ============================================================================= */
#if defined(__AROS__)
#define SOLO_IS_AROS  1
#define SOLO_IS_AMIGA 1
#define SOLO_OS_NAME  "aros"

#elif defined(__amigaos4__)
#define SOLO_IS_AMIGAOS4 1
#define SOLO_IS_AMIGA    1
#define SOLO_OS_NAME     "amigaos4"

#elif defined(__AMIGA__) || defined(AMIGA) || defined(_AMIGA) || defined(__amigaos__)
#define SOLO_IS_AMIGA_CLASSIC 1
#define SOLO_IS_AMIGA         1
#define SOLO_OS_NAME          "amiga"

#elif defined(__KOLIBRIOS__) || defined(__MENUETOS__)
#define SOLO_IS_KOLIBRI 1
#define SOLO_OS_NAME    "kolibri"

#elif defined(_PLAN9_SOURCE) || defined(__plan9__) || defined(_PLAN9)
#define SOLO_IS_PLAN9 1
#define SOLO_OS_NAME  "plan9"

#elif defined(_WIN32_WCE) || defined(UNDER_CE)
#define SOLO_IS_WINDOWS_CE 1
#define SOLO_IS_WINDOWS    1
#define SOLO_OS_NAME       "wince"

#elif defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
#define SOLO_IS_WINDOWS_NT 1
#define SOLO_IS_WINDOWS    1
#define SOLO_OS_NAME       "windows"

#elif defined(__MSDOS__) || defined(MSDOS) || defined(_MSDOS) || defined(__DOS__)
#define SOLO_IS_DOS  1
#define SOLO_OS_NAME "dos"

#elif defined(__OS2__) || defined(_OS2)
#define SOLO_IS_OS2  1
#define SOLO_OS_NAME "os2"

#elif defined(__HAIKU__)
#define SOLO_IS_HAIKU 1
#define SOLO_IS_POSIX 1
#define SOLO_OS_NAME  "haiku"

#elif defined(__BEOS__)
#define SOLO_IS_BEOS  1
#define SOLO_IS_POSIX 1
#define SOLO_OS_NAME  "beos"

#elif defined(__ATARIST__) || defined(__atarist__) || defined(__atarist) || defined(atarist) ||    \
    defined(__TOS__) || defined(__MINT__)
#define SOLO_IS_ATARIST 1
#define SOLO_OS_NAME    "atarist"

#elif defined(macintosh) || defined(__MACOS__)
#define SOLO_IS_MAC68K 1
#define SOLO_OS_NAME   "mac68k"

#elif defined(_HUMAN68K__) || defined(__human68k__)
#define SOLO_IS_X68K 1
#define SOLO_OS_NAME "x68k"

#elif defined(__wasi__)
#define SOLO_IS_WASI 1
#define SOLO_OS_NAME "wasi"

#elif defined(__linux__)
#define SOLO_IS_LINUX 1
#define SOLO_IS_POSIX 1
#define SOLO_OS_NAME  "linux"

#elif defined(__APPLE__) && defined(__MACH__)
#define SOLO_IS_DARWIN 1
#define SOLO_IS_POSIX  1
#define SOLO_OS_NAME   "darwin"

#elif defined(__FreeBSD__)
#define SOLO_IS_FREEBSD 1
#define SOLO_IS_BSD     1
#define SOLO_IS_POSIX   1
#define SOLO_OS_NAME    "freebsd"

#elif defined(__OpenBSD__)
#define SOLO_IS_OPENBSD 1
#define SOLO_IS_BSD     1
#define SOLO_IS_POSIX   1
#define SOLO_OS_NAME    "openbsd"

#elif defined(__NetBSD__)
#define SOLO_IS_NETBSD 1
#define SOLO_IS_BSD    1
#define SOLO_IS_POSIX  1
#define SOLO_OS_NAME   "netbsd"

#elif defined(__minix) || defined(__minix__) || defined(_MINIX)
#define SOLO_IS_MINIX 1
#define SOLO_IS_POSIX 1
#define SOLO_OS_NAME  "minix"

#elif defined(__sun) && defined(__SVR4)
#define SOLO_IS_SOLARIS 1
#define SOLO_IS_POSIX   1
#define SOLO_OS_NAME    "solaris"

#elif defined(__unix__) || defined(__unix)
#define SOLO_IS_GENERIC_UNIX 1
#define SOLO_IS_POSIX        1
#define SOLO_OS_NAME         "unix"

#else
#define SOLO_IS_UNKNOWN 1
#define SOLO_OS_NAME    "unknown"
#endif

/* =============================================================================
 * Fallback Defaults for Boolean Platform Predicates (0 when inactive)
 * ============================================================================= */
#ifndef SOLO_IS_POSIX
#define SOLO_IS_POSIX 0
#endif
#ifndef SOLO_IS_WINDOWS
#define SOLO_IS_WINDOWS 0
#endif
#ifndef SOLO_IS_WINDOWS_NT
#define SOLO_IS_WINDOWS_NT 0
#endif
#ifndef SOLO_IS_WINDOWS_CE
#define SOLO_IS_WINDOWS_CE 0
#endif
#ifndef SOLO_IS_AMIGA
#define SOLO_IS_AMIGA 0
#endif
#ifndef SOLO_IS_AMIGA_CLASSIC
#define SOLO_IS_AMIGA_CLASSIC 0
#endif
#ifndef SOLO_IS_AROS
#define SOLO_IS_AROS 0
#endif
#ifndef SOLO_IS_AMIGAOS4
#define SOLO_IS_AMIGAOS4 0
#endif
#ifndef SOLO_IS_LINUX
#define SOLO_IS_LINUX 0
#endif
#ifndef SOLO_IS_DARWIN
#define SOLO_IS_DARWIN 0
#endif
#ifndef SOLO_IS_BSD
#define SOLO_IS_BSD 0
#endif
#ifndef SOLO_IS_FREEBSD
#define SOLO_IS_FREEBSD 0
#endif
#ifndef SOLO_IS_OPENBSD
#define SOLO_IS_OPENBSD 0
#endif
#ifndef SOLO_IS_NETBSD
#define SOLO_IS_NETBSD 0
#endif
#ifndef SOLO_IS_MINIX
#define SOLO_IS_MINIX 0
#endif
#ifndef SOLO_IS_SOLARIS
#define SOLO_IS_SOLARIS 0
#endif
#ifndef SOLO_IS_GENERIC_UNIX
#define SOLO_IS_GENERIC_UNIX 0
#endif
#ifndef SOLO_IS_HAIKU
#define SOLO_IS_HAIKU 0
#endif
#ifndef SOLO_IS_BEOS
#define SOLO_IS_BEOS 0
#endif
#ifndef SOLO_IS_PLAN9
#define SOLO_IS_PLAN9 0
#endif
#ifndef SOLO_IS_KOLIBRI
#define SOLO_IS_KOLIBRI 0
#endif
#ifndef SOLO_IS_DOS
#define SOLO_IS_DOS 0
#endif
#ifndef SOLO_IS_OS2
#define SOLO_IS_OS2 0
#endif
#ifndef SOLO_IS_ATARIST
#define SOLO_IS_ATARIST 0
#endif
#ifndef SOLO_IS_MAC68K
#define SOLO_IS_MAC68K 0
#endif
#ifndef SOLO_IS_X68K
#define SOLO_IS_X68K 0
#endif
#ifndef SOLO_IS_WASI
#define SOLO_IS_WASI 0
#endif
#ifndef SOLO_IS_UNKNOWN
#define SOLO_IS_UNKNOWN 0
#endif

#if SOLO_IS_POSIX || SOLO_IS_AMIGA || SOLO_IS_WINDOWS || SOLO_IS_KOLIBRI ||            \
    SOLO_IS_PLAN9 || SOLO_IS_DOS
#define SOLO_OS_HAS_NATIVE_DRIVERS 1
#endif

/* =============================================================================
 * Architecture (CPU Arch) Identification & Normalization
 * ============================================================================= */
#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
#define SOLO_ARCH_X86_64 1
#define SOLO_ARCH_NAME   "x86_64"

#elif defined(__i386__) || defined(_M_IX86) || defined(_X86_)
#define SOLO_ARCH_X86  1
#define SOLO_ARCH_NAME "x86"

#elif defined(__aarch64__) || defined(_M_ARM64)
#define SOLO_ARCH_ARM64 1
#define SOLO_ARCH_NAME  "arm64"

#elif defined(__arm__) || defined(_M_ARM)
#define SOLO_ARCH_ARM  1
#define SOLO_ARCH_NAME "arm"

#elif defined(__m68k__) || defined(mc68000) || defined(_M_M68K)
#define SOLO_ARCH_M68K 1
#define SOLO_ARCH_NAME "m68k"

#elif defined(__powerpc__) || defined(__ppc__) || defined(_M_PPC)
#define SOLO_ARCH_PPC  1
#define SOLO_ARCH_NAME "ppc"

#elif defined(__mips__) || defined(_M_MRX000)
#define SOLO_ARCH_MIPS 1
#define SOLO_ARCH_NAME "mips"

#elif defined(__alpha__) || defined(_M_ALPHA)
#define SOLO_ARCH_ALPHA 1
#define SOLO_ARCH_NAME  "alpha"

#elif defined(__ia64__) || defined(_M_IA64)
#define SOLO_ARCH_IA64 1
#define SOLO_ARCH_NAME "ia64"

#elif defined(__sh__) || defined(_M_SH)
#define SOLO_ARCH_SH   1
#define SOLO_ARCH_NAME "sh"

#elif defined(__riscv) && (__riscv_xlen == 64)
#define SOLO_ARCH_RISCV64 1
#define SOLO_ARCH_NAME    "riscv64"

#elif defined(__wasm32__)
#define SOLO_ARCH_WASM32 1
#define SOLO_ARCH_NAME   "wasm32"

#else
#define SOLO_ARCH_UNKNOWN 1
#define SOLO_ARCH_NAME    "unknown"
#endif

/* =============================================================================
 * Word Size / Bitness Normalization
 * ============================================================================= */
#if defined(SOLO_ARCH_X86_64) || defined(SOLO_ARCH_ARM64) || defined(SOLO_ARCH_ALPHA) ||  \
    defined(SOLO_ARCH_IA64) || defined(SOLO_ARCH_RISCV64) || defined(__LP64__) ||            \
    defined(_LP64) || (defined(__WORDSIZE) && (__WORDSIZE == 64))
#define SOLO_BITS 64
#elif SOLO_IS_DOS && !defined(__DJGPP__) && !defined(__WATCOMC__)
#define SOLO_BITS 16
#else
#define SOLO_BITS 32
#endif

#define SOLO_IS_64BIT (SOLO_BITS == 64)
#define SOLO_IS_32BIT (SOLO_BITS == 32)
#define SOLO_IS_16BIT (SOLO_BITS == 16)

/* =============================================================================
 * Endianness / Byte Order Determination
 * ============================================================================= */
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) &&                                    \
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define SOLO_ENDIAN_BIG 1
#elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) &&                               \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define SOLO_ENDIAN_LITTLE 1
#elif defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN)
#define SOLO_ENDIAN_BIG 1
#elif defined(__LITTLE_ENDIAN__) || defined(_LITTLE_ENDIAN)
#define SOLO_ENDIAN_LITTLE 1
#elif defined(SOLO_ARCH_M68K) || defined(SOLO_ARCH_PPC)
#define SOLO_ENDIAN_BIG 1
#elif defined(SOLO_ARCH_X86) || defined(SOLO_ARCH_X86_64) || defined(SOLO_ARCH_ALPHA) ||  \
    defined(SOLO_ARCH_IA64) || defined(SOLO_ARCH_WASM32)
#define SOLO_ENDIAN_LITTLE 1
#endif

#if defined(SOLO_ENDIAN_LITTLE)
#define SOLO_IS_LITTLE_ENDIAN  1
#define SOLO_IS_BIG_ENDIAN     0
#define SOLO_GET_ENDIAN_NAME() "little"

#elif defined(SOLO_ENDIAN_BIG)
#define SOLO_IS_LITTLE_ENDIAN  0
#define SOLO_IS_BIG_ENDIAN     1
#define SOLO_GET_ENDIAN_NAME() "big"

#else
/*
 * Dynamic C89 union fallback for vintage compilers without byte-order macros.
 */
static const char *solo_detect_endianness(void)
{
    static const char *cached = (const char *)0;
    if (cached == (const char *)0) {
        union {
            unsigned short s;
            unsigned char c[sizeof(unsigned short)];
        } u;
        u.s = 1;
        cached = (u.c[0] == 1) ? "little" : "big";
    }
    return cached;
}
#define SOLO_GET_ENDIAN_NAME() solo_detect_endianness()
#define SOLO_IS_LITTLE_ENDIAN  (solo_detect_endianness()[0] == 'l')
#define SOLO_IS_BIG_ENDIAN     (solo_detect_endianness()[0] == 'b')
#endif


#ifdef __cplusplus
extern "C" {
#endif

/*
 * Returns host OS, architecture, bits and endianness at runtime.
 */
const char *solo_platform_os(void);
const char *solo_platform_arch(void);
int solo_platform_bits(void);
const char *solo_platform_endian(void);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_PLATFORM_H */
