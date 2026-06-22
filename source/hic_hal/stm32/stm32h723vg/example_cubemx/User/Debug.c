#include <stdarg.h>
#include <stdio.h>
#include "main.h"
#include "Debug.h"

#define SYSTICK_LOAD (SystemCoreClock / 1000000U)
#define SYSTICK_DELAY_CALIB (SYSTICK_LOAD >> 1)

extern UART_HandleTypeDef huart2;

#ifdef DEBUG
int _write(int file, char *ptr, int len) {
    (void)file; // Unused
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, 1000);
    return len;
}
#endif

void DebugPrint(const char *fmt, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
#endif
}

void DelayMsFunc(int ms) {
    DelayUsFunc(ms * 1000);
}

void DelayUsFunc(int us) {
    uint32_t start = SysTick->VAL;
    uint32_t ticks = (us * SYSTICK_LOAD) - SYSTICK_DELAY_CALIB;
    while ((start - SysTick->VAL) < ticks)
        ;
}
