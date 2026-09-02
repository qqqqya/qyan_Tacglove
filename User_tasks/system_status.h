/**
 * @file system_status.h
 * @brief 跨任务共享的轻量系统状态快照。
 */
#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 与LED业务状态机一致的系统状态。 */
typedef enum
{
    SYSTEM_STATE_SELF_TEST = 0,
    SYSTEM_STATE_IDLE = 1,
    SYSTEM_STATE_CAPTURE_PREPARING = 2,
    SYSTEM_STATE_CAPTURING = 3,
    SYSTEM_STATE_FAULT = 4
} system_state_t;

/** @brief 当前阶段使用的故障码。 */
typedef enum
{
    SYSTEM_FAULT_NONE = 0x0000,
    SYSTEM_FAULT_LED_OR_BEEP_INIT = 0x0101,
    SYSTEM_FAULT_SELF_TEST = 0x0102,
    SYSTEM_FAULT_CAPTURE_FLOW = 0x0201,
    SYSTEM_FAULT_COMMUNICATION_INIT = 0x0301
} system_fault_code_t;

typedef struct
{
    system_state_t state;
    uint16_t fault_code;
} system_status_snapshot_t;

/** @brief 原子更新当前系统状态和故障码。 */
void system_status_set(system_state_t state, uint16_t fault_code);

/** @brief 获取当前系统状态快照。 */
system_status_snapshot_t system_status_get(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_STATUS_H */
