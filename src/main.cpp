#include <Arduino.h>  // Remove this line for Arduino IDE

/*

Amiga 500 Keyboard Tested for Arduino Uno (ATmega328)

Heavily modified from 'AMIGA 500/1000/2000 Keyboard Interface' code by olaf:
https://forum.arduino.cc/t/amiga-500-1000-2000-keyboard-interface/136052

This version uses an Arduino Uno (ATmega328) instead of a Leonardo (ATmega32u4)
As such it does not translate Amiga keyboard events to USB HID reports since this is not possible on Uno.
Instead it just logs out the events via USB CDC (Serial) comms. 
i.e. it is useful only as a keyboard test tool.

LICENSE: MIT No Attribution

Copyright 2024 Neil Davis

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

/*

Connections:

KB Conn'            Uno
  Pin#    Purpose   Pin#
----------------------
  1       KB_CLK    D8
  2       KB_DAT    D9
  3       KB_RST    D10
  4       Vcc       5v
  5       NC (Key)  -
  6       GND       GND
  7       LED1      -
  8       LED2      -

*/

/*
  CONFIG:
*/

// Pins
#define KB_CLK        (1 << PB0)  // Uno D8
#define KB_DAT        (1 << PB1)  // Uno D9
#define KB_RESET      (1 << PB2)  // Uno D10
#define LED_BUILTIN_P (1 << PB5)  // Uno D13 (built in LED)

// Serial monitor baud rate
#define SERIAL_BAUD 115200

// How long to drive KB_DAT low for on handshake pulse (minimum 85 uS)
#define KDAT_HANDSHAKE_PULSE_MICROS 100

// Key code->string table
char *key_str[] = {
  // 00        01        02        03        04        05        06        07
    "~`",     "1!",     "2\"",    "3£",     "4£",     "5%",     "6^",     "7&",
  // 08        09        0A        0B        0C        0D        0E        0F
    "8*",     "9(",     "0)",     "-_",     "=+",     "\\|",    "<N/A>",  "KP 0",
  // 10        11        12        13        14        15        16        17
    "Q",      "W",      "E",      "R",      "T",      "Y",      "U",      "I",
  // 18        19        1A        1B        1C        1D        1E        1F
    "O",      "P",      "[{",     "]}",     "<N/A>",  "KP 1",    "KP 2",    "KP 3",
  // 20        21        22        23        24        25        26        27
    "A",      "S",      "D",      "F",      "G",      "H",      "J",      "K",
  // 28        29        2A        2B        2C        2D        2E        2F
    "L",      ";:",     "#@",     "R_BLANK","<N/A>",  "KP 4",    "KP 5",    "KP 6",
  // 30        31        32        33        34        35        36        37
    "L_BLANK","Z",      "X",      "C",      "V",      "B",      "N",      "M",
  // 38        39        3A        3B        3C        3D        3E        3F
    ",<",     ".>",     "/?",     "<N/A>"  ,"KP .",    "KP 7",    "KP 8",    "KP 9",
  // 40        41        42        43        44        45        46        47
    "SPACE",  "BS",     "TAB",    "KP ENT", "RETURN", "ESC",    "DEL",    "<N/A>",
  // 48        49        4A        4B        4C        4D        4E        4F
    "<N/A>",  "<N/A>",  "KP -",    "<N/A>",  "UP",     "DOWN",   "RIGHT",  "LEFT",
  // 50        51        52        53        54        55        56        57
    "F1",     "F2",     "F3",     "F4",     "F5",     "F6",     "F7",     "F8",
  // 58        59        5A        5B        5C        5D        5E        5F
    "F9",     "F10",    "KP (",    "KP )",    "KP /",    "KP *",    "KP +",    "HELP",
  // 60        61        62        63        64        65        66        67
    "L_SHIFT","R_SHIFT","CAPS",   "CTRL",   "L_ALT",  "R_ALT",  "L_AMIGA","R_AMIGA"
};

/*
  CONSTS
*/

// State machine
#define STATE_SYNCH_HI        0
#define STATE_SYNCH_LO        1
#define STATE_HANDSHAKE       2
#define STATE_READ            3
#define STATE_WAIT_LO         4 
#define STATE_WAIT_RST        5
//
char *state_str[] = {
  "STATE_SYNCH_HI",
  "STATE_SYNCH_LO",
  "STATE_HANDSHAKE",
  "STATE_READ",
  "STATE_WAIT_LO",
  "STATE_WAIT_RST"
};

// Special Key codes
#define KEY_CAPS_LOCK 0x62
#define KEY_AMIGA_R   0x67  // Highest possible 'normal' key code

// String formatting buffer maximum size in bytes
#define SERIAL_PRINTF_MAX_BUFF  256

/*
  GLOBAL VARS
*/

uint32_t kdat_handshake_micros = 0;
uint8_t state = STATE_SYNCH_HI, bitn = 7, key = 0;

/*
  CODE
*/

// Helper function for formatted serial printing
void serial_printf(const char *fmt, ...) {
  char buff[SERIAL_PRINTF_MAX_BUFF];
  va_list pargs;
  va_start(pargs, fmt);
  vsnprintf(buff, SERIAL_PRINTF_MAX_BUFF, fmt, pargs);
  va_end(pargs);
  Serial.print(buff);
}

// Caps lock state changed
void caps_lock(uint8_t caps_on) {
  if (caps_on) {
    // LED ON
    PORTB |= LED_BUILTIN_P; // set OUTPUT to HIGH
  } else {
      // LED OFF
      PORTB &= ~LED_BUILTIN_P; // set OUTPUT to LOW
  }
  serial_printf("CAPS LOCK: %s \n", (caps_on ? "ON" : "OFF"));
}

// Received a 'special' control code from the keyboard
void special_code(uint8_t special) {
  char buf[SERIAL_PRINTF_MAX_BUFF];
  switch (special) {
  case 0x78:  strncpy(buf, "RESET WARNING", SERIAL_PRINTF_MAX_BUFF);              break;
  case 0xf9:  strncpy(buf, "LOST SYNC", SERIAL_PRINTF_MAX_BUFF);                  break;
  case 0xfa:  strncpy(buf, "OUTPUT BUFFER OVERFLOW", SERIAL_PRINTF_MAX_BUFF);     break;
  case 0xfb:  strncpy(buf, "CONTROLLER FAIL", SERIAL_PRINTF_MAX_BUFF);            break;
  case 0xfc:  strncpy(buf, "SELF TEST FAIL", SERIAL_PRINTF_MAX_BUFF);             break;
  case 0xfd:  strncpy(buf, "BEGIN POWER UP KEY STREAM", SERIAL_PRINTF_MAX_BUFF);  break;
  case 0xfe:  strncpy(buf, "END POWER UP KEY STREAM", SERIAL_PRINTF_MAX_BUFF);    break;
  case 0xff:  strncpy(buf, "INTERRUPT", SERIAL_PRINTF_MAX_BUFF);                  break;
    default:  strncpy(buf, "(UNKNOWN)", SERIAL_PRINTF_MAX_BUFF);                  break;
  }
  serial_printf("SPECIAL: (0x%02x) %s\n", special, buf);
}

// Received a key event from the keyboard
void keypress(uint8_t k, uint8_t keydown) {
  serial_printf("KEY %s: (0x%02x) %s\n", (keydown ? "DOWN" : "  UP"), k, key_str[k]);
}

// State machine state change
void new_state(uint8_t state_new) {
  if (state_new != state) {
    //serial_printf("State change: %d -> %d: %s -> %s\n", state, state_new, state_str[state], state_str[state_new]);
    state = state_new;
  }
}

// Standard Arduino setup/init function
void setup() {
  // Init UART
  Serial.begin(SERIAL_BAUD);  
  Serial.println("Shall we begin?");
  // Keyboard (Port B)
  DDRB = ~(KB_CLK | KB_DAT | KB_RESET); // KB_CLK, KB_DAT, KB_RESET as inputs. Others (LED_BUILTIN_P) as output.
  PORTB = (KB_CLK | KB_DAT | KB_RESET); // PULL UP all inputs and drive outputs (LED_BUILTIN_P) LOW
  
}

// Standard Arduino loop function
void loop() {
  // Run the state machine ... 
  if (((PINB & KB_RESET) == 0) && STATE_WAIT_RST != state) {
    // Start of reset sequence
    interrupts();
    Serial.println("RESET: start");
    new_state(STATE_WAIT_RST);
  } else if (STATE_WAIT_RST == state) {
    // Waiting for reset end
    if ((PINB & KB_RESET) != 0) {
      Serial.println("RESET: end");
      new_state(STATE_SYNCH_HI);
    }
  } else if (STATE_SYNCH_HI == state) {
    // Wait for KB_CLK FALLING edge to sync on any char 
    // (keyboard will clock out '1' bits continuously until handshake received)
    if ((PINB & KB_CLK) == 0) {
      new_state(STATE_SYNCH_LO);
    }
  } else if (STATE_SYNCH_LO == state) {
    // Wait for KB_CLK RISING edge to sync on any char 
    // (keyboard will clock out '1' bits continuously until handshake received)
    if ((PINB & KB_CLK) != 0) {
      new_state(STATE_HANDSHAKE);
    }
  } else if (STATE_HANDSHAKE == state) {
    // KB_DAT Handshake - we need to pulse KB_DAT low for >= 85us
    if (0 == kdat_handshake_micros) {
      DDRB |= KB_DAT;   // set IO direction to OUTPUT
      PORTB &= ~KB_DAT; // set OUTPUT to LOW
      kdat_handshake_micros = micros();
    }
    else if (micros() - kdat_handshake_micros > KDAT_HANDSHAKE_PULSE_MICROS) {
      kdat_handshake_micros = 0;
      PORTB |= KB_DAT;  // Pull up
      DDRB &= ~KB_DAT;  // set IO direction to INPUT
      new_state(STATE_WAIT_LO);
      key = 0;
      bitn = 7;
    }
  }  else if (STATE_WAIT_LO == state) {
    // Waiting for KB_CLK FALLING edge to begin reading next bit
    if ((PINB & KB_CLK) == 0) {
      noInterrupts();
      new_state(STATE_READ);
    }
  } else if (STATE_READ == state) {
    // Waiting for KB_CLK RISING edge and then read the next bit from KB_DAT
    // Read 8-bit code. MSB-to-LSB order, but rotated so that MSB is last. i.e. 7-6-5-4-3-2-1-8
    if ((PINB & KB_CLK) != 0) {  
      if (bitn--){  // Read bits 7..1 in order
        uint8_t thisBit = ((PINB & KB_DAT) == 0);
        key |= (thisBit << bitn);
        new_state(STATE_WAIT_LO);
      }
      else {  // Read bit 8 last
        uint8_t bit8 = ((PINB & KB_DAT) == 0);
        if (KEY_CAPS_LOCK == key) {
          // Caps lock is handled differently. Only sent on key DOWN.
          // bit 8 is on(0)/off(1) NOT up/down
          caps_lock(bit8 == 0);
        } else if (key <= KEY_AMIGA_R) {
          // Standard key up/down. bit 8 is up(1)/down(0)
          keypress(key, (bit8 == 0));
        } else {
          // Special codes. These are not 'bit rotated' so move bit 8 to key MSB
          uint8_t special = key | (bit8 << 7);
          special_code(special);
        }
        new_state(STATE_HANDSHAKE);
        interrupts();
      }
    }
  }
}
    
