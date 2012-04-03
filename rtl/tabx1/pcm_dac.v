// --------------------------------------------------------------------
// Module:      Sound.v
// Description: Sound module.  This is NOT a sound blaster emulator. 
// This module produces simple sounds by implementing a simple interface,
// The user simply writes a byte of data to the left and/or right channels.
// Then poll the status register which will raise a flag when ready for the 
// next byte of data. Alternatively, it can generate an interupt to request
// the next byte.
//
// Sound uses 16 I/O addresses 0x0nn0 to 0x0nnF, nn can be anything
// 
// I/O Address  Description
// -----------  ------------------
//      0x0210  Left  Channel
//      0x0211  Right Channel
//      0x0212  High byte of timing increment
//      0x0213  Low byte of timing increment
//      0x0215  Control, 0x01 to enable interupt, 0x00 for polled mode
//      0x0217  Status, 0x80 when ready for next data, else 0x00
//
// --------------------------------------------------------------------
module  pcm_dac (
    input 				  clk,		// Main Clock
    input  		[15:0]  dac_L,		// Left DAC 
    input 		[15:0]  dac_R,		// Right DAC 
    input 				  wren_L,	// Write data to Left DAC
    input 				  wren_R,	// Write data to Right DAC
    output 				  audio_L,	// Left PCM audio output
    output 				  audio_R	// Right PCM audio output
);

// --------------------------------------------------------------------
// Pulse the DAC data into the DAC registers
// --------------------------------------------------------------------
  always @(posedge wren_L) begin
	  dsp_audio_l <= {~dac_L[15], dac_L[14:0]};
  end
 
  always @(posedge wren_R) begin
	  dsp_audio_r <= {~dac_R[15], dac_R[14:0]};
  end
 
// --------------------------------------------------------------------
// DAC CLock Generation Section:
// We need to divide down the clock for PCM, clk = 50Mhz
// then 50,000,000 / 256 = 195,312.5Hz which is ~2X highest CODEC rate (96Khz)
// 0 =   2 => 25,000,000.00 Hz
// 1 =   4 => 12,500,000.00 Hz
// 2 =   8 =>  6,250,000.00 Hz
// 3 =  16 =>  3,125,000.00 Hz
// 4 =  32 =>  1,562,500.00 Hz
// 5 =  64 =>    781,250.00 Hz
// 6 = 128 =>    390,625.00 Hz
// 7 = 256 =>    195,312.50 Hz
// 8 = 512 =>     97,656.25 Hz
// --------------------------------------------------------------------
  wire       dac_clk = clkdiv[4];
  reg  [8:0] clkdiv;
  always @(posedge clk) clkdiv <= clkdiv + 9'd1; 

// --------------------------------------------------------------------
// Audio Generation Section
// --------------------------------------------------------------------
  reg [15:0] dsp_audio_l;
  reg [15:0] dsp_audio_r;
  dac16 left (.clk(dac_clk),.DAC_in(dsp_audio_l),.audio_out(audio_L));
  dac16 right(.clk(dac_clk),.DAC_in(dsp_audio_r),.audio_out(audio_R));
  
// --------------------------------------------------------------------
endmodule
// --------------------------------------------------------------------

// --------------------------------------------------------------------
// Module:      DAC16.v
// Description: 16 bit pcm DAC
// --------------------------------------------------------------------
module dac16(
	input  		 clk,
	input [15:0] DAC_in,
	output 	 	 audio_out
);
	reg [16:0] DAC_Register;
	always @(posedge clk) DAC_Register <= DAC_Register[15:0] + DAC_in;
	assign audio_out = DAC_Register[16];

// --------------------------------------------------------------------
endmodule 
// --------------------------------------------------------------------
