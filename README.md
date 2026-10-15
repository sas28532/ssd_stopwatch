# ssd_stopwatch
Implements a bare metal CMSIS PlatformIO project of a standard stopwatch that displays elapsed time in hundredths of a second on a four element SSD array on a STM32 Nucleo-F446ZE board.

 Description:
   - PAUSE mode: stopwatch holds current value
   - COUNT UP / COUNT DOWN time changes in 0.01 s steps
   - 4-digit SSD shows XX.XX (0.00 to 99.99)
   - Buttons on PE5/PE6, PF7/PF8/PF9 control state and reset
   - SysTick: time base (125 Hz)
   - TIM5: SSD multiplex refresh
