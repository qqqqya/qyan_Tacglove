/**
 * @file beep_task.h
 * @brief 蜂鸣器应用任务创建接口。
 */
#ifndef BEEP_TASK_H
#define BEEP_TASK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建蜂鸣器上电自检任务。
 * @retval true FreeRTOS任务创建成功。
 * @retval false FreeRTOS heap不足或任务创建失败。
 */
bool beep_task_create(void);

#ifdef __cplusplus
}
#endif

#endif /* BEEP_TASK_H */
