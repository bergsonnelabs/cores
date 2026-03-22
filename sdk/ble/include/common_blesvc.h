/* Minimal common_blesvc.h for SVCCTL */
#ifndef __COMMON_BLESVC_H
#define __COMMON_BLESVC_H

#include "svc_ctl.h"
#include "ble_const.h"
#include "ble_std.h"

/* Stub dbg_trace.h */
#ifndef DBG_TRACE_H
#define DBG_TRACE_H
#endif

/* BLE config defines needed by svc_ctl.c */
#ifndef BLE_CFG_SVC_MAX_NBR_CB
#define BLE_CFG_SVC_MAX_NBR_CB   4
#endif
#ifndef BLE_CFG_CLT_MAX_NBR_CB
#define BLE_CFG_CLT_MAX_NBR_CB   0
#endif
#ifndef BLE_CFG_MAX_NBR_CB
#define BLE_CFG_MAX_NBR_CB       4
#endif

#endif /* __COMMON_BLESVC_H */
