/**
 * KW307 SDK - Platform Port Template
 * File: port_template/port_template.c
 *
 * Skeleton for porting the SDK to a new platform. Fill in the 5 functions
 * below; the upper SDK layers (KW307_uart.c / KW307_api.c) stay unchanged
 * and call these through the common API defined in core/KW307_port.h.
 *
 * STEPS
 *   1. Copy this file to a new folder, e.g.  mcu/<yourboard>/port_<yourboard>.c
 *   2. Create user_config.h in the same folder with any macros you need
 *      (UART peripheral handle, baud rate, pin numbers, etc.).
 *   3. Implement the TODO sections below. Keep the function signatures
 *      exactly as declared in core/KW307_port.h.
 *   4. Add your new .c to the build and add the folder to the include path.
 *      No other SDK files need changes.
 *
 * REFERENCE IMPLEMENTATIONS (good starting points to read):
 *   example_platform/Nuvoton_M251/port_uart.c               - MCU, ISR-driven ring buffer
 *   example_platform/Arduino_ESP32S3/port_arduino.cpp - Arduino HardwareSerial
 *   pc/windows/port_win32.c            - Windows Win32 COM
 */

#include <stdint.h>
#include "KW307_port.h"      /* add KW307_SDK_v2.3/core to include path */
#include "KW307_uart.h"
#include "user_config.h"     /* your per-project macros */
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "stdio.h"

/* ================================================================
   Internal state (example - adjust to your platform)
   ================================================================ */
static uint8_t s_is_open = 0;
/* TODO: add handle / ring buffer / anything else your platform needs */


extern KW307Uart_t* kw307_default_uart(void);


// RX interrupt handler
void kw307_port_on_uart_rx() {
    while (uart_is_readable(KW307_UART_PERIPH)) {
        uint8_t ch = uart_getc(KW307_UART_PERIPH);
        kw307_uart_feed_byte(kw307_default_uart(),  ch);


    }
}

/* ================================================================
   1. kw307_port_open
      Called once by kw307_init(). Configure and open the UART.
      port_name : ignored on MCU (use user_config.h macros instead).
                  PC : "COM3" / "/dev/ttyUSB0" - use if non-NULL.
      baud_rate : typically 115200; 0 means "use user_config.h default".
      Returns   : 0 on success, negative on error.
   ================================================================ */
int kw307_port_open(const char* port_name, int baud_rate)
{
    (void)port_name;
    (void)baud_rate;

    int baud = baud_rate;
    if (baud == 0){
    	baud = KW307_UART_BAUD;
    }
   // if (KW307_UART_PERIPH == uart1 )
   // 	printf("Uart Init: %s at %d\n", "uart1", baud);
    uart_init(KW307_UART_PERIPH, baud);
	gpio_set_function(
			KW307_UART_TX_PIN_MFP,
			UART_FUNCSEL_NUM(KW307_UART_PERIPH, KW307_UART_TX_PIN_MFP));
	gpio_set_function(
			KW307_UART_RX_PIN_MFP,
			UART_FUNCSEL_NUM(KW307_UART_PERIPH, KW307_UART_RX_PIN_MFP));

	int UART_IRQ = KW307_UART_PERIPH == uart0 ? UART0_IRQ : UART1_IRQ;
	 // Set up and enable the interrupt handlers
	irq_set_exclusive_handler(UART_IRQ, kw307_port_on_uart_rx);
	irq_set_enabled(UART_IRQ, true);

	// Eenable the UART to send interrupts - RX only
	uart_set_irq_enables(KW307_UART_PERIPH, true, false);

    s_is_open = 1;
    return 0;
}


/* ================================================================
   2. kw307_port_close
      Disable the UART and release resources.
   ================================================================ */
void kw307_port_close(void)
{
    /* : disable RX interrupt, close peripheral */
    s_is_open = 0;
}


/* ================================================================
   3. kw307_port_send
      Transmit `len` bytes. Blocking TX is fine.
      Returns bytes sent (>=0), or negative on error.
   ================================================================ */
int kw307_port_send(const uint8_t* buf, int len)
{
   //printf("Uart send(%d)\n", len);
	if (!s_is_open || len <= 0 || !buf) return -1;

    uart_write_blocking (
    		KW307_UART_PERIPH,
    		buf,
    		len);

    return len;
}


/* ================================================================
   4. kw307_port_recv
      timeout_ms = 0 : non-blocking, return whatever is available now.
      timeout_ms > 0 : wait up to timeout_ms ms for at least 1 byte.
      Returns bytes received (0 = timeout), negative on error.
   ================================================================ */
int kw307_port_recv(uint8_t* buf, int max_len, int timeout_ms)
{
    if (!s_is_open || max_len <= 0 || !buf) return -1;
    int r = 0;

    if (uart_is_readable_within_us (KW307_UART_PERIPH,  timeout_ms * 1000)){
    	while (uart_is_readable (KW307_UART_PERIPH)){
    		buf[r] = (uint8_t)  uart_getc (KW307_UART_PERIPH);
			r++;
			if ( r >= max_len){
				break;
			}
    	}
    }
    //if (r > 0) printf("Uart read %d\n", r);
    return r;
}


/* ================================================================
   5. kw307_port_is_open
      Returns non-zero if the port is currently open.
   ================================================================ */
int kw307_port_is_open(void)
{
    return s_is_open ? 1 : 0;
}


/* ================================================================
   OPTIONAL - ISR hook (MCU only)
   ----------------------------------------------------------------
   If you use ISR-driven RX, expose a feed function here and call it
   from your platform's UART interrupt handler. See example_platform/Nuvoton_M251/port_uart.c
   + example_platform/Nuvoton_M251/user_isr.c for the M251 reference.

   void kw307_port_isr_feed(void)
   {
       uint8_t b = UART_READ(...);
       // push b into your ring buffer
   }
   ================================================================ */
