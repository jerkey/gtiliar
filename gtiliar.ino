// hardware: put two 470Ω 1W resistors in series across "dump load" input
// put a 6.8uF capacitor from ADC1 of GTI to ground to smooth its current perception
// put a 2.55KΩ 1/4W resistor across relay contacts (maybe unnecessary with dump load?)
// TODO: write a routine triggered by "zero current, high voltage" to drain the caps
#define VERSIONSTRING   "Grid Tie Inverter Liar to fool GTI inputs according to difficulty knob"
#define CUTOUT_VOLTAGE  33.0    // turn off the inverter if the actual voltage is above this
#define CUTOUT_WARNING  (CUTOUT_VOLTAGE * 0.8)    // turn off the inverter if the actual voltage is above this
#define GTI_VSENSE      3       // output to GTI's brain through LPF and Ω/divider
//#define GTI_ISENSE      11      // output to GTI's brain through LPF and Ω/divider
#include <Adafruit_NeoPixel.h>
#define NEOPIXEL_PIN    7       // control a strip of addressible LEDs
#define NUM_LEDS        22      // how many LEDs in the ledstrip
Adafruit_NeoPixel ledstrip = Adafruit_NeoPixel(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
#define KNOB_INPUT      A4      // input from user control knob
#define VOLT_INPUT      A0      // read the actual DC input voltage
#define ISENSE_INPUT    A2      // read ADC1 from the GTI
#define ISENSE_COEFF    (1023./5./14.5)    // divide ACD reading by this to get amps
//#define VOLT_IN_COEFF   38.0    // divide ADC reading by this to get voltage
#define VOLT_IN_COEFF 13.179  // divide ADC reading by this to get voltage
#define GTI_VSENSE_COEFF   245.0    // mutiply desired voltage by this to get PWM value
// 2.55KΩ/10KΩ at pwm=73 we get 0.298v   at pwm=191 we get .779v
#define ADC_OVERSAMPLE  75      // average analog inputs over this many cycles
#define KNOB_SHUTOFF    0.01      // below this knob position, turn off inverter
#define KNOB_TURNON     0.025      // above this knob position, turn inverter on
#define INVERTER_TURNON 0.35    // signal voltage when inverter turns itself on
#define INVERTER_TURNON_PAUSE 100    // milliseconds to wait for inverter to see turnon voltage
#define INVERTER_FLOOR  0.29    // minimum signal voltage to keep inverter on
#define INVERTER_CEILING 0.75    // maximum signal voltage to keep inverter happy

byte lastPwmVal = 0; // remember what we wrote last
bool inverterOn = false; // whether the inverter was last on or off
float volt_input, amps_input, knob_input;

void setup() {
  ledstrip.begin();
  ledstrip.show(); // Initialize all pixels to 'off'
  Serial.begin(57600);
  Serial.println(VERSIONSTRING);
  pinMode(GTI_VSENSE,OUTPUT);
  //pinMode(GTI_ISENSE,OUTPUT);
  setPwmFrequency(GTI_VSENSE,1); // set PWM to high-frequency
  delay(1000);
  Serial.println("finish 1-second delay");
}

void loop() {
  volt_input = avgAnalogRead(VOLT_INPUT) / VOLT_IN_COEFF; // get input voltage
  amps_input = avgAnalogRead(ISENSE_INPUT) / ISENSE_COEFF; // get sensed amperage
  knob_input = avgAnalogRead(KNOB_INPUT) / 1023; // get knob position 0.0-1.0

  doLedStrip(amps_input * volt_input / 200.0); // animate LEDs according to wattage
  //doLedStrip(knob_input * 2); // animate LEDs according to knob

  if (volt_input > 20.0 && amps_input < 1.0 && knob_input > KNOB_TURNON) {
    Serial.println("TRIGGERING CAPACITOR CLEARING FUNCTION");
    for (int i=0; i<4; i++) {
      for (int j=0; j<10; j++) {
        setOutputVoltage(j*(INVERTER_CEILING-INVERTER_FLOOR)/10 + INVERTER_FLOOR);
        delay(25); // this times ten
      }
    }
  }

  if (inverterOn && knob_input < KNOB_SHUTOFF) { // if knob is turned down all the way
    inverterOn = false;
    setOutputVoltage(0); // tell inverter to turn off
  }
  if (!inverterOn && knob_input > KNOB_TURNON) { // if knob is turned up from all the way down
    inverterOn = true;
    setOutputVoltage(INVERTER_TURNON); // voltage to trigger inverter to turn on
    delay(INVERTER_TURNON_PAUSE); // wait for it to see it before going back to desired setting
  }

  float knob_value = (knob_input - KNOB_TURNON) * (1.0 + KNOB_TURNON); // normalize knob range from minimum
  float desiredInverterValue = volt_input / CUTOUT_VOLTAGE * knob_value; // here is where the magic happens
  if (volt_input < CUTOUT_VOLTAGE && inverterOn) {
    setOutputVoltage( desiredInverterValue*(INVERTER_CEILING-INVERTER_FLOOR) + INVERTER_FLOOR ); // full range
  } else {
    setOutputVoltage(0); // turn off inverter above safe voltages
  }

  Serial.print("\tReal voltage: ");
  Serial.print(volt_input,1);
  Serial.print("\tReal amps: ");
  Serial.print(amps_input,1);
  Serial.print("\tW: ");
  Serial.print(amps_input * volt_input,1);
  Serial.print("\tknob position: ");
  Serial.print(knob_input,4);
  Serial.print("\tpwmVal: ");
  Serial.print(lastPwmVal);
  Serial.print("\tOutputVoltage: ");
  Serial.print(lastPwmVal / GTI_VSENSE_COEFF,3);
  Serial.println();
  delay(100);
}

void doLedStrip(float quantity) {
  for(int i=0; i<ledstrip.numPixels(); i++) {
    if (volt_input > CUTOUT_WARNING && (millis() % 1000 > 500) && (i % 2 == 0)) { // blinking red
      ledstrip.setPixelColor(i, ledstrip.Color(64,0,0)); // bright red
    } else { // voltage is not above cutout warning
      if ((millis() % 1000) / 100 == i % 10) { // animation?
        byte brightness = constrain(quantity*255,2,255);
        ledstrip.setPixelColor(i, ledstrip.Color(brightness,brightness,brightness)); // white
      } else {
        ledstrip.setPixelColor(i, ledstrip.Color(2,2,2));
      }
    }
  }
  ledstrip.show();
}

void setOutputVoltage(float outputVoltage) {
  byte pwmVal = constrain((outputVoltage*GTI_VSENSE_COEFF),0,255);
  if (pwmVal != lastPwmVal) { // let's not analogWrite unnecessarily
    analogWrite(GTI_VSENSE,pwmVal);
    lastPwmVal = pwmVal;
  }
}

float avgAnalogRead(int pinNumber) {
  unsigned long adder = 0; // store ADC samples here
  for (int i=0; i < ADC_OVERSAMPLE; i++) adder += analogRead(pinNumber);
  return ((float)adder/ADC_OVERSAMPLE);
}

void setPwmFrequency(int pin, int divisor) {
  byte mode;
  if(pin == 5 || pin == 6 || pin == 9 || pin == 10) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 64: mode = 0x03; break;
      case 256: mode = 0x04; break;
      case 1024: mode = 0x05; break;
      default: return;
    }
    if(pin == 5 || pin == 6) {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    } else {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  } else if(pin == 3 || pin == 11) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 32: mode = 0x03; break;
      case 64: mode = 0x04; break;
      case 128: mode = 0x05; break;
      case 256: mode = 0x06; break;
      case 1024: mode = 0x07; break;
      default: return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
  }
}
