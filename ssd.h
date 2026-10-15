#ifndef SSD_H
#define SSD_H

#include "stm32f446xx.h"
#include <stdint.h>

// Initialize GPIO and SSD hardware
void ssd_init(void);

// Convert a float value (0.00–99.99) into 4 digits for display
void ssd_display(float value);

// Refresh a single digit (0–3) on the SSD using multiplexing
void ssd_refresh_digit(uint8_t digit);

#endif
