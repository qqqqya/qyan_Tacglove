/**
 * @file task_status.h
 * @brief 应用任务层统一返回状态定义。
 */
#ifndef TASK_STATUS_H
#define TASK_STATUS_H

/** @brief 任务创建和任务管理函数的返回状态。 */
typedef enum
{
    TASK_OK              = 0,    /**< Operation completed successfully. */
    TASK_ERROR           = 1,    /**< General runtime error. */
    TASK_ERROR_TIMEOUT   = 2,    /**< Operation timed out. */
    TASK_ERROR_RESOURCE  = 3,    /**< Required resource is unavailable. */
    TASK_ERROR_PARAMETER = 4,    /**< Invalid parameter. */
    TASK_ERROR_NO_MEMORY = 5,    /**< Memory allocation failed. */
    TASK_ERROR_ISR       = 6,    /**< Operation is not allowed in ISR context. */
    TASK_RESERVED        = 0xFF  /**< Reserved status. */
} task_status_t;

#endif /* TASK_STATUS_H */
