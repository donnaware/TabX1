TabX1 Readme and Description:
---------------------------------------------------------------------
TabX1 is my first attempt at creating a cool Linux based tablet 
computer. The idea behind it is to create a functional tablet at a
very low cost using a low cost low power SoC ARM chip and de-load
much of the processing to an attached FPGA chip. 



Directory tree:
---------------------------------------------------------------------
design			main design files
pcb			pcb schematic and artwork
rtl			verilog code
rtl\tabx1		tabx1 system
src			source code files
src\taboot		FPGA Boot up and Battery controller
src\linux		Custom Linux drivers for TabX1
src\linux\framebuffer	Custom Linux driver for TabX1 LCD graphics
src\linux\mouse		Custom Linux driver mouse/keyboard
src\linux\sound		Custom Linux driver for the sound module
src\linux\utilities	Linux utilities to control TabX1 FPGA


Verilog module list:


File		Module Description
---------------------------------------------------------------------
Tabx1.v		Top Level Module
pll.v		Phase Locked Loop
lcd.v		Custom LCD graphics controller
vdu.v		Text mode only VGA
Sound.v		Sound Module
PS2_mouse.v	PS2 Keyboard and Mouse Controller
Serial.v	RS232 Serial Controller
