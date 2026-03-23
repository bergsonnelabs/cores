/**
 * cmsis_compiler.h -- Minimal shim
 *
 * stm32_timer.h includes <cmsis_compiler.h>. The GCC ARM toolchain
 * provides __get_PRIMASK etc. as built-ins, so we just need the
 * header to exist and provide __STATIC_INLINE if needed.
 */

#ifndef __CMSIS_COMPILER_H
#define __CMSIS_COMPILER_H

/* GCC ARM provides these intrinsics natively via arm_acle.h or built-ins.
 * Just make sure the macros exist for any code that uses them. */

#ifndef __STATIC_INLINE
#define __STATIC_INLINE  static inline
#endif

#ifndef __STATIC_FORCEINLINE
#define __STATIC_FORCEINLINE  static inline __attribute__((always_inline))
#endif

#ifndef __WEAK
#define __WEAK  __attribute__((weak))
#endif

#ifndef __PACKED
#define __PACKED  __attribute__((packed))
#endif

#ifndef __IO
#define __IO  volatile
#endif

#endif /* __CMSIS_COMPILER_H */
