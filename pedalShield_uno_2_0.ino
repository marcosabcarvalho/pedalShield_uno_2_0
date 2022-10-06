// pedalSheild UNO 2.0
// James Shawn Carnley
// https://github.com/wrenchpilot

// CC-by-www.Electrosmash.com
// Based on OpenMusicLabs previous works.

// Convert existing pedalShield UNO design and code
// to use a Rotary Encoder instead of push buttons
// and a toggle switch.  Also add a TFT display
// to show current effect name and effect ammount.

#include <BfButton.h>
#include <U8glib.h>
#include "FastMap.h"

// defining harware resources.
int LED = 13;
int FOOTSWITCH = 12;
int Pin_BTN = 3;  // Push button on encoder
int Pin_A = 4;    // Output A
int Pin_B = 2;    // Output B

// Initialize libraries
BfButton btn(BfButton::STANDALONE_DIGITAL, Pin_BTN, true, LOW);  // Push button event listener
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE);                     // Setup Display
FastMap mapper;                                                  // map() is slow af

//defining the output PWM parameters
#define PWM_FREQ 0x00FF  // pwm frequency - 31.3KHz
#define PWM_MODE 0       // Fast (1) or Phase Correct (0)
#define PWM_QTY 2        // 2 PWMs in parallel

// other variables
int SIG_A = 0;                        // pin A output
int SIG_B = 0;                        // pin B output
int lastSIG_A = 0;                    // last state of SIG_A
int lastSIG_B = 0;                    // last state of SIG_B
int pulseCount = 0;                   // Rotary pulses
int read_counter = 0;                 // Interrupt read counter
int ocr_counter = 0;                  // Output compare register counter
int input;                            // Signal from instrument
int vol_variable = 512;               // Volume
int dist_variable = 250;              // Effect ammount?
int distortion_threshold = 16384;      // distortion threshold
String normalized_output;             // Map output display to a 0-100 scale
byte ADC_low, ADC_high;               // Analogue to Digital Converter low and high bytes
int bank = 1;                         // Initial effect bank
int bank_max = 4;                     // Max number of effect banks
String bank_names[] = { "C L E A N",  // Effect Names
                        "O C T A V E R",
                        "D I S T O R T I O N",
                        "E M P T Y  B A N K" };

// Button press hanlding function
void pressHandler(BfButton* btn, BfButton::press_pattern_t pattern) {
  switch (pattern) {
    case BfButton::SINGLE_PRESS:  // Bank Scroll
      bank++;
      break;

    case BfButton::DOUBLE_PRESS:  // Reset Effect
      switch (bank) {
        case (1):
          vol_variable = 512;
          break;

        case (2):
          dist_variable = 250;
          break;

        case (3):
          distortion_threshold = 16384;
          break;
      }
      break;

    case BfButton::LONG_PRESS:  // Max Effect
      switch (bank) {
        case (1):
          vol_variable = 1024;
          break;

        case (2):
          dist_variable = 500;
          break;

        case (3):
          distortion_threshold = 32767;
          break;
      }
      break;
  }
}

void setup() {
  // Setup IO
  pinMode(FOOTSWITCH, INPUT_PULLUP);

  // Setup display
  u8g.setColorIndex(1);

  // Setup initial state of rotary encoder
  SIG_B = digitalRead(Pin_B);  //current state of encoder pin B
  SIG_A = SIG_B > 0 ? 0 : 1;   //let them be different

  // Handle button presses
  btn.onPress(pressHandler)
    .onDoublePress(pressHandler)      // default timeout
    .onPressFor(pressHandler, 1000);  // custom timeout for 1 second

  // setup ADC
  ADMUX = 0x60;   // left adjust, adc0, internal vcc
  ADCSRA = 0xe5;  // turn on adc, ck/32, auto trigger
  ADCSRB = 0x07;  // t1 capture for trigger
  DIDR0 = 0x01;   // turn off digital inputs for adc0

  // setup PWM
  TCCR1A = (((PWM_QTY - 1) << 5) | 0x80 | (PWM_MODE << 1));  //
  TCCR1B = ((PWM_MODE << 3) | 0x11);                         // ck/1
  TIMSK1 = 0x20;                                             // interrupt on capture interrupt
  ICR1H = (PWM_FREQ >> 8);                                   //
  ICR1L = (PWM_FREQ & 0xff);                                 //
  DDRB |= ((PWM_QTY << 1) | 0x02);                           // turn on outputs
                                                             // turn on interrupts - not really necessary with arduino
  // Excuse me!
  sei();
}

void loop() {

  if (digitalRead(FOOTSWITCH)) {  // STOMP++ :)
    digitalWrite(LED, HIGH);      // Turn on the LED if the effect is ON.

    // Read Rotary Encoder Push Button
    btn.read();

    // Check the rotary encoder for values
    SIG_A = digitalRead(Pin_A);  //read state of A
    SIG_B = digitalRead(Pin_B);  //read state of B

    u8g.firstPage();
    do {
      u8g.setFont(u8g_font_profont12r);
      draw(bank_names[bank - 1].c_str(), 0, 12);
      switch (bank) {
        case (1):
          u8g.setFont(u8g_font_profont29);
          mapper.init(0, 1024, long(0), long(100));
          draw("VOLUME", 0, 35);
          normalized_output = String(mapper.map(float(vol_variable)));
          break;

        case (2):
          u8g.setFont(u8g_font_profont29);
          mapper.init(0, 500, long(0), long(100));
          draw("AMOUNT", 0, 35);
          normalized_output = String(mapper.map(float(dist_variable)));
          break;

        case (3):
          u8g.setFont(u8g_font_profont29);
          mapper.init(0, 32767, long(0), long(100));
          draw("AMOUNT", 0, 35);
          normalized_output = String(mapper.map(float(distortion_threshold)));
          break;

        default:
          normalized_output = "";
      }

      draw(normalized_output.c_str(), 0, 57);
    } while (u8g.nextPage());
    // nothing else here, all happens in the TIMER1_CAPT_vect interruption.
  } else {
    digitalWrite(LED, LOW);  // --STOMP :(
  }
}

void draw(char* str, int posx, int posy) {
  u8g.drawStr(posx, posy, str);
}

ISR(TIMER1_CAPT_vect) {
  if (digitalRead(FOOTSWITCH)) {  // STOMP
    read_counter++;               //
    if (read_counter == 100) {    // do work every 100 reads.
      read_counter = 0;
      // Read Rotary Encoder Push Button
      btn.read();

      // Check the rotary encoder for values
      SIG_A = digitalRead(Pin_A);  //read state of A
      SIG_B = digitalRead(Pin_B);  //read state of B

      /****************************************
      * Effect Banks
      *****************************************/
      switch (bank) {
        /* C L E A N */
        case (1):
          // get ADC data
          ADC_low = ADCL;  // low byte first
          ADC_high = ADCH;

          //construct the input summing the ADC low and high byte.
          input = ((ADC_high << 8) | ADC_low) + 0x8000;  // make a signed 16b value

          if (!(SIG_B == SIG_A) && (lastSIG_B != SIG_B)) {
            pulseCount--;
            if (vol_variable > 0) vol_variable--;  //anti-clockwise rotation
            lastSIG_B = SIG_B;
          } else if (!(SIG_B != SIG_A) && (lastSIG_B == SIG_B)) {
            pulseCount++;  //clockwise rotation
            if (vol_variable < 1024) vol_variable++;
            lastSIG_B = SIG_B > 0 ? 0 : 1;  //save last state of B
          }

          //the amplitude of the signal is modified following the vol_variable using the Arduino map fucntion
          //UNTESTED! LOOOK AT ME! DON'T FORGET RE-VISIT THIS! GAY PR0N!
          input = mapper.map(float(vol_variable));
          //input = map(input, 0, 1024, 0, vol_variable);

          //write the PWM signal
          OCR1AL = ((input + 0x8000) >> 8);  // convert to unsigned, send out high byte
          OCR1BL = input;                    // send out low byte
          break;

        /* O C T A V E R */
        case (2):
          if (!(SIG_B == SIG_A) && (lastSIG_B != SIG_B)) {
            pulseCount--;
            if (dist_variable > 0) dist_variable--;  //anti-clockwise rotation
            lastSIG_B = SIG_B;
          } else if (!(SIG_B != SIG_A) && (lastSIG_B == SIG_B)) {
            pulseCount++;                              //clockwise rotation
            if (dist_variable < 500) dist_variable++;  //clockwise rotation
            lastSIG_B = SIG_B > 0 ? 0 : 1;
          }
          ocr_counter++;
          if (ocr_counter >= dist_variable) {
            ocr_counter = 0;

            // get ADC data
            ADC_low = ADCL;  // you need to fetch the low byte first
            ADC_high = ADCH;

            //construct the input sumple summing the ADC low and high byte.
            input = ((ADC_high << 8) | ADC_low) + 0x8000;  // make a signed 16b value

            OCR1AL = ((input + 0x8000) >> 8);  // convert to unsigned, send out high byte
            OCR1BL = input;                    // send out low byte
          }
          break;

        /* D I S T O R T I O N */
        case (3):

          // get ADC data
          ADC_low = ADCL;  // low byte first
          ADC_high = ADCH;

          //construct the input summing the ADC low and high byte.
          input = ((ADC_high << 8) | ADC_low) + 0x8000;  // make a signed 16b value

          if (!(SIG_B == SIG_A) && (lastSIG_B != SIG_B)) {
            pulseCount--;                                                                     // anti-clockwise rotation
            if (distortion_threshold > 0) distortion_threshold = distortion_threshold - 100;  // decrease the vol
            lastSIG_B = SIG_B;                                                                // save last state of B
          } else if (!(SIG_B != SIG_A) && (lastSIG_B == SIG_B)) {
            pulseCount++;                                                                         // clockwise rotation
            if (distortion_threshold < 32767) distortion_threshold = distortion_threshold + 100;  // increase the vol
            lastSIG_B = SIG_B > 0 ? 0 : 1;
          }

          // the input signal is 16bits (values from -32768 to +32767)
          // the value of input is clipped to the distortion_threshold value
          if (input > distortion_threshold) input = distortion_threshold;

          //write the PWM signal
          OCR1AL = ((input + 0x8000) >> 8);  // convert to unsigned, send out high byte
          OCR1BL = input;                    // send out low byte
          break;

          /* E M P T Y */
        case (4):
          break;

        /***********/
        default:  // whoops too far!
          bank = 1;
          break;
      }
    }
  }
}