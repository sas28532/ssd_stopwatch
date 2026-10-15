#include "ssd.h"

// Holds the 4 digits (0–9 or 10 = blank)
static uint8_t digits[4];

// Decimal point between digit 1 and 2 (xx.xx)
static uint8_t dp_index = 1;

// Segment bit patterns (a b c d e f g)
static const uint8_t digitSegments[] = {
    0b0111111, // 0
    0b0000110, // 1
    0b1011011, // 2
    0b1001111, // 3
    0b1100110, // 4
    0b1101101, // 5
    0b1111101, // 6
    0b0000111, // 7
    0b1111111, // 8
    0b1101111, // 9
    0b0000000  // blank
};

/********************************************************************
 * ssd_set_segments()
 * Drives the segment pins for ONE digit.
 * Segments are active‑LOW → writing 0 turns ON the LED.
 ********************************************************************/
static void ssd_set_segments(uint8_t pattern, uint8_t dp_on)
{
    // Turn everything OFF first (HIGH)
    GPIOG->BSRR = (1<<9) | (1<<14);          // A, D
    GPIOF->BSRR = (1<<12) | (1<<13) |
                  (1<<14) | (1<<15);         // B, C, DP, F
    GPIOE->BSRR = (1<<8);                    // E
    GPIOB->BSRR = (1<<4);                    // G

    // Turn ON segments that should be ON (LOW)
    if (pattern & (1<<0)) GPIOG->BSRR = (1<<(9+16));   // A
    if (pattern & (1<<1)) GPIOF->BSRR = (1<<(12+16));  // B
    if (pattern & (1<<2)) GPIOF->BSRR = (1<<(13+16));  // C
    if (pattern & (1<<3)) GPIOG->BSRR = (1<<(14+16));  // D
    if (pattern & (1<<4)) GPIOE->BSRR = (1<<(8+16));   // E
    if (pattern & (1<<5)) GPIOF->BSRR = (1<<(15+16));  // F
    if (pattern & (1<<6)) GPIOB->BSRR = (1<<(4+16));   // G

    // Decimal point (active‑LOW)
    if (dp_on)
        GPIOF->BSRR = (1<<(14+16));   // ON
    else
        GPIOF->BSRR = (1<<14);        // OFF
}

/********************************************************************
 * ssd_disable_all_digits()
 * Common‑anode → HIGH = OFF
 ********************************************************************/
static void ssd_disable_all_digits(void)
{
    GPIOE->BSRR = (1<<10) | (1<<7);      // PE10, PE7 OFF
    GPIOB->BSRR = (1<<5)  | (1<<3);      // PB5, PB3 OFF
}

/********************************************************************
 * ssd_enable_digit()
 * Common‑anode → LOW = ON
 ********************************************************************/
void ssd_enable_digit(uint8_t digit) {
    // Turn all OFF (HIGH = OFF)
    GPIOE->BSRR = (1 << 10) | (1 << 7);
    GPIOB->BSRR = (1 << 5) | (1 << 3);

    // Enable selected digit (LOW = ON)
    switch (digit) {
        case 0: GPIOE->BSRR = (1 << (10+16)); break; // PE10 LOW
        case 1: GPIOE->BSRR = (1 << (7+16));  break; // PE7 LOW
        case 2: GPIOB->BSRR = (1 << (5+16));  break; // PB5 LOW
        case 3: GPIOB->BSRR = (1 << (3+16));  break; // PB3 LOW
    }
}


/********************************************************************
 * ssd_init()
 * Configures all GPIO pins as outputs.
 ********************************************************************/
void ssd_init(void)
{
    // Enable GPIO clocks: B, E, F, G
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN |
                    RCC_AHB1ENR_GPIOEEN |
                    RCC_AHB1ENR_GPIOFEN |
                    RCC_AHB1ENR_GPIOGEN;

    // Enable system configuration controller (needed to modify pin functions)
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    // Disable SWO/JTAG function on PB3 so it can act as a normal GPIO
    // PB3 is TRACESWO by default → prevents digit 4 from turning on
    //SYSCFG->PMC |= SYSCFG_PMC_MII_RMII_SEL;

    // Configure digit pins as outputs
    GPIOE->MODER &= ~(GPIO_MODER_MODER10 | GPIO_MODER_MODER7);
    GPIOE->MODER |=  (GPIO_MODER_MODER10_0 | GPIO_MODER_MODER7_0);

    GPIOB->MODER &= ~(GPIO_MODER_MODER5 | GPIO_MODER_MODER3 | GPIO_MODER_MODER4);
    GPIOB->MODER |=  (GPIO_MODER_MODER5_0 | GPIO_MODER_MODER3_0 | GPIO_MODER_MODER4_0);

    // Configure segment pins as outputs
    GPIOG->MODER &= ~(GPIO_MODER_MODER9 | GPIO_MODER_MODER14);
    GPIOG->MODER |=  (GPIO_MODER_MODER9_0 | GPIO_MODER_MODER14_0);

    GPIOF->MODER &= ~(GPIO_MODER_MODER12 | GPIO_MODER_MODER13 |
                      GPIO_MODER_MODER14 | GPIO_MODER_MODER15);
    GPIOF->MODER |=  (GPIO_MODER_MODER12_0 | GPIO_MODER_MODER13_0 |
                      GPIO_MODER_MODER14_0 | GPIO_MODER_MODER15_0);

    GPIOE->MODER &= ~(GPIO_MODER_MODER8);
    GPIOE->MODER |=  (GPIO_MODER_MODER8_0);

    // Start with all digits off
    ssd_disable_all_digits();
}


/********************************************************************
 * ssd_display()
 * Converts float → 4 digits.
 ********************************************************************/
void ssd_display(float value)
{
    if (value < 0.0f) value = 0.0f;
    if (value > 99.99f) value = 99.99f;

    uint16_t v = (uint16_t)(value * 100 + 0.5f);

    digits[3] = v % 10;
    digits[2] = (v / 10) % 10;
    digits[1] = (v / 100) % 10;
    digits[0] = (v / 1000) % 10;

    if (value < 10.0f)
        digits[0] = 10; // blank
}

/********************************************************************
 * ssd_refresh_digit()
 * Called by TIM5 at 100 Hz → multiplexing
 ********************************************************************/
void ssd_refresh_digit(uint8_t d)
{
    uint8_t idx = digits[d];
    uint8_t pattern = digitSegments[idx];

    uint8_t dp_on = (d == dp_index);

    ssd_set_segments(pattern, dp_on);
    ssd_enable_digit(d);

}

/********************************************************************************************
GENERAL CONCEPTS (NO CODE ALLOWED FOR THESE QUESTIONS)
------------------------------------------------------

1. Why do you need to enable the clock for GPIOA/B/C (or any GPIO port)?
   - The microcontroller turns off clocks to unused hardware to save power.
   - A GPIO port literally does nothing until its clock is enabled.
   - Enabling the clock “wakes up” that port so you can configure pins, read buttons,
     or drive the SSD segments.

2. Why can't all the digits be on at the same time? How do we solve that problem?
   - Our SSD is multiplexed: all digits share the same segment lines (A–G, DP).
   - If all digits turned on at once, every digit would show the exact same pattern.
   - We solve this by turning on ONE digit at a time, very fast (hundreds of times per second).
   - Persistence of vision makes it look like all digits are lit continuously.

3. Explain to a non-engineer, in plain English, what your program does and how it does it.
   - The program runs a stopwatch on a 4‑digit display.
   - It keeps track of time, updates the display, and reacts to button presses.
   - A timer rapidly switches between digits so the display looks stable.
   - Another timer updates the stopwatch time.
   - Interrupts let the program react instantly when a button is pressed.

4. What is a helper file, and why are we using one in this project?
   - A helper file is a separate C file that handles one specific job.
   - Here, “ssd.c” handles all the display logic so the main program stays clean.
   - It keeps the project organized and makes debugging easier.

5. What is a timer interrupt, and why do we use one?
   - A timer interrupt is a hardware event that fires at a precise rate.
   - We use SysTick to update the stopwatch time every 10 ms.
   - We use TIM5 to refresh the SSD digits at a stable frequency.
   - This keeps the display smooth and the timing accurate.

6. How does your code know which button was pressed?
   - Each button is wired to a different EXTI line (external interrupt line).
   - When a button rises from LOW to HIGH, its EXTI line triggers an interrupt.
   - Inside the interrupt handler, we check which EXTI flag is set.
   - That tells us exactly which button caused the interrupt.

------------------------------------------------------
CODE-SPECIFIC QUESTIONS (YOU *CAN* LOOK AT YOUR CODE)
------------------------------------------------------

7. Which timers are used in your program, and how did you set up each one?
   - SysTick: configured for 100 Hz (10 ms). It increments a counter used to update time.
   - TIM5: configured for ~100 Hz refresh. It cycles through digits 0–3 to multiplex the SSD.

8. Walk through how your code updates the display.
   - ssd_display() converts the float time into four digit values.
   - TIM5 interrupt calls ssd_refresh_digit(), which:
       • selects the correct digit,
       • sets the correct segment pattern,
       • turns on only that digit.
   - This repeats for digits 0,1,2,3 in a loop.

9. Which variable(s) are volatile and why?
   - state, current_time, systick_ticks, current_digit.
   - They are volatile because they are changed inside interrupts.
   - Volatile prevents the compiler from optimizing them incorrectly.

10. Why do interrupt flags need to be cleared?
   - If you don’t clear the flag, the interrupt keeps firing forever.
   - Clearing the flag tells the hardware “I handled this event.”

11. How does your code turn the time into the digits on the display?
   - It multiplies the float time by 100 to convert to an integer.
   - Then it extracts each digit using modulo and division:
       • thousands → digit[0]
       • hundreds  → digit[1]
       • tens      → digit[2]
       • ones      → digit[3]
   - It also blanks the leading digit when the value is < 10.00.

------------------------------------------------------
ADDITIONAL PRESENTATION NOTES (OPTIONAL BUT USEFUL)
------------------------------------------------------

• Multiplexing is the key idea: one digit at a time, very fast.
• Interrupts let the program react instantly instead of constantly checking buttons.
• Helper files keep the project modular and readable.
• Volatile variables protect against compiler optimization errors.
• Timers provide precise timing without wasting CPU cycles.
• The SSD uses active‑low logic: writing LOW turns a segment ON.

********************************************************************************************/
