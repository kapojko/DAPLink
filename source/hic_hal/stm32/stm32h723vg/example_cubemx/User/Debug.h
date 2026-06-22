#pragma once

#if defined(DEBUG) && (defined(TRANSMIT_DEBUG) || !defined(TRANSMIT_UART))
#define PRINT(format, ...) DebugPrint(format, ##__VA_ARGS__)
#else
#define PRINT(...)
#endif

void DebugPrint(const char *fmt, ...);

void DelayMsFunc(int ms);
void DelayUsFunc(int us);
