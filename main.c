/****************************************************************
* Author: S. SHAFI
* CPEG222 Project 3: Stopwatch on External SSD
* Date: 9/28/26
*
* Description:
*   - PAUSE mode: stopwatch holds current value
*   - COUNT UP / COUNT DOWN time changes in 0.01 s steps
*   - 4-digit SSD shows XX.XX (0.00 to 99.99)
*   - Buttons on PE5/PE6, PF7/PF8/PF9 control state and reset
*   - SysTick: time base (125 Hz)
*   - TIM5: SSD multiplex refresh
****************************************************************/

#include "stm32f446xx.h"
#include "ssd.h"

typedef enum {
    STATE_PAUSE = 0,
    STATE_COUNT_UP,
    STATE_COUNT_DOWN
} State;

// Global state variables
volatile State state = STATE_PAUSE;      // current stopwatch mode
volatile float current_time = 0.00f;     // time in seconds (0.00–99.99)
volatile uint32_t systick_ticks = 0;     // SysTick tick counter
volatile uint8_t current_digit = 0;      // which SSD digit is active (0–3)

// Button pins (EXTI line numbers)
#define LEFT_BUTTON_PIN   9   // PF9 → EXTI9
#define CENTER_BUTTON_PIN 8   // PF8 → EXTI8
#define RIGHT_BUTTON_PIN  6   // PE6 → EXTI6
#define UP_BUTTON_PIN     7   // PF7 → EXTI7
#define DOWN_BUTTON_PIN   5   // PE5 → EXTI5

/******************** SysTick (10 ms) ********************/

void SysTick_Handler(void)
{
    // Increment global tick counter every 10 ms
    systick_ticks++;
}

static void systick_init(void)
{
    // Configure SysTick for 10 ms interrupts (100 Hz)
    // SystemCoreClock / 100 → 10 ms period
    SysTick_Config(SystemCoreClock / 100);

    // Set SysTick interrupt priority (lower number = higher priority)
    NVIC_SetPriority(SysTick_IRQn, 1);
}

/******************** Time update logic ********************/

static void update_time(void)
{
    // Remember last tick value to detect changes
    static uint32_t last = 0;

    // Only update when a new SysTick has occurred
    if (systick_ticks != last)
    {
        last = systick_ticks;

        // COUNT UP: increase by 0.01 s, clamp at 99.99
        if (state == STATE_COUNT_UP && current_time < 99.99f)
        {
            current_time += 0.01f;
            if (current_time > 99.99f) current_time = 99.99f;
        }
        // COUNT DOWN: decrease by 0.01 s, clamp at 0.00
        else if (state == STATE_COUNT_DOWN && current_time > 0.00f)
        {
            current_time -= 0.01f;
            if (current_time < 0.00f) current_time = 0.00f;
        }
    }
}

/******************** TIM5: SSD multiplexing ********************/

void TIM5_IRQHandler(void)
{
    // Check update interrupt flag
    if (TIM5->SR & TIM_SR_UIF)
    {
        // Clear flag
        TIM5->SR &= ~TIM_SR_UIF;

        // Refresh current digit on SSD
        ssd_refresh_digit(current_digit);

        // Move to next digit (0–3, wrap with & 0x03)
        current_digit = (current_digit + 1) & 0x03;
    }
}

static void tim5_init(void)
{
    // Enable TIM5 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;

    // TIM5 clock = 16 MHz
    // Prescaler: 16000 → 16 MHz / 16000 = 1 kHz
    TIM5->PSC = 8000 - 1;

    // Auto-reload: 10 → 1 kHz / 10 = 100 Hz (10 ms)
    TIM5->ARR = 4 - 1;

    // Enable update interrupt
    TIM5->DIER |= TIM_DIER_UIE;

    // Enable counter
    TIM5->CR1  |= TIM_CR1_CEN;

    // Enable TIM5 interrupt in NVIC
    NVIC_EnableIRQ(TIM5_IRQn);
    NVIC_SetPriority(TIM5_IRQn, 2);
}

/******************** EXTI button interrupts ********************/

void EXTI9_5_IRQHandler(void)
{
    // Read pending interrupt bits
    uint32_t pending = EXTI->PR;

    // DOWN → PE5 → EXTI5: start COUNT DOWN
    if (pending & (1 << DOWN_BUTTON_PIN))
    {
        EXTI->PR = (1 << DOWN_BUTTON_PIN);  // clear pending
        state = STATE_COUNT_DOWN;
    }

    // RIGHT → PE6 → EXTI6: reset to 99.99 and PAUSE
    if (pending & (1 << RIGHT_BUTTON_PIN))
    {
        EXTI->PR = (1 << RIGHT_BUTTON_PIN);
        current_time = 99.99f;
        state = STATE_PAUSE;
    }

    // UP → PF7 → EXTI7: start COUNT UP
    if (pending & (1 << UP_BUTTON_PIN))
    {
        EXTI->PR = (1 << UP_BUTTON_PIN);
        state = STATE_COUNT_UP;
    }

    // CENTER → PF8 → EXTI8: PAUSE (hold current time)
    if (pending & (1 << CENTER_BUTTON_PIN))
    {
        EXTI->PR = (1 << CENTER_BUTTON_PIN);
        state = STATE_PAUSE;
    }

    // LEFT → PF9 → EXTI9: reset to 0.00 and PAUSE
    if (pending & (1 << LEFT_BUTTON_PIN))
    {
        EXTI->PR = (1 << LEFT_BUTTON_PIN);
        current_time = 0.00f;
        state = STATE_PAUSE;
    }
}

static void buttons_init(void)
{
    // Enable GPIOE, GPIOF, and SYSCFG clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN | RCC_AHB1ENR_GPIOFEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    // Configure button pins as inputs (default mode = 00)
    GPIOE->MODER &= ~(GPIO_MODER_MODER5 | GPIO_MODER_MODER6);
    GPIOF->MODER &= ~(GPIO_MODER_MODER7 | GPIO_MODER_MODER8 | GPIO_MODER_MODER9);

    // Optional: enable pull-ups if buttons are active-low
    // GPIOE->PUPDR |= (1 << (5 * 2)) | (1 << (6 * 2)); // PE5, PE6 pull-up
    // GPIOF->PUPDR |= (1 << (7 * 2)) | (1 << (8 * 2)) | (1 << (9 * 2)); // PF7–PF9 pull-up

    // Map pins to EXTI lines
    // EXTICR2: lines 4–7 (we use 5,6,7)
    SYSCFG->EXTICR[1] &= ~(SYSCFG_EXTICR2_EXTI5 |
                           SYSCFG_EXTICR2_EXTI6 |
                           SYSCFG_EXTICR2_EXTI7);
    SYSCFG->EXTICR[1] |=  (SYSCFG_EXTICR2_EXTI5_PE |   // EXTI5 ← PE5
                           SYSCFG_EXTICR2_EXTI6_PE |   // EXTI6 ← PE6
                           SYSCFG_EXTICR2_EXTI7_PF);   // EXTI7 ← PF7

    // EXTICR3: lines 8–11 (we use 8,9)
    SYSCFG->EXTICR[2] &= ~(SYSCFG_EXTICR3_EXTI8 |
                           SYSCFG_EXTICR3_EXTI9);
    SYSCFG->EXTICR[2] |=  (SYSCFG_EXTICR3_EXTI8_PF |   // EXTI8 ← PF8
                           SYSCFG_EXTICR3_EXTI9_PF);   // EXTI9 ← PF9

    // Configure edge triggers
    // If buttons are wired active-low with pull-ups, use falling edge:
    // EXTI->FTSR |= (1 << DOWN_BUTTON_PIN)  |
    //              (1 << RIGHT_BUTTON_PIN) |
    //              (1 << UP_BUTTON_PIN)    |
    //              (1 << CENTER_BUTTON_PIN)|
    //              (1 << LEFT_BUTTON_PIN);

    // If buttons are active-high, use rising edge:
    EXTI->RTSR |= (1 << DOWN_BUTTON_PIN)  |
                  (1 << RIGHT_BUTTON_PIN) |
                  (1 << UP_BUTTON_PIN)    |
                  (1 << CENTER_BUTTON_PIN)|
                  (1 << LEFT_BUTTON_PIN);

    // Unmask EXTI lines (enable interrupts)
    EXTI->IMR |= (1 << DOWN_BUTTON_PIN)  |
                 (1 << RIGHT_BUTTON_PIN) |
                 (1 << UP_BUTTON_PIN)    |
                 (1 << CENTER_BUTTON_PIN)|
                 (1 << LEFT_BUTTON_PIN);

    // Enable EXTI9_5 interrupt in NVIC
    NVIC_EnableIRQ(EXTI9_5_IRQn);
    NVIC_SetPriority(EXTI9_5_IRQn, 3);
}

/******************** main ********************/

int main(void)
{
    // Initial state: paused at 0.00
    state = STATE_PAUSE;
    current_time = 0.00f;

    // Initialize SSD, SysTick, TIM5, and buttons
    ssd_init();
    systick_init();
    tim5_init();
    buttons_init();

    // Main loop: continuously update display and time
    while (1) {
        update_time();          // compute new time first
        ssd_display(current_time); // then convert to digits
    }

}
