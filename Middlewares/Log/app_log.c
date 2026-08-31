/**
 * @file app_log.c
 * @brief 应用日志格式化实现。
 */
#include "app_log.h"

#if APP_LOG_ENABLE

#include <stdarg.h>
#include <stdio.h>

/** @brief 输出一条带级别前缀的日志。 */
static void app_log_vprint(const char *level, const char *format, va_list arguments)
{
    (void)printf("[%s] ", level);
    (void)vprintf(format, arguments);
    (void)printf("\r\n");
}

void app_log_info(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    app_log_vprint("I", format, arguments);
    va_end(arguments);
}

void app_log_error(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    app_log_vprint("E", format, arguments);
    va_end(arguments);
}

#endif /* APP_LOG_ENABLE */
