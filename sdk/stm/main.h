/**
 * main.h — Minimal header required by ST middleware
 *
 * Several ST middleware files include "main.h" for Error_Handler,
 * HAL includes, and peripheral init declarations. This provides
 * the minimum needed for our build.
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32wbaxx_hal.h"

/* Error handler — called by ST middleware on fatal errors */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
