#ifndef LOG_OUTPUT_H_
#define LOG_OUTPUT_H_

#include "config.h"

#if defined(DEBUG_SERIAL_USART2) || defined(DEBUG_SERIAL_USART3)
#define printf_log(...) printf(__VA_ARGS__) // Define printf_log as printf when debugging
#else
#define printf_log(...) ((void)0) // Define printf_log as a no-op when not debugging
#endif

#endif // !LOG_OUTPUT_H_