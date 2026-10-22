/*
 * Star.cpp
 */

#include "Star.h"

Star::Star(PIO pio, uint sm, uint gpio, uint8_t brightness) :
		xLeds(PicoLed::addLeds<PicoLed::WS2812B>(pio, sm, gpio, STAR_LED_LEN, PicoLed::FORMAT_GRB)) {
	xLeds.setBrightness(brightness);
	xLeds.clear();
	xLeds.show();
}

Star::~Star() {
}

void Star::setCentre(PicoLed::Color c) {
	xLeds.setPixelColor(0, c);
}

void Star::setRing(uint index, PicoLed::Color c) {
	xLeds.setPixelColor(1 + (index % STAR_RING_LEN), c);
}

void Star::fill(PicoLed::Color c) {
	xLeds.fill(c);
}

void Star::clear() {
	xLeds.clear();
}

void Star::show() {
	xLeds.show();
}

PicoLed::Color Star::scale(PicoLed::Color c, uint8_t level) {
	return PicoLed::RGB(
			(uint8_t)(((uint)c.red * level) / 255),
			(uint8_t)(((uint)c.green * level) / 255),
			(uint8_t)(((uint)c.blue * level) / 255));
}
