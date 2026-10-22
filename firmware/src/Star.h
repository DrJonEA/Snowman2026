/*
 * Star.h
 *
 * 7 LED WS2812B disk. LED 0 is the centre, LEDs 1-6 form the outer ring.
 */

#ifndef EXP_DEMOMOVEMENT_SRC_STAR_H_
#define EXP_DEMOMOVEMENT_SRC_STAR_H_

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "PicoLed.hpp"

#define STAR_LED_LEN   7
#define STAR_RING_LEN  6

class Star {
public:
	Star(PIO pio, uint sm, uint gpio, uint8_t brightness = 30);
	virtual ~Star();

	void setCentre(PicoLed::Color c);
	// index 0..5 around the ring
	void setRing(uint index, PicoLed::Color c);
	void fill(PicoLed::Color c);
	void clear();
	void show();

	// Scale a colour by 0..255
	static PicoLed::Color scale(PicoLed::Color c, uint8_t level);

private:
	PicoLed::PicoLedController xLeds;
};

#endif /* EXP_DEMOMOVEMENT_SRC_STAR_H_ */
