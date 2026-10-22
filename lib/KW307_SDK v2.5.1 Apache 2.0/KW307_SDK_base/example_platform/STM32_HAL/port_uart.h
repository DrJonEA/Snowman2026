/**
 * KW307 SDK - STM32 HAL UART Platform Header
 * File: example_platform/STM32_HAL/port_uart.h
 *
 * Compile port_uart.c into your project to use this platform.
 * Edit user_config.h in this folder to point to your UART handle.
 *
 * ISR hookup:
 *   No user_isr.c is needed. port_uart.c provides a HAL weak-symbol
 *   override for HAL_UART_RxCpltCallback() that captures each received
 *   byte and re-arms the next 1-byte RX interrupt. The standard
 *   CubeMX-generated stm32xxxx_it.c already contains:
 *
 *       void USARTx_IRQHandler(void) { HAL_UART_IRQHandler(&huartx); }
 *
 *   which routes RX completion to our callback automatically.
 */

#ifndef KW307_PORT_UART_STM32_H
#define KW307_PORT_UART_STM32_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Push one received byte into the SDK's internal ring buffer.
 * Called from HAL_UART_RxCpltCallback() inside port_uart.c; advanced
 * users who bypass HAL callbacks can call this directly.
 */
void kw307_port_isr_feed_byte(uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* KW307_PORT_UART_STM32_H */
