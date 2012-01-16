//==============================================================================
//==============================================================================
// FPGA Controler Routines
//
// FPGA.H -- FPGA Controller                                        
// DonnaWare International LLP Copyright (2001) All Rights Reserved        
//==============================================================================
//==============================================================================

//------------------------------------------------------------------------------
//  Upload FPGA Firware 
//  Un-comment these lines and set them according to your application.
//
// #define FPGAClock   PIN_xx      // FPGA Serial upload clock line
// #define FPGADOut    PIN_xx      // FPGA Serial upload data line
// #define FPGALoad    PIN_xx      // FPGA Serial upload select line
// #define FPGAReset   PIN_xx      // Reset
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  Upload FPGA Firware via RS232 Line
//------------------------------------------------------------------------------
#define USEASM      1               // Use Assembly for speed
#define IOPORT      0xF80           // IO Port, A=F80, B=F81, C=F82, D=F83, E=F84
#define FPGAclk     1               // FPGA clock ( pin  75)
#define FPGAsio     2               // FPGA I/O data ( pin 76)
#define FPGAce      3               // FPGA Config line LO=reset HI=active
#define FPGA_SIO    IOPORT,FPGAsio  // FPGA SIO line LO=reset HI=active
#define FPGA_CLK    IOPORT,FPGAclk  // FPGA clock line
//--------------------------------------------------------------------------
// LOAD FPGA Config Byte - Uploads a single byte of data to FPGA from RS232 port
// CAUTION: the USEASM option is needed to keep up with the RS232 input if
// the baud rate is high (eg. >115.2kbps), UNLESS, the #opt level is set to 
// at least 9. 
//--------------------------------------------------------------------------
void LoadFPGAByte(byte Sdata)
{
#if USEASM
    int8  Rbit, Rdata;
    #asm
            Movf    Sdata,W         // Store it for later
            Movwf   Rdata           // Tempstore data
            Movlw   0x08            // Number of bits to send
            Movwf   Rbit            // Save it
    WBit:   Btfss   Rdata,0         // Check next bit
            Bcf     FPGA_SIO        // If clear send a zero
            Btfsc   Rdata,0         // Check next bit
            Bsf     FPGA_SIO        // If set send a one
            Bsf     FPGA_CLK        // Clock pin high
            Rrcf    Rdata,F         // Rotate for next bit
            Bcf     FPGA_CLK        // Raise clock pin
            Decfsz  Rbit,F          // Decrement bit counter
            Goto    WBit            // Do next bit
    #endasm
#else
    int8 i;
   
    for(i = 0; i < 8; ++i) {
        output_bit(FPGADOut, shift_right(&SData,1,0));  // Send a data bit
        output_low(FPGAClock);
        output_high(FPGAClock);                         // Pulse the clock
    }
#endif
}

//------------------------------------------------------------------------------
// Recieves a stream from RS232 line and uploads it to the FPGA
//------------------------------------------------------------------------------
void Upload_RBF(void)
{
    int32 i, rbflength;         // RBF file length in 256 byte blocks
    char string[30];
    
    gets(string);
    rbflength = atoi32(string);
    
    Output_Low(FPGALoad);            // FPGA Upload pin
    delay_ms(10);                    // 50 ms delay to put FPGA into load mode
    Output_High(FPGALoad);           // FPGA Upload pin
    for(i = 0; i < rbflength; i++) {
        LoadFPGAByte(getch());
    }
    
    if(verbose) printf("\r\nrbf %Ld bytes received\r\n", rbflength);
    else        putchar('F');                    // To signal Success
}

//------------------------------------------------------------------------------
//    End .h
//------------------------------------------------------------------------------

