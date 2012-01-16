//------------------------------------------------------------------------------
// Library for an SST25V DataFlash:  SIZE   4,194,304  bytes 
// init_STFlash() - Initializes the pins that control the flash device. This must  
//                  be called before any other flash function is used.             
//
// BYTE STFlash_getByte() - Gets a byte of data from the flash device                     
//                          Use after calling STFlash_startContinuousRead()              
//
// void STFlash_getBytes(a, n) - Read n bytes and store in array a                              
//                               Use after calling STFlash_startContinuousRead()              
//
// void STFlash_readBuffer(b, i, a, n) - Read n bytes from buffer b at index i
//                                       and store in array a
//
// BYTE STFlash_readStatus() - Return the status of the flash device:  
//                             Rdy/Busy Comp 0101XX
//
// void STFlash_writeToBuffer(b, i, a, n) - Write n bytes from array a to 
//                                          buffer b at index i
//
// void STFlash_eraseBlock(b) - Erase all bytes in block b to 0xFF. A block is 256.    
// 
// The main program may define FLASH_SELECT, FLASH_CLOCK,   
// FLASH_DI, and FLASH_DO to override the defaults below.  
//                                      
//                       Pin Layout                         
//   ---------------------------------------------------    
//   |    __                                           | 
//   | 1: CS    FLASH_SELECT   | 8: VCC  +2.7V - +3.6V | 
//   |                         |    ____               |  
//   | 2: SO   FLASH_DO        | 7: HOLD  Hold         |   
//   |    ___                  |                       |    
//   | 3: WP    Write Protect  | 6: SCK   FLASH_CLOCK  |              
//   |                         |    __                 |              
//   | 4: Vss   Ground         | 5: SI     FLASH_DI    |              
//   ---------------------------------------------------              
//                                                                    
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//     * * * USER CONFIGURATION Section, set these per Hardware set up * * *
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
#define UseHWSPI   1         // Set to 1 for HW, 0 for SW, must use correct pins for HW 

#define FLASH_DI     MOSI    // PIC to Flash
#define FLASH_CLOCK  SCK     // Flash Clk
#define FLASH_DO     MISO    // Flash to PIC
#define FLASH_SELECT SSEL    // Flash /CS

#define SCLKDELAY    2       // Delay in cycles, for SW driver
#define SELSDELAY    1       // Delay in us, for SW driver

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// HW SPI SSP Routines                                                             
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#if UseHWSPI
    #byte SSPBUF  = 0x0FC9 
    #byte SSPCON  = 0x0FC6 
    #byte SSPSTAT = 0x0FC7 
    #bit  SSPBF   = SSPSTAT.0 
    #bit  SSPSMP  = SSPSTAT.7
    #bit  SSPWCOL = SSPCON.7
    #bit  SSPCKP  = SSPCON.4

    #define  READ_SSP()     (SSPBUF) 
    #define  SSP_HAS_DATA() (SSPBF) 
    #define  WAIT_FOR_SSP()  while(!SSP_HAS_DATA()) 
    #define  WRITE_SSP(chr)  SSPBUF=(chr)

    int ssp_xfer(int data) {
        SSPWCOL = 0;      // Precautionary Measure
        WRITE_SSP(data);
        WAIT_FOR_SSP();
        return(READ_SSP());
    }

    #define ssp_get()        ssp_xfer(0xFF)
    #define ssp_put(data)    ssp_xfer(data)
#endif 

//------------------------------------------------------------------------------
// Purpose:       Initialize the pins that control the flash device.
//                This must be called before any other flash function is used.
// Inputs:        None
// Outputs:       None
// Dependencies:  None
//------------------------------------------------------------------------------
void Init_STFlash(void)
{
#if UseHWSPI
    setup_spi(spi_master | spi_h_to_l | spi_clk_div_4);
    SSPCKP = 0;
    SSPSMP = 0;
#else
    output_low(FLASH_CLOCK);
#endif    
    output_high(FLASH_SELECT);
}


//------------------------------------------------------------------------------
// Purpose:       Select the flash device
// Inputs:        None
// Outputs:       None
//------------------------------------------------------------------------------
void STchip_select(void)
{
#if UseHWSPI
   output_low(FLASH_SELECT);            // Enable select line
#else
   output_high(FLASH_SELECT);       // Flash Select High
   output_low(FLASH_CLOCK);         // Flash Clock Low
   delay_ms(10);
   output_low(FLASH_SELECT);            // Enable select line
   delay_us(SELSDELAY);                 // Settling time
#endif
}

//------------------------------------------------------------------------------
// Purpose:       Deselect the flash device
// Inputs:        None
// Outputs:       None
//------------------------------------------------------------------------------
void STchip_deselect(void)
{
#if UseHWSPI
   output_high(FLASH_SELECT);           // Disable select line
#else
   output_high(FLASH_SELECT);           // Disable select line
   output_low(FLASH_CLOCK);            // Pulse the clock
   delay_us(SELSDELAY);                 // Settling time
#endif   
}

//------------------------------------------------------------------------------
// Purpose:       Send data Byte to the flash device
// Inputs:        1 byte of data
// Outputs:       None
//------------------------------------------------------------------------------
void STFlash_SendByte(int8 data)
{
#if UseHWSPI
    ssp_put(data);
#else
    int8 i;
    for(i=0; i<8; i++) {
        output_bit(FLASH_DI, shift_left(&data,1,0));    // Send a data bit
        output_high(FLASH_CLOCK);                       // Pulse the clock
        delay_cycles(SCLKDELAY);                        // Same as a NOP
        output_low(FLASH_CLOCK);
        delay_cycles(SCLKDELAY);                        // Same as a NOP
   }
#endif
}

//------------------------------------------------------------------------------
// Purpose:       Receive data Byte from the flash device
// Inputs:        None
// Outputs:       1 byte of data
// Dependencies:  Must enter with Clock high (preceded by a send)
//------------------------------------------------------------------------------
int8 STFlash_GetByte(void)
{
#if UseHWSPI
    return(ssp_get());
#else
   int8 i, flashData;
   for(i=0; i<8; i++) {
       output_high(FLASH_CLOCK);                       // Pulse the clock
       shift_left(&flashData, 1, input(FLASH_DO));      
       output_low(FLASH_CLOCK);
   }
   return(flashData);
#endif
}

//------------------------------------------------------------------------------
// Purpose:       Get a byte of data from the flash device. This function is
//                meant to be used after STFlash_startContinuousRead() has
//                been called to initiate a continuous read. This function is
//                also used by STFlash_readPage() and STFlash_readBuffer().
// Inputs:        1) A pointer to an array to fill
//                2) The number of bytes of data to read
// Outputs:       None
// Dependencies:  None
//------------------------------------------------------------------------------
void STFlash_getBytes(int8 *data, int16 size)
{
#if UseHWSPI
   int16 i;
   for(i=0; i<size; ++i) {
       data[i] = ssp_get();
   }
#else
   int16 i;
   int8  j;
   for(i=0; i<size; i++) {
      for(j=0; j<8; j++) {
         output_high(FLASH_CLOCK);
         delay_cycles(SCLKDELAY);                   // Small delay NOP
         shift_left(data+i, 1, input(FLASH_DO));
         output_low(FLASH_CLOCK);
         delay_cycles(SCLKDELAY);                   // Same as a NOP
      }
   }
#endif
}

//------------------------------------------------------------------------------
// Purpose:       Return the Read status Register of the flash device
// Inputs:        None            ____
// Outputs:       The Read status
// Dependencies:  STFlash_sendData(), STFlash_getByte()
//------------------------------------------------------------------------------
int8 STFlash_readStatus()
{
   int8 status;
   STchip_select();                // Enable select line
   STFlash_SendByte(0x05);         // Send status command
   status = STFlash_GetByte();     // Get the status
   STchip_deselect();              // Disable select line
   return(status);                 // Return the status
}

//------------------------------------------------------------------------------
// Purpose:       Wait until the flash device is ready to accept commands
// Inputs:        None
// Outputs:       None
// Dependencies:  STFlash_sendData()
//------------------------------------------------------------------------------
void STFlash_waitUntilReady(void)
{
   STchip_select();                // Enable select line
   STFlash_sendByte(0x05);         // Send status command
   while(input(FLASH_DO));         // Wait until ready
   STFlash_GetByte();              // Get byte 
   STchip_deselect();              // Disable select line
}

//----------------------------------------------------------------------------
// Purpose:       Enable Page Program write
// Inputs:        None.
// Outputs:       None.
// Dependencies:  STFlash_sendData(), STFlash_waitUntilReady()
//----------------------------------------------------------------------------
void STFlash_WriteEnable(void)
{
   STchip_select();                // Enable select line
   STFlash_sendByte(0x06);         // Send opcode
   STchip_deselect();              // Disable select line
}

//----------------------------------------------------------------------------
// Purpose:       Disable Page Program write
// Inputs:        None.
// Outputs:       None.
// Dependencies:  STFlash_sendData(), STFlash_waitUntilReady()
//----------------------------------------------------------------------------
void STFlash_WriteDisable(void)
{
   STchip_select();                // Enable select line
   STFlash_sendByte(0x04);         // Send opcode
   STchip_deselect();              // Disable select line
}

//------------------------------------------------------------------------------
// Purpose:       Write a byte to the status register of the flash device
// Inputs:        None
// Outputs:       None
// Dependencies:  STFlash_sendData(), STFlash_getByte()
//------------------------------------------------------------------------------
void STFlash_writeStatus(int8 value)
{
   STFlash_WriteEnable();          // Enable b yte writting
   STchip_select();                // Enable select line
   STFlash_sendByte(0x01);         // Send status command
   STFlash_sendByte(value);        // Send status value
   STchip_deselect();              // Disable select line
   STFlash_WriteDisable();         // Disable writting
}

//------------------------------------------------------------------------------
// Purpose:       Reads a block of data (usually 256 bytes) from the ST
//                Flash and into a buffer pointed to by int Buffer;
//
// Inputs:        1) Address of block to read from
//                2) A pointer to an array to fill
//                3) The number of bytes of data to read
// Outputs:       None
// Dependencies:  STFlash_sendData(), STFlash_getByte()
//------------------------------------------------------------------------------
void STFlash_ReadBlock(int32 Address, int8 *buffer, int16 size)
{
   STchip_select();                       // Enable select line
   STFlash_sendByte(0x03);                // Send opcode
   STFlash_sendByte(Make8(Address, 2));   // Send address 
   STFlash_sendByte(Make8(Address, 1));   // Send address
   STFlash_sendByte(Make8(Address, 0));   // Send address
   STFlash_getBytes(buffer, size);        // Get bytes into buffer
   STchip_deselect();                     // Disable select line
}

//------------------------------------------------------------------------------
// Purpose:       Send some bytes of data to the flash device
// Inputs:        1) A pointer to an array of data to send
//                2) The number of bytes to send
// Outputs:       None
// Dependencies:  None
//------------------------------------------------------------------------------
void STFlash_Write1Byte(int32 Address, int8 data)
{
   STFlash_WriteEnable();                 // Write 1 byte to specified address
   STchip_select();                       // Enable select line
   STFlash_sendByte(0x02);                // Send Opcode
   STFlash_sendByte(Make8(Address, 2));   // Send Address 
   STFlash_sendByte(Make8(Address, 1));   // Send Address
   STFlash_sendByte(Make8(Address, 0));   // Send Address
   STFlash_SendByte(data);                // Send Data
   STchip_deselect();                     // Disable select line
}

//------------------------------------------------------------------------------
// Purpose:       Writes a block of data (usually 256 bytes) to the ST
//                Flash from a buffer pointed to by int Buffer;
//
// Inputs:        1) Address of block to read from
//                2) A pointer to an array to fill
//                3) The number of bytes of data to read
// Outputs:       None
//------------------------------------------------------------------------------
void STFlash_WriteBlock(int32 Address, int8 *buffer, int16 size)
{
    int16 i;
    int32 addr;
    addr = Address;
    for(i = 0; i < size; i++) {
       STFlash_Write1Byte(addr++, buffer[i]);
       delay_us(10);
       while(STFlash_readStatus() & 0x01);
    }
    STFlash_WriteDisable();
}

//------------------------------------------------------------------------------
// Purpose:       Erase a block of data (usually 256 bytes)
//
// Inputs:        1) Address of block to erase
// Outputs:       None
//------------------------------------------------------------------------------
void STFlash_EraseBlock(int32 Address)
{
   STFlash_WriteEnable();                // Enable b yte writting
   STchip_select();                      // Enable select line
   STFlash_sendByte(0xD8);               // Send opcode
   STFlash_sendByte(Make8(Address, 2));  // Send address 
   STFlash_sendByte(Make8(Address, 1));  // Send address
   STFlash_sendByte(Make8(Address, 0));  // Send address
   output_high(FLASH_SELECT);            // Disable select line
   STchip_deselect();                    // Disable select line
   STFlash_WriteDisable();               // Disable writting
}

//------------------------------------------------------------------------------
// Purpose:       Erase a block of data (usually 256 bytes)
//
// Inputs:        1) Address of block to erase
// Outputs:       None
//------------------------------------------------------------------------------
void STFlash_Erase32KBlock(int32 Address)
{
   STFlash_WriteEnable();                // Enable b yte writting
   STchip_select();                      // Enable select line
   STFlash_sendByte(0x52);               // Send opcode
   STFlash_sendByte(Make8(Address, 2));  // Send address 
   STFlash_sendByte(Make8(Address, 1));  // Send address
   STFlash_sendByte(Make8(Address, 0));  // Send address
   output_high(FLASH_SELECT);            // Disable select line
   STchip_deselect();                    // Disable select line
   STFlash_WriteDisable();               // Disable writting
}


//----------------------------------------------------------------------------
//  End .h
//----------------------------------------------------------------------------
