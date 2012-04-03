//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
// Design Name : tabx1 - tablet computer LCD support IP core
// File Name   : tabx1.v
// Function    : TabX1 Computer Controller Module
// Description : This controller module implements logic for the TabX1 computer 
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
module tabx1 (
    input             clk_50,          // main clock input
    input             reset,           // Reset input, negative logic
	 output            Test_LED,        // GPIO outputs

    input             mode,            // Mode: Text or graphics
	 input             RS232_in,        // RS232 Input line for tty mode
	 output            RS232_out,       // RS232 output line to tty
	 input             kbd_data,        // PS2 Keyboard input
	 input	          kbd_clk,         // PS2 Keyboard clock line
	 inout             mse_data,        // PS2 Mouse input
	 inout             mse_clk,         // PS2 Mouse clock line

    inout      [ 7:0] cpu_Data,        // Bi-Directional Data to ARM CPU
    input      [20:0] cpu_Address,     // Address input from ARM CPU
    input             cpu_RD,          // Read data enable, negative logic
    input             cpu_WR,          // Write data enable, negative logic
    input             cpu_CS1,         // Chip select 1, for Display, negative logic
    input             cpu_CS2,         // Chip select 2, for Audio, negative logic
    output            cpu_wait,        // CPU wait output, negative logic

    inout      [ 7:0] dram_Data,       // Bi-Directional DRAM Data port to SRAM
    output     [11:0] dram_Address,    // Address output for DRAM
    output            dram_CAS,        // DRAM Column Address Strobe
    output            dram_RAS,        // DRAM  Address Strobe
    output            dram_WE,         // DRAM Write Enable pin

    output     [ 5:0] lcd_r,           // LCD Red signals
    output     [ 5:0] lcd_g,           // LCD Green signals
    output     [ 5:0] lcd_b,           // LCD Blue signals
    output            lcd_xclk,			// LCD Pixel Clock
	 output            lcd_de,          // LCD Data enable line
	 
    input             I2S_TF,				// I2S Frame sync Input
    input             I2S_TK,				// I2S Sample rate clock input
    input             I2S_TD,				// I2S Serial audio data input
	 
	 output            audio_L,			// Audio Output Left channel	 
	 output            audio_R				// Audio Output Left channel	 
	 
  );
    
  //-------------------------------------------------------------------------------------------------
  // Blinky
  //-------------------------------------------------------------------------------------------------
  assign Test_LED = cnt[21] & ~cnt[23];
  reg[23:0] cnt;
  always @(posedge clk_25) 
    if(rst) cnt <=       24'd0;
    else    cnt <= cnt + 24'd1;
  	 
  //-----------------------------------------------------------------------------------------------
  // Phased Locked Loop instantiation
  //-----------------------------------------------------------------------------------------------
  wire clk_100;       		// 100 Mhz Clock
  wire clk_25;					//  25 Mhz Clock
  wire lock;					// PLL Locked signal
  pll1 pll_u1(
    .inclk0 (clk_50),		//  50Mhz input clock
    .c0     (clk_100),     // 100 Mhz,  0.00 ns phase shift
    .c1     (clk_25),      //  25 Mhz,  0.00	ns phase shift
    .locked (lock)			//  Lock signal
  );

  //-----------------------------------------------------------------------------------------------
  // Signal Assignments:
  //-----------------------------------------------------------------------------------------------
  wire rst     = ~reset;      				// System reset, set to positive logic
  wire wr_en1  = (~cpu_WR & ~cpu_CS1);   	// CPU write byte enable, switched to positive logic
  wire rd_en1  = (~cpu_RD & ~cpu_CS1);   	// CPU Read  byte enable, switched to positive logic

  wire  [7:0] cpu_Data_i  = cpu_Data;	   								// Data from ARM CPU
  assign      cpu_Data    = rd_en1 ? cpu_Data_o : 8'bZZZZZZZZ;   	// Bi-Directional Data to ARM CPU
  wire  [7:0] cpu_Data_o  = cpu_Address[20] ? dat_out : lcd_out;
  assign      cpu_wait    = ~lcd_hold;										// CPU wants negative logic
  wire  [7:0] dat_out	  = mse_dat;										// Peripheral data output

  wire        lcd_wren = ~cpu_Address[20] & wr_en1;		// Write enable for the LCD
  wire        lcd_rden = ~cpu_Address[20] & rd_en1;		// Read enable for the LCD
  wire        lcd_hold;												// LCD wait line
  wire  [7:0] lcd_out;												// LCD data output

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
// LCD module instantiation
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
lcd1 lcd_u1(
    .clk_100      (clk_100),        	// main clock input
    .clk_25       (clk_25),         	// main clock input
    .reset        (rst),            	// Reset input, negative logic

    .cpu_Data_i   (cpu_Data_i),       	// Bi-Directional Data to ARM CPU
    .cpu_Data_o   (lcd_out),         	// Bi-Directional Data to ARM CPU
    .cpu_Address  (cpu_Address[19:0]), // Address input from ARM CPU
    .cpu_Ren      (lcd_rden),         	// Read data enable, positive logic
    .cpu_Wen      (lcd_wren),         	// Write data enable, positive logic
    .cpu_wait     (lcd_hold),       	// CPU wait output, negative logic

    .dram_Data    (dram_Data),      	// Bi-Directional DRAM Data port to SRAM
    .dram_Address (dram_Address),   	// Address output for DRAM
    .dram_CAS     (dram_CAS),       	// DRAM Column Address Strobe
    .dram_RAS     (dram_RAS),       	// DRAM  Address Strobe
    .dram_WE      (dram_WE),        	// DRAM Write Enable pin

    .lcd_r        (lcd_r),          	// LCD Red signals
    .lcd_g        (lcd_g),          	// LCD Green signals
    .lcd_b        (lcd_b),          	// LCD Blue signals
    .lcd_xclk     (lcd_xclk),       	// LCD Pixel Clock
	 .lcd_de       (lcd_de)          	// LCD Data enable line
  );

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
// PS2 Keyboard and Mouse Modules
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------
// PS2 Register addresses definitions and decoding
// --------------------------------------------------------------------
//        TBX_BASE     0x301FF000     /* TABX1 registers Base               */
//        TBX_SIZE     0x00001000     /* TABX1 registers Size               */
`define PS2_REG_BASE   17'h1FFFF 	// Base register for PS2
`define PS2_REG_XINC   4'h0	    	// X Increment register
`define PS2_REG_YINC   4'h1   		// Y Increment register
`define PS2_REG_STAT   4'h2    		// Button Status register

//        TBX_BASE     0x301FE000     /* TABX1 registers Base               */
//        TBX_SIZE     0x00001000     /* TABX1 registers Size               */
`define KBD_REG_BASE   17'h1FEFF 	// Base register for Keyboard
`define KBD_REG_ASCI   4'h0    		// Keyboard ASCII register
`define KBD_REG_SCAN   4'h1    		// Keyboard Scan code register
`define KBD_REG_KBDS   4'h2    		// Keyboard Status register
// --------------------------------------------------------------------
wire			ps2_rd_req = rd_en1 & ps2_base;  // Read request
wire        ps2_base   = (cpu_Address[20:4] == `PS2_REG_BASE);
wire        ps2_reg1   = (cpu_Address[ 3:0] == `PS2_REG_XINC) && ps2_rd_req;
wire        ps2_reg2   = (cpu_Address[ 3:0] == `PS2_REG_YINC) && ps2_rd_req;
wire        ps2_reg3   = (cpu_Address[ 3:0] == `PS2_REG_STAT) && ps2_rd_req;

wire			kbd_rd_req = rd_en1 & kbd_base;  // Read request
wire        kbd_base   = (cpu_Address[20:4] == `KBD_REG_BASE);
wire        kbd_reg1   = (cpu_Address[ 3:0] == `KBD_REG_ASCI) && kbd_rd_req;
wire        kbd_reg2   = (cpu_Address[ 3:0] == `KBD_REG_SCAN) && kbd_rd_req;
wire        kbd_reg3   = (cpu_Address[ 3:0] == `KBD_REG_KBDS) && kbd_rd_req;

wire [ 7:0] mse_dat_x = cache_q[ 7: 0];
wire [ 7:0] mse_dat_y = cache_q[16: 9];
wire [ 7:0] mse_dat_s = {~cache_empty, overflow, cache_q[8], cache_q[17], cache_q[20:18]};
wire [ 7:0] mse_dat   = ps2_reg1 ? mse_dat_x  : 
                        ps2_reg2 ? mse_dat_y  : 
                        ps2_reg3 ? mse_dat_s  :
                        kbd_reg1 ? ascii_code :
                        kbd_reg2 ? scan_code  :
                        kbd_reg3 ? kbd_status :	8'h55;

// --------------------------------------------------------------------
// PS2 Mouse Section
// --------------------------------------------------------------------
wire  [ 2:0] buttons;                  // Button wire array
wire  [ 8:0] x_incr;                   // x Increment wires
wire  [ 8:0] y_incr;                   // y Increment wires
wire  [ 1:0] overflow;                 // Increment overflow
wire         mse_data_ready;           // New data is ready to read

// --------------------------------------------------------------------
// PS2 Cache Section
// --------------------------------------------------------------------
wire [22:0] cache_input = {overflow, buttons, y_incr, x_incr};
wire [22:0] cache_q;
wire       cache_empty;
ps2_cache cache_u1(
    .data    ( cache_input ),
    .wrclk   ( clk_50 ),
    .wrreq   ( mse_data_ready ),

    .rdclk   ( ps2_reg3 ),
    .rdreq   ( 1'b1 ),
    .q       ( cache_q ),
    .rdempty ( cache_empty )
    );

// --------------------------------------------------------------------
// PS2 Mouse Module Instantiation
// --------------------------------------------------------------------
ps2_mouse mouse_u1(
    .clock		(clk_50),            // System clock
    .reset		(rst),               // System reset
    .ps2_clk	(mse_clk),           // Mouse clock line
    .ps2_data	(mse_data),          // Mouse data line
    .dout_dx	(x_incr),            // X position increment
    .dout_dy	(y_incr),            // Y position increment
    .ovf_xy		(overflow),          // Increment overflow
    .btn_click	(buttons),           // Button array
    .ready		(mse_data_ready)     // Data ready flag
);

// --------------------------------------------------------------------
// PS2 Keyboard Section
// --------------------------------------------------------------------
wire  [ 7:0] scan_code;								// keyboard scan data
wire  [ 7:0] ascii_code;							// keyboard ascii data
wire  [ 7:0] kbd_status = {kbd_ready, , 4'h0, kbd_shift_key_on, kbd_extended, kbd_released};

// --------------------------------------------------------------------
// Synchronize reading with host
// --------------------------------------------------------------------
reg kbd_ready;
reg kbd_reading;
always @(kbd_data_ready or kbd_reg3 or rst or kbd_ready or kbd_reading) begin
	if(rst) begin
		kbd_ready   <= 1'b0;
		kbd_reading <= 1'b0;
	end
	else begin
		if(kbd_data_ready) kbd_ready <= 1'b1;
		if(kbd_ready && !kbd_reading &&  kbd_reg3) kbd_reading <= 1'b1;
		if(kbd_ready &&  kbd_reading && !kbd_reg3) begin 
			kbd_ready   <= 1'b0;
			kbd_reading <= 1'b0;
		end
	end
end

// --------------------------------------------------------------------
// PS2 Keyboard Module Instantiation
// --------------------------------------------------------------------
wire	kbd_data_ready;
wire	kbd_extended;
wire	kbd_released;
wire	kbd_shift_key_on;
ps2_kbd kbd_u1(
	.clk					(clk_50),
	.reset				(rst), 
	
   .ps2_clk				(kbd_clk),				// input
   .ps2_data			(kbd_data),				// input
	
   .scan_code			(scan_code),	 		// output
   .ascii				(ascii_code),			// output
   .data_ready			(kbd_data_ready),	 	// output
	.rx_extended		(kbd_extended), 		// output
   .rx_released		(kbd_released), 		// output
   .rx_shift_key_on	(kbd_shift_key_on) 	// output
);

// --------------------------------------------------------------------
// I2S Audio Codec Module Instantiation
// --------------------------------------------------------------------
wire [15:0] data_L;				// DAC Data Left
wire [15:0] data_R;				// DAC Data Right
wire			rdy_L;				// New I2S data ready for Left DAC
wire			rdy_R;				// New I2S data ready for Right DAC

I2S_slave16 I2S_u1 (
	.clk			(clk_50),		// Main Clock
   .FRM			(I2S_TF),		// I2S Framing Input
   .BCK			(I2S_TK),		// I2S Sample bit clock input
   .DIN			(I2S_TD),		// I2S Serial audio data input
   .out_L		(data_L),		// Left output
   .out_R		(data_R),		// Right output
	.ready_L		(rdy_L),			// Signal that data is ready to be sent out
	.ready_R		(rdy_R)			// Signal that data is ready to be sent out
);
	
pcm_dac dac_u1 (
	.clk		(clk_50),			// Main Clock
   .dac_L	(data_L),			// Left DAC 
   .dac_R	(data_R),			// Right DAC 
   .wren_L	(rdy_L),				// Write data to DAC
	.wren_R	(rdy_R),				// Write data to DAC
   .audio_L	(audio_L),			// Right PCM Audio output
   .audio_R	(audio_R)			// Left PCM Audio output
);	

//-------------------------------------------------------------------------------------------------
endmodule 
//-------------------------------------------------------------------------------------------------
