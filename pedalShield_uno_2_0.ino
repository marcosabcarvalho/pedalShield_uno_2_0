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

// defining harware resources.
int LED = 13;
int FOOTSWITCH = 12;
int SIG_A = 0;      // pin A output
int SIG_B = 0;      // pin B output
int lastSIG_A = 0;  // last state of SIG_A
int lastSIG_B = 0;  // last state of SIG_B
int Pin_BTN = 3;    // GPIO #3-Push button on encoder
int Pin_A = 4;      // GPIO #4-Pin_A on encoder (Output A)
int Pin_B = 2;      // GPIO #5-Pin_B on encoder (Output B)

BfButton btn(BfButton::STANDALONE_DIGITAL, Pin_BTN, true, LOW);
U8GLIB_SSD1306_128X32 u8g(U8G_I2C_OPT_NONE);  // I2C / TWI

//defining the output PWM parameters
#define PWM_FREQ 0x00FF  // pwm frequency - 31.3KHz
#define PWM_MODE 0       // Fast (1) or Phase Correct (0)
#define PWM_QTY 2        // 2 PWMs in parallel

// other variables
int bank = 1;                                                          // Initial effect bank
int bank_max = 3;                                                      // Max number of effect banks
String bank_names[] = { "Clean", "Daft-Punk-Octaver", "Empty-Bank" };  // Effect Names
int pulseCount = 0;                                                    // Rotary pulses
int read_counter = 0;                                                  // Interrupt read counter
int ocr_counter = 0;                                                   // Output compare register counter
int input, vol_variable = 512;                                         // Signal from instrument
int dist_variable = 10;                                                // Effect ammount?
String iout;             // Debug string for interrupt output to serial
byte ADC_low, ADC_high;  // Analogue to Digital Converter low and high bytes

//Button press hanlding function
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
          dist_variable = 10;
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
      }
      break;
  }
}

void setup() {
  // Debug
  Serial.begin(9600);

  // Setup IO
  pinMode(FOOTSWITCH, INPUT_PULLUP);

  // setup ADC
  ADMUX = 0x60;   // left adjust, adc0, internal vcc
  ADCSRA = 0xe5;  // turn on adc, ck/32, auto trigger
  ADCSRB = 0x07;  // t1 capture for trigger
  DIDR0 = 0x01;   // turn off digital inputs for adc0

  // setup PWM
  TCCR1A = (((PWM_QTY - 1) << 5) | 0x80 | (PWM_MODE << 1));  //
  TCCR1B = ((PWM_MODE << 3) | 0x11);                         // ck/1
  TIMSK1 = 0x20;                                             // interrupt on capture interrupt
  ICR1H = (PWM_FREQ >> 8);
  ICR1L = (PWM_FREQ & 0xff);
  DDRB |= ((PWM_QTY << 1) | 0x02);  // turn on outputs
                                    // turn on interrupts - not really necessary with arduino

  //Setup initial state of rotary encoder
  SIG_B = digitalRead(Pin_B);  //current state of encoder pin B
  SIG_A = SIG_B > 0 ? 0 : 1;   //let them be different

  // Handle button presses
  btn.onPress(pressHandler)
    .onDoublePress(pressHandler)      // default timeout
    .onPressFor(pressHandler, 1000);  // custom timeout for 1 second

  u8g.setColorIndex(1);  // pixel on for Display

  // Excuse me!
  sei();
}

void loop() {
  // Turn on the LED if the effect is ON.
  if (digitalRead(FOOTSWITCH)) {
    digitalWrite(LED, HIGH);
    // Read Rotary Encoder Push Button
    btn.read();
    // Debug interrupt output
    Serial.println(iout);
  } else {
    digitalWrite(LED, LOW);
  }

  u8g.firstPage();
  do {
    u8g.setFont(u8g_font_profont12);
    draw(bank_names[bank - 1].c_str(), 0, 8);

    switch (bank) {
      case (1):
        u8g.setFont(u8g_font_unifont);
        draw("Volume:", 0, 30);
        draw(String(vol_variable).c_str(), 80, 30);
        break;

      case (2):
        u8g.setFont(u8g_font_unifont);
        draw("Effect:", 0, 30);
        draw(String(dist_variable).c_str(), 80, 30);
        break;
    }
  } while (u8g.nextPage());
  // nothing else here, all happens in the TIMER1_CAPT_vect interruption.
}

void draw(char* parola, int posx, int posy) {
  // graphic commands to redraw the complete screen should be placed here
  //u8g.setFont(font);
  //u8g.setFont(u8g_font_osb21);
  u8g.drawStr(posx, posy, parola);
}

ISR(TIMER1_CAPT_vect) {

  read_counter++;
  if (read_counter == 100) {  // encoder states are checked every 100 reads.
    read_counter = 0;

    // Check the rotary encoder for values
    SIG_A = digitalRead(Pin_A);  //read state of A
    SIG_B = digitalRead(Pin_B);  //read state of B

    /****************************************
    * Effect Banks
    *****************************************/
    if (bank == 1) {  // Clean
      // get ADC data
      ADC_low = ADCL;  // you need to fetch the low byte first
      ADC_high = ADCH;
      //construct the input sumple summing the ADC low and high byte.
      input = ((ADC_high << 8) | ADC_low) + 0x8000;  // make a signed 16b value

      if ((SIG_B == SIG_A) && (lastSIG_B != SIG_B)) {
        pulseCount--;
        if (vol_variable < 1024) vol_variable++;  //anti-clockwise rotation
        lastSIG_B = SIG_B;
      } else if ((SIG_B != SIG_A) && (lastSIG_B == SIG_B)) {
        pulseCount++;                                           //clockwise rotation
        if (vol_variable > 0) vol_variable = vol_variable - 1;  //clockwise rotation
        lastSIG_B = SIG_B > 0 ? 0 : 1;                          //save last state of B
      }
      //the amplitude of the signal is modified following the vol_variableusing the Arduino map fucntion
      input = map(input, 0, 1024, 0, vol_variable);
      //write the PWM signal
      OCR1AL = ((input + 0x8000) >> 8);  // convert to unsigned, send out high byte
      OCR1BL = input;                    // send out low byte

      iout = bank_names[bank - 1] + ": " + String(vol_variable);
    } else if (bank == 2) {  // Daft Punk Octaver
      if (!(SIG_B == SIG_A) && (lastSIG_B != SIG_B)) {
        pulseCount--;
        if (dist_variable > 0) dist_variable--;  //anti-clockwise rotation
        lastSIG_B = SIG_B;
      } else if (!(SIG_B != SIG_A) && (lastSIG_B == SIG_B)) {
        pulseCount++;                              //clockwise rotation
        if (dist_variable < 500) dist_variable++;  //clockwise rotation
        lastSIG_B = SIG_B > 0 ? 0 : 1;             //save last state of B
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
      iout = bank_names[bank - 1] + ": " + String(dist_variable);
    }
  } else if (bank == 3) {
    //TODO
    iout = bank_names[bank - 1];
  } else if (bank > bank_max) {  // whoops too far!
    bank = 0;
  }
}