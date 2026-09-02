/**
 * @file system_status.c
 * @brief 系统状态快照实现。
 */
#include "system_status.h"

/** @brief Cortex-M0可原子访问的32位打包状态：低8位状态，高16位故障码。 */
static volatile uint32_t s_packed_status = SYSTEM_STATE_SELF_TEST;

void system_status_set(system_state_t state, uint16_t fault_code)
{
    s_packed_status = ((uint32_t)fault_code << 16U) |
        ((uint32_t)state & 0xFFU);
}

system_status_snapshot_t system_status_get(void)
{
    const uint32_t packed = s_packed_status;
    const system_status_snapshot_t snapshot = {
        .state = (system_state_t)(packed & 0xFFU),
        .fault_code = (uint16_t)(packed >> 16U)};
    return snapshot;
}
