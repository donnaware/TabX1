//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
// Design Name : PS2 Mouse module for tablet computer LCD support IP core
// File Name   : ps2_mouse.v
// Function    : TabX1 Computer Mouse Controller Module
// Description : This controller module implements logic for the TabX1 computer 
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//  NOTE:  make sure to change the mouse ports (mouse_clock, mouse_data) to
//		bi-directional 'inout' ports in the top-level module
//     inout  mouse_clock, mouse_data;
//
// This module interfaces to a mouse connected to PS2 port.
// The outputs provided give dx and dy movement values (9 bits 2's comp)
// and three button click signals.
//
// NOTE: change the following parameters for a different system clock
//	(current configuration for 50 MHz clock)
//		CLK_HOLD				: 100 usec hold to bring PS2_CLK low
//  		RCV_WATCHDOG_TIMER_VALUE : (PS/2 RECEIVER) 2 msec count
//  		RCV_WATCHDOG_TIMER_BITS  :  			bits needed for timer
//  		INIT_TIMER_VALUE		: (INIT process) 0.5 sec count
// 		INIT_TIMER_BITS		: 			bits needed for timer
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
module ps2_mouse(
	input					clock, 		// System Clock
	input					reset, 
	inout					ps2_clk, 	// Clock for PS/2 mouse
	inout					ps2_data, 	// Data for PS/2 mouse
	output reg [8:0]	dout_dx, 	// 9-bit 2's compl, indicates movement of mouse
	output reg [8:0] 	dout_dy, 	// 9-bit 2's compl, indicates movement of mouse
	output reg [1:0]	ovf_xy, 		// ==1 if overflow: dx, dy
	output reg [2:0] 	btn_click,  // button click: Left-Middle-Right
	output				ready, 		// synchronous 1 cycle ready flag
	output				streaming	// ==1 if mouse is in stream mode
);

  //---------------------------------------------------------------------------------------------
  // PARAMETERS
  // # of cycles for clock=50 MHz
  //---------------------------------------------------------------------------------------------
  parameter CLK_HOLD                 =     5000;	// 100 usec hold to bring PS2_CLK low
  parameter RCV_WATCHDOG_TIMER_VALUE =   100000; 	// For PS/2 RECEIVER : # of sys_clks for 2msec.
  parameter RCV_WATCHDOG_TIMER_BITS  =       17;   // 				 : bits needed for timer
  parameter INIT_TIMER_VALUE         = 25000000;	// For INIT process : sys_clks for 0.5 sec.(SELF-TEST phase takes several miliseconds)
  parameter INIT_TIMER_BITS          =       28;   // 			     : bits needed for timer
  //---------------------------------------------------------------------------------------------
  wire reset_init_timer;
  wire main_reset = (reset || reset_init_timer || hotplug);
  reg  hotplug;

  //---------------------------------------------------------------------------------------------
  //CONTROLLER:	
  //-initialization process:
  //	Host:  FF  Reset command
  //	Mouse: FA  Acknowledge
  //	Mouse: AA  Self-test passed
  //	Mouse: 00  Mouse ID
  //	Host:  F4  Enable
  //	Mouse: FA  Acknowledge
  //---------------------------------------------------------------------------------------------
  parameter SND_RESET  = 0;
  parameter RCV_ACK1   = 1;
  parameter RCV_STEST  = 2;
  parameter RCV_ID     = 3;
  parameter SND_ENABLE = 4;  
  parameter RCV_ACK2   = 5;
  parameter STREAM     = 6;
  //---------------------------------------------------------------------------------------------
  wire rcv_ACK = (key_ready && curkey==8'hFA);
  wire rcv_STP = (key_ready && curkey==8'hAA);
  
  //---------------------------------------------------------------------------------------------
  reg  [2:0] state;
  wire       send, ack;
  wire [7:0] packet;
  wire [7:0] curkey;
  wire       key_ready;

  //---------------------------------------------------------------------------------------------
  //NOTE: no support for scrolling wheel, extra buttons
  //---------------------------------------------------------------------------------------------
  always @(posedge clock) begin
	 if(main_reset) begin
	 	 state   <= SND_RESET;
		 hotplug <= 1'b0;
	 end
    else case(state)
       SND_RESET:		state <= ack       ? RCV_ACK1   	: state;
       RCV_ACK1:		state <= rcv_ACK   ? RCV_STEST  	: state;
       RCV_STEST:		state <= rcv_STP   ? RCV_ID     	: state;
       RCV_ID:			state <= key_ready ? SND_ENABLE 	: state;		//any device type
       SND_ENABLE:	state <= ack       ? RCV_ACK2   	: state;
       RCV_ACK2:		state <= rcv_ACK   ? STREAM     	: state;
		 STREAM:			state <= rcv_STP   ? hotplug<=1'b1: state;
	  default:			state <= SND_RESET;
    endcase
  end

  assign send      = (state==SND_RESET) || (state==SND_ENABLE);
  assign packet    = (state==SND_RESET) ? 8'hFF : (state==SND_ENABLE) ? 8'hF4 : 8'h00;
  assign streaming = (state==STREAM);

  //---------------------------------------------------------------------------------------------
  // Connect PS/2 interface module
  //---------------------------------------------------------------------------------------------
  ps2_interface ps2_mouse(
			.reset(main_reset), 
			.clock(clock),
  			.ps2c(ps2_clk), 
			.ps2d(ps2_data),
			.send(send), 
			.snd_packet(packet), 
			.ack(ack),
			.rcv_packet(curkey), 
			.key_ready(key_ready)  
	);
  defparam ps2_mouse.CLK_HOLD			      = CLK_HOLD;
  defparam ps2_mouse.WATCHDOG_TIMER_VALUE = RCV_WATCHDOG_TIMER_VALUE;
  defparam ps2_mouse.WATCHDOG_TIMER_BITS  = RCV_WATCHDOG_TIMER_BITS;

  //---------------------------------------------------------------------------------------------
  // DECODER
  //http://www.computer-engineering.org/ps2mouse/
  //			 bit-7				   3	      		bit-0
  //Byte 1:  Y-ovf  X-ovf  Y-sign  X-sign  1  Btn-M  Btn-R  Btn-L
  //Byte 2:  X movement
  //Byte 3:  Y movement
  //---------------------------------------------------------------------------------------------
  reg [1:0] bindex;
  reg [1:0] old_bindex;
  reg [7:0] dx;			//temporary storage of mouse status
  reg [7:0] dy;			//temporary storage of mouse status
  reg [7:0] status;		//temporary storage of mouse status

  always @(posedge clock) begin
  	if(main_reset) begin
	  bindex <= 0;
	  status <= 0;
	  dx     <= 0;
	  dy     <= 0;
	end else if(key_ready && state==STREAM) begin
	  case(bindex)
	  	2'b00:	status <= curkey;
		2'b01:	dx     <= curkey;
		2'b10:	dy     <= curkey;
		default:	status <= curkey;
	  endcase
	  
	  bindex <= (bindex == 2'b10) ? 2'b0 : bindex + 2'b1;
    	  if(bindex == 2'b10) begin					        //Now, dy is ready
				dout_dx   <= {status[4], dx};				     //2's compl 9-bit
				dout_dy   <= {status[5], curkey};			  //2's compl 9-bit
				ovf_xy    <= {status[6], status[7]};			 //overflow: x, y
				btn_click <= {status[0], status[2], status[1]};  //button click: Left-Middle-Right
       end
	end	//end else-if (key_ready)
  end

  always @(posedge clock) old_bindex <= bindex;

  assign ready = (bindex==2'b00) && old_bindex==2'b10;

  //---------------------------------------------------------------------------------------------
  // INITIALIZATION TIMER
  // 	==> RESET if processs hangs during initialization
  //---------------------------------------------------------------------------------------------
  reg [INIT_TIMER_BITS-1:0] init_timer_count;
  assign reset_init_timer = (state != STREAM) && (init_timer_count==INIT_TIMER_VALUE-1);
  always @(posedge clock) begin
	 init_timer_count <= (main_reset || state==STREAM) ? 0 : init_timer_count + 1;
  end

//-------------------------------------------------------------------------------------------------
endmodule
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// PS/2 INTERFACE: transmit or receive data from ps/2
//-------------------------------------------------------------------------------------------------
module ps2_interface(reset, clock, ps2c, ps2d, send, snd_packet, ack, rcv_packet, key_ready);
   input 		 clock,reset;
   inout 		 ps2c;				// ps2 clock	(BI-DIRECTIONAL)
   inout 		 ps2d;				// ps2 data	(BI-DIRECTIONAL)
   input 		 send;				//flag: send packet				_
   output       ack;				// end of transmission			 | for transmitting
   input  [7:0] snd_packet;	// data packet to send to PS/2	_|
   output [7:0] rcv_packet;	//packet received from PS/2		_
   output       key_ready;		// new data ready (rcv_packet)	_| for receiving

   //---------------------------------------------------------------------------------------------
   // MAIN CONTROL
   //---------------------------------------------------------------------------------------------
   parameter CLK_HOLD             =   5005;	// hold PS2_CLK low for 100 usec (@50 Mhz)
   parameter WATCHDOG_TIMER_VALUE = 100000; 	// Number of sys_clks for 2msec.	_
   parameter WATCHDOG_TIMER_BITS  =     17; 	// Number of bits needed for timer	_| for RECEIVER

   wire serial_dout;			//output (to ps2d)
   wire rcv_ack;				//ACK from ps/2 mouse after data transmission

   wire we_clk, we_data;

   assign ps2c = we_clk  ? 1'b0 : 1'bZ;
   assign ps2d = we_data ? serial_dout : 1'bZ;

   assign ack = rcv_ack;

   //---------------------------------------------------------------------------------------------
   // TRANSMITTER MODULE
   //---------------------------------------------------------------------------------------------

   //---------------------------------------------------------------------------------------------
   // COUNTER: 100 usec hold
   //---------------------------------------------------------------------------------------------
   reg [15:0] counter;
   wire en_cnt;
   wire cnt_ready;
   always @(posedge clock) begin
      counter <= reset ? 16'd0 : en_cnt ? counter+1 : 16'd0;
   end
   assign cnt_ready = (counter>=CLK_HOLD);
   
   //---------------------------------------------------------------------------------------------
   // SEND DATA
   //	hold CLK low for at least 100 usec
   //	DATA low
   //	Release CLK
   //	(on negedge of ps2_clock) - device brings clock LOW
   //		REPEAT:	SEND data
   //	Release DATA
   //	Wait for device to bring DATA low
   //	Wait for device to bring CLK low
   //	Wait for device to release CLK, DATA
   //---------------------------------------------------------------------------------------------
   reg [3:0] index;

   //---------------------------------------------------------------------------------------------
   // synchronize PS2 clock to local clock and look for falling edge
   //---------------------------------------------------------------------------------------------
   reg [2:0] ps2c_sync;
   always @ (posedge clock) ps2c_sync <= {ps2c_sync[1:0],ps2c};
   wire falling_edge = ps2c_sync[2] & ~ps2c_sync[1];
  
   always @(posedge clock) begin
   	if(reset) begin
			index <= 0;
		end
	else if(falling_edge) begin			//falling edge of ps2c
	   if(send) begin										//transmission mode
		if(index==0) index <= cnt_ready ? 1 : 0;		//index=0: CLK low
		else    	 	 index <= index + 1;					//index=1: snd_packet[0], =8: snd_packet[7],
														//      9: odd parity, =10: stop bit
														//	  11: wait for ack
	   end else index <= 0;
	end else    index <= (send) ? index : 0;
   end

   assign en_cnt = (index==0 && ~reset && send);
   assign serial_dout = (index==0 && cnt_ready) ? 0 :				//bring DATA low before releasing CLK
					(index>=1 && index <=8) ? snd_packet[index-1] :
					(index==9) ? ~(^snd_packet) :				//odd parity
							1;							//including last '1' stop bit

   assign we_clk = (send && !cnt_ready && index==0);				//Enable when counter is counting up
   assign we_data = (index==0 && cnt_ready) || (index>=1 && index<=9);//Enable after 100usec CLK hold
   assign rcv_ack = (index==11 && ps2d==0);						//use to reset RECEIVER module

   //---------------------------------------------------------------------------------------------
   // RECEIVER MODULE
   //---------------------------------------------------------------------------------------------
   reg 	[7:0] rcv_packet;		// current keycode
   reg			key_ready;		// new data
   wire        fifo_rd;			// read request
   wire [7:0]  fifo_data;		// data from mouse
   wire        fifo_empty;		// flag: no data
// wire        fifo_overflow;	// keyboard data overflow

   assign      fifo_rd = ~fifo_empty;	// continuous read

   always @(posedge clock)  begin  	// get key if ready
 	   rcv_packet <= ~fifo_empty ? fifo_data : rcv_packet;
   	key_ready  <= ~fifo_empty;
   end

   //---------------------------------------------------------------------------------------------
   // connect ps2 FIFO module
   //---------------------------------------------------------------------------------------------
   reg [WATCHDOG_TIMER_BITS-1 : 0] watchdog_timer_count;
   wire [3:0] rcv_count;				//count incoming data bits from ps/2 (0-11)

   wire watchdog_timer_done = watchdog_timer_count==(WATCHDOG_TIMER_VALUE-1);
   always @(posedge clock)  begin
   	if(reset || send || rcv_count==0) watchdog_timer_count <= 0;
  	   else if(~watchdog_timer_done)     watchdog_timer_count <= watchdog_timer_count + 1;
   end

   ps2 ps2_receiver(.clock(clock), .reset(!send && (reset || rcv_ack) ),	//RESET on reset or End of Transmission
  					.ps2c(ps2c), .ps2d(ps2d),
					.fifo_rd(fifo_rd), .fifo_data(fifo_data),		//in1, out8
					.fifo_empty(fifo_empty) , .fifo_overflow(),		//out1, out1
					.watchdog(watchdog_timer_done), .count(rcv_count) );

endmodule

//-------------------------------------------------------------------------------------------------
// PS/2 FIFO receiver module (from 6.111 Fall 2004)
//-------------------------------------------------------------------------------------------------
module ps2(reset, clock, ps2c, ps2d, fifo_rd, fifo_data, fifo_empty,fifo_overflow, watchdog, count);
  input clock,reset,watchdog,ps2c,ps2d;
  input fifo_rd;
  output [7:0] fifo_data;
  output fifo_empty;
  output fifo_overflow;
  output [3:0] count;

  reg [3:0] count;      // count incoming data bits
  reg [9:0] shift;      // accumulate incoming data bits

  reg [7:0] fifo[7:0];   // 8 element data fifo
  reg fifo_overflow;
  reg [2:0] wptr,rptr;   // fifo write and read pointers

  wire [2:0] wptr_inc = wptr + 3'd1;

  assign fifo_empty = (wptr == rptr);
  assign fifo_data = fifo[rptr];

  // synchronize PS2 clock to local clock and look for falling edge
  reg [2:0] ps2c_sync;
  always @ (posedge clock) ps2c_sync <= {ps2c_sync[1:0],ps2c};
  wire sample = ps2c_sync[2] & ~ps2c_sync[1];

  reg timeout;
  always @(posedge clock) begin
    if(reset) begin
      count   <= 0;
      wptr    <= 0;
      rptr    <= 0;
      timeout <= 0;
      fifo_overflow <= 0;
    end else if (sample) begin
      // order of arrival: 0,8 bits of data (LSB first),odd parity,1
      if(count==10) begin
        // just received what should be the stop bit
        if(shift[0]==0 && ps2d==1 && (^shift[9:1])==1) begin
          fifo[wptr] <= shift[8:1];
          wptr <= wptr_inc;
	  fifo_overflow <= fifo_overflow | (wptr_inc == rptr);
        end
        count <= 0;
        timeout <= 0;
      end else begin
        shift <= {ps2d,shift[9:1]};
        count <= count + 1;
      end
    end else if (watchdog && count!=0) begin
      if (timeout) begin
        // second tick of watchdog while trying to read PS2 data
	count <= 0;
        timeout <= 0;
      end else timeout <= 1;
    end

    // bump read pointer if we're done with current value.
    // Read also resets the overflow indicator
    if (fifo_rd && !fifo_empty) begin
      rptr <= rptr + 1;
      fifo_overflow <= 0;
    end
  end
//-------------------------------------------------------------------------------------------------
endmodule
//-------------------------------------------------------------------------------------------------
