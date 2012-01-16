//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// taboot Driver code                                         
// DonnaWare International LLP Copyright (2001) All Rights Reserved        
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include <18F2520.h>               // Proceesor definitions
#device  *=16 adc=8                // Use 16 bit pointers, Use 8 bit ADC values
#use     delay(clock=20000000)     // Set device crystal speed
//------------------------------------------------------------------------------
// Set up microcontroller fuses
// (These are ignored when used with bootloader)
//------------------------------------------------------------------------------
#fuses  NOWDT, HS, NOPUT, NOPROTECT, NOBROWNOUT, NOCPD, NOWRT, NODEBUG

//------------------------------------------------------------------------------
//  Set up RS232 interface
//------------------------------------------------------------------------------
#use rs232(baud=230400,bits=8,parity=N,xmit=PIN_C6,rcv=PIN_C7)
//#use rs232(baud=115200,bits=8,parity=N,xmit=PIN_C6,rcv=PIN_C7)

//------------------------------------------------------------------------------
//  Use fast I/O for all Ports
//------------------------------------------------------------------------------
#use fast_io(A)      // Use Fast I/O for Port A
#use fast_io(B)      // Use Fast I/O for Port B
#use fast_io(C)      // Use Fast I/O for Port C

//------------------------------------------------------------------------------
//    I/O Definitions
//------------------------------------------------------------------------------
#define Spare_A0     PIN_A0          // Spare Pin
#define FPGAClock    PIN_A1          // FPGA Serial upload clock line
#define FPGADOut     PIN_A2          // FPGA Serial upload data line
#define FPGALoad     PIN_A3          // FPGA Serial upload select line
#define Spare_A4     PIN_A4          // Spare Pin
#define PS5V_EN      PIN_A5          // 5V PS enable

#define Button       PIN_B0          // On Off Button Pin
#define TestLED      PIN_B1          // Test LED (Active high)
#define Spare_B2     PIN_B2          // Spare Pin
#define SysReset     PIN_B3          // Resets FPGA and Main CPU
#define Spare_B4     PIN_B4          // Spare Pin
#define Spare_B5     PIN_B5          // Spare Pin
#define Spare_B6     PIN_B6          // Spare Pin
#define Spare_B7     PIN_B7          // Spare Pin

#define SSEL         PIN_C0          // SPI Enable
#define Spare_C1     PIN_C1          // Spare Pin
#define BACKLITE     PIN_C2          // LCD Backlight PWM control
#define SCK          PIN_C3          // SPI Clock
#define MISO         PIN_C4          // SPI Input - Device to MCU
#define MOSI         PIN_C5          // SPI Output - MCU to Device
#define RS232_Tx     PIN_C6          // RS2323 pin
#define RS232_Rx     PIN_C7          // RS2323 pin

//------------------------------------------------------------------------------
//    Standard includes
//------------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//------------------------------------------------------------------------------
//   Macros
//------------------------------------------------------------------------------
#define HIGH_BYTE(var)  make8(var,1)     // Extracts high byte
#define LOW_BYTE(var)   make8(var,0)     // Extracts low byte
#define delay_ns(val)   delay_cycles(val)  // for 20Mhz xtal, 1 = 200ns (Same as a NOP)
#define set(var)        output_high(var)
#define clr(var)        output_low(var)
#define get(var)        input(var)

//------------------------------------------------------------------------------
//   Global Variables
//------------------------------------------------------------------------------
int8  verbose;          // verbose mode
int8  but_state;        // 

//------------------------------------------------------------------------------
//  Get char with time out                
//------------------------------------------------------------------------------
char getchto(int8 to)
{
    int8 i;
    char ch = 0xFF;
    for(i = 0; i < to; i++) {   // Try a few times
        if(kbhit()) {
            ch = getch();
            break;
        }
        delay_ms(50);          // 50 ms delay
    }
    return(ch);                // Return the value
}
  
//--------------------------------------------------------------------------
// Input hex data from std in and return int
//--------------------------------------------------------------------------
int8 get1hex(void) 
{
   char digit;
   digit = getc();
   putc(digit);
   if(digit <= '9') return(digit - '0');
   else             return((toupper(digit) - 'A') + 10);
}
int8 gethex(void) // get 2 hex digits and return 8 bit int
{
   return(get1hex()*16 + get1hex());
}


//------------------------------------------------------------------------------
//  EEPROM Programming:
//  Memory Map:
//  Addr        Description of values
//  ----        ----------------------------------------------------------------------
//  0x00        pointer to eeprom addr of inital rbf, 0XFF=no fpga init
//  0x01        board reset, 0x01=reset on init, 0xFF=no init reset
//  0x02        verbosity level, 0x00=lowest, 0x01 next highest, etc
//  0x03        LCD Backlight PWM level
//  0x04        LCD Backlight Saved PWM level
//  0x05        reserved
//    .
//    .
//  0x0F        reserved
//  0x10 - 0x1F 1st flash file record
//  0x20 - 0x2F 2nd flash file record
//       .
//       .
//  0xF0 - 0xFF last flash file record
//
//  Flash File Record format:
//  0xX0        MSB of file flash addr
//  0xX1        ISB of file flash addr
//  0xX2        LSB of file flash addr
//  0xX3        MSB of file flash length
//  0xX4        ISB of file flash length
//  0xX5        LSB of file flash length
//  0xX6 - 0xXF File name or description field, free form ASCII
//
// Example:
// 0x10  00 00 00  01 A3 EA   "plaid.rbf"   // len=107498
// 0x20  02 00 00  01 B4 D1   "lcd1.rbf"    // len=111825
//------------------------------------------------------------------------------
#define INIT_RBF    0x00
#define INIT_RESET  0x01
#define INIT_VERB   0x02
#define INIT_PWM    0x03
#define SAVED_PWM   0x04

//------------------------------------------------------------------------------
// Read EEPROM data from specified input address
//------------------------------------------------------------------------------
void ee_read(void)  
{ 
   int8 data;
   
   if(verbose) {
       printf("addr: 0x");
       data = read_eeprom(gethex());
       printf(" data: 0x%02x\r\n", data);
   }
   else {
       data = read_eeprom(gethex());
       printf("0x%02x\r\n", data);
   }
}

//------------------------------------------------------------------------------
// Write EEPROM data to specified input address
//------------------------------------------------------------------------------
void ee_write(void) 
{ 
   int8 addr;

   if(verbose) {
       printf("addr: 0x"); 
       addr = gethex();
       printf(" data: 0x"); 
       write_eeprom(addr, gethex());
       printf("\r\n eeprom data written"); 
   }
   else {
       addr = gethex();
       printf(" "); 
       write_eeprom(addr, gethex());
       printf("\r\n"); 
   }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Include Drivers
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include "FPGA.h"             // FPGA driver
#if FLASHRAM
#include "Flash.h"
#endif


//----------------------------------------------------------------------------
//  Reset System
//----------------------------------------------------------------------------
void ResetSystem(void)
{
    Output_Low(SysReset);              // System Reset asserted
    delay_ms(250);                     // quarter second delay
    Output_High(SysReset);             // System Reset de-asserted
    if(verbose) printf("Reset done\r\n"); 
}

//----------------------------------------------------------------------------
// Setting Backlight PWM level:  PWM freq = 19,531Hz, PWM time => 51.2 us
// value = PWM_Duty% * PWM_time * clock/ t2div
// value = PWM_Duty% * (51.2us) 20,000,000/4 = PWM_Duty% * 256
// 
// example for 50% duty cycle,
// value = .5 * 256 = 128>>2 OR /4 (shifted 2 to use 8 bit value) 
// value = 32 
//----------------------------------------------------------------------------
void SetBacklight(void)
{
    int8  pwm;
    float brightness; 
    char  string[4];
        
    if(verbose) printf("Brightness %%: "); // prompt message
    string[0] = getc(); putc(string[0]);
    string[1] = getc(); putc(string[1]); 
    string[2] = 0;
    brightness = atof(string);
    pwm = (brightness/100)*64;
//  if(verbose) printf("pwm = 0x%02x\r\n", pwm); // debug message
    Set_pwm1_duty(pwm); 
}


//------------------------------------------------------------------------------
// Detect Button press
//------------------------------------------------------------------------------
void check_button(void) 
{
  if(!input(Button)) {
    delay_ms (200);  //debounce
    if(!input(Button)) {
      but_state = ~but_state;
      if(but_state) {
        if(read_eeprom(SAVED_PWM) != 0xFF) Set_pwm1_duty(read_eeprom(SAVED_PWM));
        else                               Set_pwm1_duty(0x40); // default value
      }
      else                                 Set_pwm1_duty(0x00); // Backlight off
    }
  }
}

//-----------------------------------------------------------------------------
//------------------------------------------------------------------------------
//  Print help menu                 
//-----------------------------------------------------------------------------
//------------------------------------------------------------------------------
void helpmenu(void)
{
    printf("\r\n");
    printf("\r\nTaboot 1.0\r\n");
    
    printf("?     Show help menu\r\n");
    printf("V/v   Set to high/low verbosity\r\n");
    printf("L/l   Turn test LED ON/OFF\r\n");
//  printf("5/0   Turn 5V PS ON/OFF\r\n");
    printf("Bnn   Set Backlight Level to nn%%\r\n");
    printf("U     Upload FPGA Firmware\r\n");
    printf("~     System reset\r\n");
    printf("raa   Read EEPROM from aa address\r\n");
    printf("waadd Write to EEPROM address aa and data dd\r\n");
    
#if FLASHRAM
    printf("R     Read 256 byte block\r\n");
    printf("W     Write 256 byte block\r\n");
    printf("E     Erase Sector\r\n");
    printf("S     Read Flash Status\r\n");
    printf("+/-   Unlock/Lock flash for writing\r\n");
    printf("T     Transfer rbf File #\r\n");
#endif
}

//-----------------------------------------------------------------------------
//------------------------------------------------------------------------------
//  Process command byte                 
//-----------------------------------------------------------------------------
//------------------------------------------------------------------------------
void proc_cmd(int8 cmd)
{
    switch(cmd) {                                    // Switch on the command
        case '?': {helpmenu();              } break; // print help menu
        case 'V': {verbose = TRUE;          } break; // Set to high verbosity
        case 'v': {verbose = FALSE;         } break; // Set to low verbosity
        case 'L': {set(TestLED);            } break; // Turn Test LED On
        case 'l': {clr(TestLED);            } break; // Turn Test LED Off
        case '5': {set(PS5V_EN);            } break; // Turn 5V PS On
        case '0': {clr(PS5V_EN);            } break; // Turn 5V PS Off
        case 'B': {SetBacklight();          } break; // Backlight off
        case '~': {ResetSystem();           } break; // Reset FPGA
        case 'U': {Upload_RBF();            } break; // Upload FPGA RBF file
        case 'r': {ee_read();               } break; // Read EEPROM
        case 'w': {ee_write();              } break; // Write to EEPROM

    #if FLASHRAM
        case 'R': {Read_Flash();            } break; // Read 256 byte block
        case 'W': {Write_Flash();           } break; // Write 256 byte block
        case 'a': {Write_Test_Flash();      } break; // Write 256 byte Test block
        case 'E': {Erase_Sector();          } break; // Erase Sector
        case 'S': {Status_Flash();          } break; // Read Flash Status
        case '+': {unlock_flash();          } break; // Unlock Flash for writting
        case '-': {relock_flash();          } break; // relock Flash for writting
        case 'T': {LoadFPGA();              } break; // Transfer rbf #
    #endif
         default: cmd = 0x00;                 break; // invalid command
    }                                                // End of case loop
    if(cmd) putchar(cmd);   // Echo command byte back
}

//------------------------------------------------------------------------------
//  End .h
//------------------------------------------------------------------------------


