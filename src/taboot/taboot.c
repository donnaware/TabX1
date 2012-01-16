//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//  taboot Firmware
//  This code will initialize an FPGA from SPI Flash Ram
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Compilation Swithces
//------------------------------------------------------------------------------
#define DEBUG     1     // Suppress debug messages by setting to 0
#define FLASHRAM  1     // Set to 1 to include Flash RAM routines
//------------------------------------------------------------------------------
#include "taboot.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Main Program
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void main(void)
{
    setup_adc_ports(NO_ANALOGS|VSS_VDD);        // No ADC needed
    setup_adc(ADC_OFF|ADC_TAD_MUL_0);           // No ADC needed
    setup_vref(FALSE);                          // V ref not needed
    setup_timer_0(RTCC_INTERNAL|RTCC_DIV_256);  // Nominal timing
    setup_timer_1(T1_DISABLED);                 // Not needed
    setup_timer_2(T2_DIV_BY_4,64,1);            // 20Mhz/4/4/64 = 19,531Hz

    Set_Tris_A(0b11110001);           // Port A, All Digital
    Set_Tris_B(0b11110101);           // Port B,  All Digital
    Set_Tris_C(0b10010010);           // Set up for RS232 & SPI
    Port_B_Pullups(True);             // Turn on port B pull ups

    Clr(TESTLED);                     // Turn off status LED
    Clr(BACKLITE);                    // Turn off status LCD
    Set(FPGALoad);                    // FPGA Upload pin, not loading

    but_state = 0;                    // initial button state is off

    Setup_ccp1(CCP_PWM);              // Configure CCP1 as a PWM
    Set_pwm1_duty(0x00);              // Backlight off

    delay_ms(100);                    // small delay

    verbose = read_eeprom(INIT_VERB); // Get verbosity level
    
    if(read_eeprom(INIT_RESET) == 0x01) ResetSystem(); // System Reset flag
    else                                Output_High(SysReset);

    Set(TESTLED);                   // Turn On status LED
    #if FLASHRAM                    // If Flash RAM support code on
        Try_Init_Flash(5);          // Try to init 5 times
        delay_ms(400);              // small delay
        InitFPGA(INIT_RBF);         // Initial load of FPGA
    #endif                          // End if

    if(read_eeprom(INIT_PWM) != 0xFF) Set_pwm1_duty(read_eeprom(INIT_PWM));
    else                              Set_pwm1_duty(0x00);  // Backlight off

    Clr(TESTLED);                   // Turn off status LED

    if(verbose) helpmenu();         // print help menu
    printf(">");                    // Print prompt
    do {
        if(kbhit()) {
            proc_cmd(getch());      // Process command byte
            printf("\r\n> ");       // Print carriage return and line feed
        }
        else {
            check_button();         // check button press
        }
    } while(TRUE);
}
//------------------------------------------------------------------------------
// End.c
//------------------------------------------------------------------------------



