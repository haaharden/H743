#ifndef LOG_H
#define LOG_H

#include  "SEGGER_RTT.h"

#define LOGI(fmt, ...)                                      \
    do {                                                    \
        SEGGER_RTT_printf(0, "[I] " fmt "\r\n", ##__VA_ARGS__); \
    } while (0)

#define LOGW(fmt, ...)                                      \
    do {                                                    \
        SEGGER_RTT_printf(0, "[W] " fmt "\r\n", ##__VA_ARGS__); \
    } while (0)

#define LOGE(fmt, ...)                                      \
    do {                                                    \
        SEGGER_RTT_printf(0, "[E] " fmt "\r\n", ##__VA_ARGS__); \
    } while (0)

#define LOGD(fmt, ...)                                      \
    do {                                                    \
        SEGGER_RTT_printf(0, "[D] " fmt "\r\n", ##__VA_ARGS__); \
    } while (0)

#endif /* LOG_H */