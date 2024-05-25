# Amiga A500 Keyboard Tester (Arduino Uno) #

## Overview ##

A small utility to test an [Amiga A500](https://en.wikipedia.org/wiki/Amiga_500)
keyboard using an [Arduino Uno](https://docs.arduino.cc/hardware/uno-rev3/)
with serial output over USB. 
The serial output will display all
[key scan codes](http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node017A.html) (up/down) and
['special' control codes](http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node017B.html)
in both hexadecimal and human readable form.
The 'Caps Lock' LED indicator on the keyboard will also be reflected by the Uno's on board LED.


Based on information available from the
[Amiga Hardware Reference Manual](http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0172.html)
and [code by olaf](https://forum.arduino.cc/t/amiga-500-1000-2000-keyboard-interface/136052).
Unlike that code, this project does not translate amiga keys to USB HID for PC keyboard compatibility.
The Uno's [ATmega328](https://www.microchip.com/en-us/product/ATmega328)
MCU lacks the required on-chip support for USB that the
[Leonardo](https://docs.arduino.cc/hardware/leonardo/)'s
[ATmega32u4](https://www.microchip.com/en-us/product/ATmega32U4) has.
Therefore this utility is useful only as an external tool for testing Amiga keyboards.
Still I found this useful to test a keyboard MCU replacement build prior to plugging into a working Amiga
if you don't have a Leonardo board to re-purpose one of the many existing USB HID adapter sketches.

## Hardware Connections ##

Make connections from the A500 Keyboard connector to the Arduino Uno as follows:
Note the position of the 'key' which is the pin without a wire in the keyboard
[Molex connector](https://eab.abime.net/showthread.php?t=96725).

|KB Conn' Pin | Purpose | Uno Pin |
|-------------|---------|---------|
  1           | KB_CLK  | D8      |
  2           | KB_DAT  | D9      |
  3           | KB_RST  | D10     |
  4           | Vcc     | 5v      |
  5           | NC (Key)|  -      |
  6           | GND     | GND     |
  7           | LED1    |  -      |
  8           | LED2    |  -      |

Connect your Arduino UNO to your PC using the USB cable as normal.

## Software Environment ##

This tool was developed using the [PlatformIO](https://platformio.org/) extension for
[Visual Studio Code (VScode)](https://code.visualstudio.com/).
It will also work as an [Arduino IDE](https://www.arduino.cc/en/software) sketch
by renaming [```main.cpp```](/src/main.cpp) to e.g. ```main.ino```

Build and upload to your Uno over USB using the PlatformIO IDE (or Arduino IDE as a sketch).
Alternatively, a prebuilt [binary firmware](/build/firmware.hex) is available if you wish 
to flash directly using e.g. [avrdude](https://github.com/avrdudes/avrdude)
in which case I'm going to assume that you know what you are doing :)

Then use the IDE "serial monitor" to view the output.
Alternatively use your choice of [terminal emulator](https://en.wikipedia.org/wiki/Terminal_emulator)
software. By default the code is setup to use `115200` baud (8 data bits, no parity, 1 stop bit)
The baud rate can be changed in the code on this line:

```C
// Serial monitor baud rate
#define SERIAL_BAUD 115200
```

