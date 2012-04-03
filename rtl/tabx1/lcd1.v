//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
// Design Name : lcd1 - tablet computer LCD support IP core
// File Name   : lcd1.v
// Function    : LCD Controller Module
// Description : This controller module implements an LCD controller for the TabX1 computer using 
//               the RGB565 format (16 Bit color). 
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
module lcd1 (
    input             clk_100,        // main clock input
    input             clk_25,         // main clock input
    input             reset,          // Reset input, negative logic

    input       [ 7:0] cpu_Data_i,    // Data fromt ARM CPU
    output reg  [ 7:0] cpu_Data_o,    // Data to ARM CPU

    input      [19:0] cpu_Address,    // Address input from ARM CPU
    input             cpu_Ren,        // Read data enable, negative logic
    input             cpu_Wen,        // Write data enable, negative logic
    output            cpu_wait,       // CPU wait output, negative logic

    inout  reg [ 7:0] dram_Data,      // Bi-Directional DRAM Data port to SRAM
    output reg [11:0] dram_Address,   // Address output for DRAM
    output reg        dram_CAS,       // DRAM Column Address Strobe
    output reg        dram_RAS,       // DRAM  Address Strobe
    output reg        dram_WE,        // DRAM Write Enable pin

    output     [ 5:0] lcd_r,          // LCD Red signals
    output     [ 5:0] lcd_g,          // LCD Green signals
    output     [ 5:0] lcd_b,          // LCD Blue signals
    output            lcd_xclk,       // LCD Pixel Clock
	 output            lcd_de          // LCD Data enable line
  );

  //-----------------------------------------------------------------------------------------------
  // Signal Assignments:
  //-----------------------------------------------------------------------------------------------
  wire 	clk  		= clk_100;     					// LCD Clock
  wire 	pclk   	= clk_25;							// Input for LCD Clock
  wire 	rdb_enb	= cpu_Ren;      					// CPU Read byte clock
  wire 	rd_wait 	= read_req & ~read_rdy; 
  assign	cpu_wait = (wr_wait | rd_wait);			// positive logic

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
// CPU Read through stub
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
reg read_req;
reg read_rdy;
always @(rdb_enb or read_rdy or reset) begin
    if(reset) begin                             // If reset line is low, then set initial coditions
        read_req <= 1'b0;        	            // Read request clear
    end
    else begin                                      // If reset line is high, then run the machines
        if(rdb_enb & !read_rdy) read_req <= 1'b1;   // request a read
        else                    read_req <= 1'b0;   // derequest a read
    end
end

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
// Address Translation
// row col Byte CPU Address            DRAM Address translated
//  0, 638 H:   00000000001001111110 - 000000000 01001111110
//                                     987654321 09876543210
//                                     111111111 10000000000 
//                                               
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
wire [11:0] col_address = {1'b0, q_col};
wire [11:0] row_address = {3'b0, q_row[8:0]};
wire [19:0] q_row;
wire [10:0] q_col;
div div_u1(.denom(11'd1280),.numer(cpu_Address),.quotient(q_row),.remain(q_col));

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
// CPU write through cache 
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
wire [27:0] cache_input = {cpu_Address, cpu_Data_i};
wire        cache_full;
wire [11:0] cache_wrdw;
wire        wr_wait     = (cache_wrdw > 12'hFFA) & cpu_Wen;
wire 			wrb_clk 		= ~cpu_Wen;   					// CPU write byte clock
wire 			wrb_cs  		= 1'b1;                  	// CPU write byte Chip select

wire [27:0] cache_q;	
wire        cache_empty;
reg         cache_req;

cache	cache_u1(
	.data    ( cache_input ),
	.wrfull  ( cache_full ),
   .wrusedw ( cache_wrdw ),
	.wrclk   ( wrb_clk ),
	.wrreq   ( wrb_cs ),

	.rdclk   ( clk_100 ),
	.rdreq   ( cache_req ),
	.q       ( cache_q ),
	.rdempty ( cache_empty )
	);  
	
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
// LCD Controller section
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
wire [ 7:0] cache_data = cache_q[ 7: 0];        // Cache output Data 
wire [11:0] cache_col  = {1'b0, ca_q_col};
wire [11:0] cache_row  = {3'b0, ca_q_row[8:0]};

wire [19:0] ca_q_row;
wire [10:0] ca_q_col;
div div_u2(.denom(11'd1280),.numer(cache_q[27: 8]),.quotient(ca_q_row),.remain(ca_q_col));

//-------------------------------------------------------------------------------------------------
//  LCD driving - Horizaontal and verical sync pulses
//-------------------------------------------------------------------------------------------------
parameter Red          = 2'b00;                     // Red pane
parameter Green        = 2'b01;                     // Green pane
parameter Blue         = 2'b10;                     // Blue pane

parameter DispWidth    = 640;                       // Set to Width  of display frame being used
parameter DispHeight   = 480;                       // Set to Width  of display frame being used

parameter DataWidth    = DispWidth*2;               // Set to Width  of display frame being used
parameter DataHeight   = 480;                       // Set to Width  of display frame being used

parameter FrameWidth   = 800;                       // Set to Width  of display frame being used
parameter FrameHeight  = 525;                       // Set to Height of display frame being used

parameter HSyncLow     = DispWidth-1;               // Horz count for Sync to go low
parameter HSyncHigh    = FrameWidth-1;              // Horz count for Sync to go high
parameter VSyncLow     = DispHeight-1;              // Vert count for Sync to go low
parameter VSyncHigh    = FrameHeight-1;             // Vert count for Sync to go high

wire      CounterHmaxed= (CounterH == FrameWidth-1);   // Frame Width
wire      CounterVmaxed= (CounterV == FrameHeight-1);  // Frame Height
wire      lcd_hsync    = (CounterH < DispWidth);       // HSync low
wire      lcd_vsync    = (CounterV < DispHeight);      // VSync low
assign    lcd_de       = lcd_hsync & lcd_vsync;        // LCD Data enable line
assign    lcd_xclk     = xclk;                         // LCD pixel clock

reg  [9:0] CounterH;                                // For Horizontal Direction
reg  [9:0] CounterV;                                // For Vertical

//-------------------------------------------------------------------------------------------------
//  Pixel Clock Generation:
//  Frame Rate = (pclk  / divisor / Frame Width / Frame Height
//       60hz = 25Mhz / 1       / 800         / 525              xclk = CounterP;    (25.00 Mhz)
//       30hz = 25Mhz / 2       / 800         / 525              xclk = CounterP[0]; (12.50 Mhz) 
//       15hz = 25Mhz / 4       / 800         / 525              xclk = CounterP[1]; ( 6.25 Mhz)
//-------------------------------------------------------------------------------------------------
reg  [1:0] CounterP;                                // For dividing clock speed down
wire   xclk     =  CounterP[1];                     // Divide main clock down for pixel rate
always @(posedge pclk) CounterP <= CounterP + 2'b1; // increment pixel counter

always @(posedge xclk) begin
    if(CounterHmaxed) CounterH <= 10'd0;            // Always for Horizontal counter
    else              CounterH <= CounterH + 10'd1; // Increment Horizontal Counter to next location
end
always @(posedge xclk) begin                        // Always for vertical counter
    if(CounterHmaxed) begin
        if(CounterVmaxed) CounterV <= 10'd0;            // Check to see if we are finished with a frame
        else              CounterV <= CounterV + 10'd1; // Goto the next line in the frame
    end
end

//-------------------------------------------------------------------------------------------------
//  Triple barrel double FIFO buffer pipeline
//-------------------------------------------------------------------------------------------------
wire         rd_clk_en     = lcd_de;                          // Read clock enable line
wire         rd_clk        = xclk & rd_clk_en;                // Read clock opposes pixel clock
wire         VertData      = lcd_vsync;                       // Valid Vertical data
wire         FIFOReq       = (CounterH > FrameWidth-128);     // When to start re-loading the buffer
wire         DRAMReq       = FIFOReq & VertData;              // DRAM request
wire         CounterAmaxed = (dram_Address  == DataWidth-1);  // Data Width Count

wire   [9:0] rd_addr = CounterH[9:0];                              // Read address is lower bits of horz cntr
reg          wreq;                                                 // write request flag
reg   [10:0] wr_addr;                                              // Fifo buffer write address
wire  [15:0] lcd_data;
ram1 RAM_u1(.rdaddress(rd_addr),.rdclock(rd_clk),.q(lcd_data),.wraddress(wr_addr),.wrclock(clk),.wren(wreq),.data(dram_Data));

//-------------------------------------------------------------------------------------------------
// RGB565 bit layout:  1111110000000000
//                     5432109876543210
//                     RRRRRGGGGGGBBBBB
//-------------------------------------------------------------------------------------------------
assign lcd_r = {lcd_data[ 4: 0], lcd_data[ 0]};   // Red bits
assign lcd_g =  lcd_data[10: 5];                  // Green bits
assign lcd_b = {lcd_data[15:11], lcd_data[11]};   // Blue bits

//-------------------------------------------------------------------------------------------------
// Test Pattern
//-------------------------------------------------------------------------------------------------
//wire [5:0] dram_Data1 = CounterV[6:1];
//wire [5:0] dram_Data2 = wr_addr[7:2];
//wire [5:0] dram_Data3 = CounterV[5:0];
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//    State Machine to Handle DRAM Access
//-------------------------------------------------------------------------------------------------
reg  [ 4:0] DRAMState;                      // State Machine Variable
reg  [ 4:0] NextState;                      // Next machine state
reg  [10:0] refcnt;                         // Referesh counter
//-------------------------------------------------------------------------------------------------
always @(negedge clk) begin
    if(reset) DRAMState <= 5'd00;           // Start with a zero count
    else      DRAMState <= NextState;
end
//-------------------------------------------------------------------------------------------------
always @(posedge clk)
if(reset) begin                             // If reset line is low, then set initial coditions
    dram_Data     <= 8'bZZZZZZZZ;           // Hi-Z The data bus to let DRAM output data
    dram_WE       <=  1'b1;                 // Start DRAM in Read mode
    dram_CAS      <=  1'b1;                 // Put Cas in normal
    dram_RAS      <=  1'b1;                 // Put Ras in normal
    dram_Address  <= 12'd0;                 // Start at 0,0
    cache_req     <=  1'b0;                 // Cache read request clear
    read_rdy      <=  1'b0;                 // Read ready clear
end
else begin                                  // If reset line is high, then run the machines
    case(DRAMState)                         // State Machine Case
        //-----------------------------------------------------------------------------------------
        // Load up FIFO buffers during horizontal blanking time: 
		  // (640*2)*2 clk ticks + 32 @ 100mhz => 0.01us x 2,592 = 25.92us   ~ 26us
        //  1 Line time = 800 XClk ticks @ 25mhz/2 => 0.08us x 800 = 64us
		  //  1 Line time = 800 XClk ticks @ 25mhz/4 => 0.16us x 800 = 128us
		  //  26us / .16us = 162.5 ~ 164 
        //-----------------------------------------------------------------------------------------
        5'd00: begin                                 // Initial State
            if(DRAMReq) NextState <= 5'd01;          // Request to fill a buffer
            else        NextState <= 5'd06;          // Step to next state on next clock cycle
        end                                          // End State 
        5'd01: begin                                 // Handle State 
            dram_Address <= {2'b00,CounterV};        // Load our new line number into DRAM
            NextState    <= 5'd02;                   // Step to next state on next clock cycle
        end                                          // End State
        5'd02: begin                                 // Handle State
            dram_RAS  <= 1'b0;                       // Pulse Row Address into DRAM
            NextState <= 5'd03;                      // Step to next state on next clock cycle
        end                                          // End State
        5'd03: begin                                 // Handle State
            dram_Address <= 12'd0;                   // Start at begining of the row
            wr_addr      <= 10'b1;                   // Put the Horizontal address count into DRAM
            wreq         <=  1'b1;                   // Request a write to the display pipeline
            NextState    <= 5'd04;                   // Step to next state on next clock cycle
        end                                          // End State
        5'd04: begin                                 // Handle State
            dram_CAS  <= 1'b0;                       // Set CAS Low, DRAM data should be present on Data bus on next clk
            NextState <= 5'd05;                      // Step to next state on next clock cycle
        end                                          // End State
        5'd05: begin                                 // Handle State
            dram_CAS <= 1'b1;                        // Set CAS Low, DRAM data should be present on Data bus on next clk
            wr_addr  <= wr_addr + 10'd1;             // Increment Horizontal Counter to next location
            dram_Address <= dram_Address + 12'd1;    // Increment Horizontal Counter to next location
            if(CounterAmaxed) NextState <= 5'd06;    // Step to next state on next clock cycle
            else              NextState <= 5'd04;    // Loop back to previous state on next clock cycle
        end                                          // End State 

        //-----------------------------------------------------------------------------------------
        // Handle any new data coming in from cache or read request
        //-----------------------------------------------------------------------------------------
        5'd06: begin                                 // Handle State
            wreq <= 1'b0;                            // Exit DRAM FIFO mode
            dram_RAS <= 1'b1;                        // DRAM Page mode by bringing RAS high
            if(cache_empty) NextState <= 5'd16;      // If cache is empty go check for a read or refresh
				else begin                               // Else something is in the cache
                cache_req <= 1'b1;                   // Request a read from cache
				    NextState <= 5'd07;                  // Go process next state
            end                                      // end if
        end                                          // End State
        5'd07: begin                                 // Handle State
            cache_req <= 1'b0;                       // Clear request a read from request
            dram_Address <= cache_row;               // Load the previoulsy loaded user address
            NextState <= 5'd08;                      // Step to next state on next clock cycle
        end                                          // End State
        5'd08: begin                                 // Handle State
				dram_RAS <= 1'b0;                        // Return Ras to 1 to exit page mode
            NextState <= 5'd09;                      // Step to next state on next clock cycle
        end                                          // End State
        5'd09: begin                                 // Handle State
            dram_Address <= cache_col;               // Start at front of buffer
            dram_WE  <= 1'b0;                        // Set RW mode
            NextState <= 5'd11;                      // Step to next state on next clock cycle
        end                                          // End State 
        5'd11: begin                                 // Handle State 
            dram_Data <= cache_data;                 // Load cached byte to DRAM 
            dram_CAS <= 1'b0;                        // Pulse Column Address into DRAM Column register
            NextState <= 5'd12;                      // Step to next state on next clock cycle
        end                                          // End State 
        5'd12: begin                                 // Handle State 
            dram_CAS <= 1'b1;                        // Pulse Column Address into DRAM Column register
            NextState <= 5'd13;                      // Step to next state on next clock cycle
        end                                          // End State 
        5'd13: begin                                 // Handle State 
            dram_RAS <= 1'b1;                        // Return Ras to 1 to exit page mode
            dram_WE  <= 1'b1;                        // Always leave in read mode
            NextState <= 5'd14;                      // Step to next state on next clock cycle
        end                                          // End State 
        5'd14: begin                                 // Handle State 
            dram_Data <= 8'bZZZZZZZZ;                // If read mode, then Hi-Z The data bus to let DRAM output data
            NextState <= 5'd31;                      // Step to next state on next clock cycle
        end                                          // End State 

        //-----------------------------------------------------------------------------------------
        // Check for read request
        //-----------------------------------------------------------------------------------------
        5'd16: begin                                 // Handle State 
            if(read_req) begin                       // Check to see if user read is pending
                dram_Address <= row_address;         // Load the previoulsy loaded user address
				    NextState <= 5'd17;                  // If cache is empty go check for a read or refresh
            end                                      // End if
		      else NextState <= 5'd31;                 // Else something is in the cache
        end                                          // End State 
        5'd17: begin                                 // Handle State
				dram_RAS <= 1'b0;                        // Ras to 0 to clock in Row address
            NextState <= 5'd18;                      // Step to next state on next clock cycle
        end                                          // End State
        5'd18: begin                                 // Handle State
            dram_Address <= col_address;             // Load column address
            NextState <= 5'd19;                      // Step to next state on next clock cycle
        end                                          // End State 
        5'd19: begin                                 // Handle State 
            dram_CAS <= 1'b0;                        // Pulse Column Address into DRAM Column register
            NextState <= 5'd20;                      // Step to next state on next clock cycle
        end                                          // End State 
        5'd20: begin                                 // Handle State 
            cpu_Data_o <= dram_Data;                 // Load cached byte to DRAM 
            dram_CAS <= 1'b1;                        // Pulse Column Address into DRAM Column register
            NextState <= 5'd21;                      // Step to next state on next clock cycle
        end                                          // End State 
        5'd21: begin                                 // Handle State 
            dram_RAS <= 1'b1;                        // Return Ras to 1 to exit page mode
            dram_WE  <= 1'b1;                        // Always leave in read mode
            read_rdy <= 1'b1;                        // Read ready clear
            NextState <= 5'd31;                      // Step to next state on next clock cycle
        end                                          // End State 

        //-----------------------------------------------------------------------------------------
        //-----------------------------------------------------------------------------------------
        // Handle DRAM Refresh - need complete refresh every 64ms, burst every 15.6us
        //    2^ 9   375 /  25Mhz = 15.0
        //    2^10   750 /  50Mhz = 15.0  (15.6us per spec)
        //    2^11  1500 / 100Mhz = 15.0  
         //   
         //   2^10 = 1024 /  100Mhz = 10us :  burst about every 10us or so
        //-----------------------------------------------------------------------------------------
        5'd31: begin                                 // Handle Refresh State 
            case(refcnt)                             // Refresh machine
                11'h5: dram_CAS <= 1'b0;             // Pulse Column Address into DRAM Column register
                11'h6: ;                             // NOP
                11'h7: dram_RAS <= 1'b0;             // Return Ras to 1 to exit page mode
                11'h8: ;                             // NOP
                11'h9: ;                             // NOP
                11'hA: dram_CAS <= 1'b1;             // Pulse Column Address into DRAM Column register
                11'hB: ;                             // NOP
                11'hC: dram_RAS <= 1'b1;             // Return Ras to 1 to exit page mode
                default: NextState <= 5'd00;         // Step to next state on next clock cycle
            endcase                                  // End case refresh
            refcnt <= refcnt + 11'h1;                // Increment the refresh counter
        end                                          // End refresh state
        //-----------------------------------------------------------------------------------------
        default: NextState <= 5'd00;                 // Default machine state, defensive move
    endcase                                          // End of State Machine Case

    if(!read_req) read_rdy <=  1'b0;                  // Read ready clear
	 
end                                                  // End of Machine 

//-------------------------------------------------------------------------------------------------
endmodule 
//-------------------------------------------------------------------------------------------------
