#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
#include <stdint.h>
#define configTICK_RATE_HZ 1000
#define pdMS_TO_TICKS(ms) ((TickType_t) (ms))
typedef uint32_t TickType_t;
typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;
#define pdPASS (1)
#define pdFAIL (0)
typedef struct
{
    uint8_t *storage; // 测试队列静态存储
    UBaseType_t length; // 容量
    UBaseType_t item_size; // 元素字节数
    UBaseType_t count; // 元素数
} StaticQueue_t;
#endif
