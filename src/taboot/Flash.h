//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//  S P I    F l a s h    F u n c t i o n s                                     
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
#include "SST25V.h"

//--------------------------------------------------------------------------
//    Read 256 bytes from flash and return on comm line
//--------------------------------------------------------------------------
void Status_Flash(void) {
     printf("Status=0x%02x\r\n",STFlash_readStatus());
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//  S T    F l a s h    F u n c t i  o n s                                     
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

//--------------------------------------------------------------------------
// Initialize ST Flash RAM
//--------------------------------------------------------------------------
void Init_Flash(void)
{
    Init_STFlash(); // Initialize ST Flash RAM
}

//--------------------------------------------------------------------------
// Initialize ST Flash RAM with retries, 
// tries = number of times to retry befor giving up
//--------------------------------------------------------------------------
short Try_Init_Flash(int tries)
{
    short ret = 1;
    do {
        Init_STFlash();                    // Initialize ST Card
        if(STFlash_readStatus() == 0x1C) { // Check Status of ST FLASH
            ret = 0;
            break;
        }
    } while(tries--);
    if(verbose) {
        if(ret) printf("Flash Init failed\r\n");
        else    printf("Flash Init OK\r\n");
    }
    return(ret);
}

//--------------------------------------------------------------------------
// Lock and unlock flash for writting
//--------------------------------------------------------------------------
void unlock_flash(void) { STFlash_writeStatus(0x00); }
void relock_flash(void) { STFlash_writeStatus(0x1C); }

//--------------------------------------------------------------------------
// Input hex string from std in and return 32 bit int
//--------------------------------------------------------------------------
int32 get_address(void) 
{
    int32 Address; 
    printf("Address: 0x");  
    Address = Make32(gethex(),gethex(),gethex(),gethex()); // Start Address
    printf("\r\n");
    return(Address);
}

//--------------------------------------------------------------------------
//    Read 256 bytes from flash and return on comm line
//--------------------------------------------------------------------------
void Read_Flash(void)
{
    int8  buffer[256];          // Buffer for data 
    int16 i;

    STFlash_ReadBlock(get_address(), Buffer, 256);
    for(i = 0; i < 256; i++) {
        printf("0x%02x ",Buffer[i]);  // Send bytes to PC
        if(!((i+1)%16)) printf("\r\n");
    }
}

//--------------------------------------------------------------------------
//    Write 256 bytes of test data to Flash
//--------------------------------------------------------------------------
void Write_Test_Flash(void) 
{
    int8  Buffer[256];          // Buffer for data 
    int32 Address; 
    int16  i;
    
    Address = get_address();                       // Start Address
    for(i = 0; i < 256; i++) Buffer[i] = i;      // get the 256 byte block
    STFlash_WriteBlock(Address, Buffer, 256);
    if(verbose) printf("Flash test write done\r\n");
    else        putchar('.');   // end of data indicator
}

//--------------------------------------------------------------------------
//    Write 256 bytes to Flash
//--------------------------------------------------------------------------
void Write_Flash(void) 
{
    int8  Buffer[256];          // Buffer for data 
    int32 Address; 
    int16  i;
    
    Address = Address = Make32(gethex(),gethex(),gethex(),gethex()); // Start Address
    for(i = 0; i < 256; i++) Buffer[i] = getch();  // get RAW 256 byte block
    STFlash_WriteBlock(Address, Buffer, 256);
    putchar('.');   // end of data indicator
}


//------------------------------------------------------------------------------
// Erase a sector
//------------------------------------------------------------------------------
void Erase_Sector(void)          
{
    STFlash_EraseBlock(get_address());
    if(verbose) printf("Flash erase done\r\n");
}

//------------------------------------------------------------------------------
// EEPROM Memory Map :
// PIC EEPROM is non-volital memory used store various parameters and
// prescriptions for the boot up and control of ZBC.
//------------------------------------------------------------------------------
//   Start    End  Size Description
// ------- ------- ---- ------------------------------------------------------
//    0x00    0x00    1 Boot type: 0 = No boot(debug) 1=RBF1 2=RBF2
//    0x01    0x03    3 Start address of RBF File 0
//    0x04    0x06    3 Size of RBF File 0
//    0x07    0x09    3 Start address of RBF File 1
//    0x0A    0x0C    3 Size of RBF File 1
//    0x0D    0x0F    3 Start address of RBF File 2
//    0x10    0x12    3 Size of RBFFile 2
//
//    0x20    0x21    1 Start of other Options
//------------------------------------------------------------------------------
#define BOOT_LOCATION 0x00      // Boot locatoin indicator
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Flash Memory Map Example:
//------------------------------------------------------------------------------
//     Start       End    Start      End    Flash   Actual
//   Address   Address  Address  Address    Space     Size Comment
//   ------- --------- -------- -------- -------- -------- ------------------
//         0   131,071 0x000000 0x01FFFF  131,071  107,498 FPGA Config file #1
//   131,072   262,143 0x020000 0x03FFFF  131,071  131,071 FPGA Config file #2
//   262,144   393,213 0x040000 0x05FFFF  131,071  131,071 FPGA Config file #3
//      .         .     .        .  .        .        .    .
// 4,063,232 4,194,303 0x3E0000 0x3FFFFF  131,071  131,071 Top of Flash Space
//
//------------------------------------------------------------------------------

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
// FLASH TO FPGA Upload Function:
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void FlashToFPGA(int8 rbf)
{
    int8  data;                     // Location of RBF file in Flash RAM
    int32 i, Length;                // Counter registers

    Output_High(TestLED);            // Turn on TestLED
    Output_Low(FPGALoad);            // FPGA Upload pin
    Delay_ms(50);                    // 50 ms delay to put FPGA into load mode
    Output_High(FPGALoad);           // FPGA Upload pin
    Delay_ms(20);                    // 50 ms delay to put FPGA into load mode
    STchip_select();                        // Enable select line
    STFlash_sendByte(0x03);                 // Send opcode
    STFlash_sendByte(read_eeprom(rbf++));   // Send address high byte
    STFlash_sendByte(read_eeprom(rbf++));   // Send address mid byte
    STFlash_sendByte(read_eeprom(rbf++));   // Send address low byte
    Length = make32(0,read_eeprom(rbf++),read_eeprom(rbf++),read_eeprom(rbf++));
    for(i = 0; i < Length; i++) {
        data = STFlash_GetByte();   // get next byte from flash ram
        LoadFPGAByte(data);         // Send byte to the FPGA 
    }
    STchip_deselect();           // Disable select line
    Output_Low(TestLED);         // Turn on TestLED
    if(verbose) printf("Flash to fpga done\r\n");
}

//----------------------------------------------------------------------------
// Load FPGA Firmware from flash
// erbfa = Pointer to Location of RBF file in EEPROM
//----------------------------------------------------------------------------
void InitFPGA(int8 erbfa)
{
    int8 rbf;                     // Location of RBF file in EEPROM
    rbf = read_eeprom(erbfa);     // read which rbf file to read
    if(rbf == 0xFF) {             // if blank, then skip it
       if(verbose) printf("no address in eeprom\r\n");
       return;       
    }
    FlashToFPGA(rbf);             // Load RBF 1
}

//----------------------------------------------------------------------------
// Load FPGA Firmware from flash
// Get EEPROM pointer from user console
//----------------------------------------------------------------------------
void LoadFPGA(void)
{
    if(verbose) printf("EEPROM Pointer: ");
    FlashToFPGA(gethex());                  // get pointer RBF 
}
//------------------------------------------------------------------------------
//    End .h
//------------------------------------------------------------------------------

