/**
 * @file os_hooks.c
 * @brief FreeRTOS内存耗尽和任务栈溢出的致命故障钩子。
 */
#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief FreeRTOS动态内存申请失败钩子。
 * @details 关闭中断并停机，连接调试器后可在本函数断点定位内存不足。
 */
void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}

/**
 * @brief FreeRTOS任务栈溢出钩子。
 * @param task 发生栈溢出的任务句柄。
 * @param task_name 发生栈溢出的任务名。
 * @details 关闭中断并保留现场，不尝试从栈破坏中继续运行。
 */
void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;
    (void)task_name;

    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}
