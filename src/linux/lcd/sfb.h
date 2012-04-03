// -----------------------------------------------------------------------------
// Simple Frame Buffer driver                                             sfb.h
// -----------------------------------------------------------------------------
#define     LCD_BASE      0x30000000     // LCD RAM Start
#define     LCD_SIZE16    0x00096000     // LCD RAM Size  =   614,400
#define     LCD_SIZE24    0x000E1000     // LCD RAM Size  =   921,600
#define     LCD_SIZE32    0x0012C000     // LCD RAM Size  = 1,228,800
#define   LCD_SPACE     0x00200000     // Total Address space of vid =2,097,152
//#define     LCD_SPACE     0x00100000     // Total Address space of vid =1,048,576
#define     TOPMEM        0x30200000     // Top of Address space of vid
#define     MEMEND        0x301FFFFF     // LCD RAM End 

//#define     LCD_WIDTH     640            // LCD visible display width
#define     LCD_WIDTH     1024            // LCD visible display width
#define     LCD_HEIGHT    480            // LCD visible display height

//---------------------------------------------------------------------------
// AT91 IO Control register  definitions
//---------------------------------------------------------------------------
#define     EBI_BASE    0xFFFFFF60     // External Bus Interface (EBI) User Interface
#define     EBI_CSA     0x00000000 /4  // Chip Select Assignment Register 
#define     EBI_CFGR    0x00000004 /4  // Configuration Register 
#define     Reserved1   0x00000008 /4  // Reserved space
#define     Reserved2   0x0000000C /4  // Reserved space
#define     SMC_CSR0    0x00000010 /4  // SMC Chip Select Register 0
#define     SMC_CSR1    0x00000014 /4  // SMC Chip Select Register 1 
#define     SMC_CSR2    0x00000018 /4  // SMC Chip Select Register 2 
#define     SMC_CSR3    0x0000001C /4  // SMC Chip Select Register 3 
#define     SMC_CSR4    0x00000020 /4  // SMC Chip Select Register 4
#define     SMC_CSR5    0x00000024 /4  // SMC Chip Select Register 5
#define     SMC_CSR6    0x00000028 /4  // SMC Chip Select Register 6
#define     SMC_CSR7    0x0000002C /4  // SMC Chip Select Register 7


//-----------------------------------------------------------------------------------------------
// SMC_CSR bit definitions             Bits
//                    33222222222211111111110000000000
//------------------  10987654321098765432109876543210 ------------------------------------------
//-----------------------------------------------------------------------------------------------
//#define SMC_NWS   0b00000000000000000000000000100000 // 00:06  Number of wait state
#define SMC_NWS     0b00000000000000000000000000000111 // 00:06  Number of wait state  
#define SMC_WSEN    0b00000000000000000000000010000000 // 07     Wait State Enable
#define SMC_TDF     0b00000000000000000000000000000000 // 08:11  Data Float Time
#define SMC_BAT     0b00000000000000000000000000000000 // 12     Byte Access Type, 0 = byte
#define SMC_DBW     0b00000000000000000100000000000000 // 13:14  Data Bus Width    2 = 8bits
#define SMC_DRP     0b00000000000000000000000000000000 // 15     Data Read Protocol
#define SMC_ACSS    0b00000000000000100000000000000000 // 16:17  Address to Chip Select Setup
#define SMC_RWSETUP 0b00000000000000000000000000000000 // 24:26  Read and Write Signal Setup Time
#define SMC_RWHOLD  0b00000000000000000000000000000000 // 28:30  Read and Write Signal Hold Time

#define SMC_BITDEF  SMC_RWHOLD|SMC_RWSETUP|SMC_ACSS|SMC_DRP|SMC_DBW|SMC_BAT|SMC_TDF|SMC_WSEN|SMC_NWS

//---------------------------------------------------------------------------
// GPIO Registers (for LCD Enable
//---------------------------------------------------------------------------
#define     PIOA_BASE                 0xFFFFF400
#define     PIOB_BASE                 0xFFFFF600
#define     PIOC_BASE                 0xFFFFF800
#define     PIO_SIZE                  0x1000UL

#define     PIO_PER                   0x00000000 /4  // Enable Register
#define     PIO_PDR                   0x00000004 /4 // Disable
#define     PIO_PSR                   0x00000008 /4
#define     PIO_OER                   0x00000010 /4
#define     PIO_ODR                   0x00000014 /4
#define     PIO_OSR                   0x00000018 /4
#define     PIO_SODR                  0x00000030 /4
#define     PIO_CODR                  0x00000034 /4
#define     PIO_IDR                   0x00000044 /4
#define     PIO_MDDR                  0x00000054 /4
#define     PIO_PUDR                  0x00000060 /4
#define     PIO_OWDR                  0x000000A4 /4

// -----------------------------------------------------------------------------
#define SFB_VIDEOMEMSTART (unsigned long) LCD_BASE
#define SFB_VIDEOMEMSIZE  (unsigned long) LCD_SPACE
#define SFB_MAX_X         LCD_WIDTH
#define SFB_MAX_Y         LCD_HEIGHT
#define SFB_MIN_X         LCD_WIDTH
#define SFB_MIN_Y         LCD_HEIGHT

// No transparency support in our hardware -------------------------------------
#define TRANSP_OFFSET     0
#define TRANSP_LENGTH     0
#define R_OFFSET          0
#define G_OFFSET          5
#define B_OFFSET         11

// -----------------------------------------------------------------------------
// end sfb.h 
// -----------------------------------------------------------------------------

