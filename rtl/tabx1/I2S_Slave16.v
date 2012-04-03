//-------------------------------------------------------------------------------------------------
// I2S CODEC slave module: The I2S bus is over-sampled using the FPGA clock to allow the logic run 
// in the FPGA clock domain. Note FPGA clock must be at least 2x the I2S. Clock to synchronize 
// the signals using the FPGA clock and shift registers. 
//
//  I2S Interface structure: 16 bit interface, Bits are sent in most to least significant order
//  command, then address and finally data bits as follows:
//
//
//   +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+
// TK|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  | 
//   +  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+  +
//   
//   +  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+  +
// TF|  |                                                                                                  |  | 
//   +--+                                                                                                  +--+
//
//      |<------------------ Left Channel ------------->|<--------------- Right Channel --------------->|
//       15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
//   +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// TD|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  | 
//   +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//
//-------------------------------------------------------------------------------------------------
module I2S_slave16 (
   input 						clk,			// Main Clock
   input          	  		FRM,			// I2S Framing Input
   input       	     		BCK,			// I2S Sample bit clock input
   input    	     	   	DIN,			// I2S Serial audio data input
   output			[15:0]  	out_L,		// Left output
   output			[15:0]  	out_R,		// Right output
	output						ready_L,		// Signal that data is ready to be sent out
	output						ready_R 		// Signal that data is ready to be sent out
);

//-----------------------------------------------------------------------------
// Assign output 
//-----------------------------------------------------------------------------
assign out_L 		= data_L[15:0];		// Left Channel Data bits
assign out_R 		= data_R[15:0];		// Right Channel Data bits
assign ready_L		= FRM_Rise;				// Data is ready pulse
assign ready_R		= FRM_Fall;				// Data is ready pulse

//-----------------------------------------------------------------------------
// Sync BCK to the FPGA clock using a 3-bits shift register
//-----------------------------------------------------------------------------
reg [2:0] FRM_1;  
always @(posedge BCK) FRM_1 <= {FRM_1[1:0], FRM};
wire FRM_delayed = FRM_1[1];

//-----------------------------------------------------------------------------
// Sync FRM to the FPGA clock using a 3-bits shift register
//-----------------------------------------------------------------------------
reg [2:0] FRMr;  
always @(posedge clk) FRMr <= {FRMr[1:0], FRM_delayed};
wire FRM_Rise   = (FRMr[2:1] == 2'b01);  // Message starts at rising edge
wire FRM_Fall   = (FRMr[2:1] == 2'b10);  // Message stops at falling edge

//-----------------------------------------------------------------------------
// I2S receiver:
// This is a 32 bit I2S format, 16 bits data for each channel. 
// FPGA is only one slave on the bus so we don't bother with a tri-state buffer
// for MISO otherwise we would need to tri-state MISO when SSEL is inactive
//-----------------------------------------------------------------------------
reg  [16:0] data_R; 		                  // Shift register of output data
reg  [16:0] data_L; 		                  // Shift register of output data
always @(posedge BCK) begin
    if(FRM_delayed) begin                 // If Frame not active
        data_R <= {data_R[15:0], DIN};    // Input shift-left register 
    end
    else begin         
        data_L <= {data_L[15:0], DIN};   	// Input shift-left register 
    end
end
	  
//-----------------------------------------------------------------------------
endmodule
//-----------------------------------------------------------------------------



