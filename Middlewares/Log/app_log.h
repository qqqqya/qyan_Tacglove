/**
 * @file app_log.h
 * @brief 可裁剪的应用日志接口。
 * @details APP_LOG_ENABLE为0时所有日志在预处理阶段移除，不占Flash、RAM和栈。
 */
#ifndef APP_LOG_H
#define APP_LOG_H

#ifndef APP_LOG_ENABLE
#define APP_LOG_ENABLE 0
#endif

#if APP_LOG_ENABLE

/** @brief 输出INFO级日志；要求工程已经实现printf底层输出。 */
void app_log_info(const char *format, ...);

/** @brief 输出ERROR级日志；要求工程已经实现printf底层输出。 */
void app_log_error(const char *format, ...);

#define APP_LOG_INFO(...)  app_log_info(__VA_ARGS__)
#define APP_LOG_ERROR(...) app_log_error(__VA_ARGS__)

#else

#define APP_LOG_INFO(...)  ((void)0)
#define APP_LOG_ERROR(...) ((void)0)

#endif

#endif /* APP_LOG_H */
