/**
 * @file FreeRTOSConfig.h
 * @brief STM32F042K6T6（Cortex-M0，6 KB SRAM）的FreeRTOS配置。
 * @details SysTick专供FreeRTOS，STM32 HAL的1 ms时基由TIM1提供。
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

extern uint32_t SystemCoreClock;

/* 调度器与时钟：CPU 48 MHz，系统节拍1 kHz。 */
#define configCPU_CLOCK_HZ                      (SystemCoreClock)
#define configTICK_RATE_HZ                      1000U
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                64U
#define configMAX_TASK_NAME_LEN                 12
#define configIDLE_SHOULD_YIELD                 1

/* 保留任务、队列和互斥量，关闭当前阶段未使用的内核组件。 */
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configUSE_QUEUE_SETS                    0
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_NEWLIB_REENTRANT              0
#define configUSE_CO_ROUTINES                   0
#define configUSE_TIMERS                        0
#define configUSE_EVENT_GROUPS                  0
#define configUSE_STREAM_BUFFERS                0
#define configENABLE_MPU                        0
#define configENABLE_FPU                        0
#define configENABLE_MVE                        0

/* heap_4为任务控制块、任务栈和本工程的事件队列提供动态内存。 */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   2048U
#define configAPPLICATION_ALLOCATED_HEAP        0

/* 内存申请失败或任务栈溢出时进入os_hooks.c并停机保留现场。 */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configCHECK_HANDLER_INSTALLATION        0

#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_vTaskDelete                     0

/* Cortex-M0只有4级中断优先级，FreeRTOS内核异常使用最低优先级。 */
#define configKERNEL_INTERRUPT_PRIORITY         3U

/** @brief 断言失败后关闭中断并停机，保留现场供调试器检查。 */
#define configASSERT(expression)                    \
    do                                              \
    {                                               \
        if ((expression) == 0)                      \
        {                                           \
            __asm volatile ("cpsid i" ::: "memory"); \
            for (;;)                                \
            {                                       \
            }                                       \
        }                                           \
    } while (0)

/* 将FreeRTOS Cortex-M0 port异常函数绑定到启动文件向量名。 */
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
