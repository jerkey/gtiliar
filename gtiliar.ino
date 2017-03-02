#define VERSIONSTRING   "Grid Tie Inverter Liar to fool GTI inputs according to difficulty knob"
#define CUTOUT_VOLTAGE  32.0    // turn off the inverter if the actual voltage is above this
#define GTI_VSENSE      3       // output to GTI's brain through LPF and Ω/divider
//#define GTI_ISENSE      11      // output to GTI's brain through LPF and Ω/divider
#define KNOB_INPUT      A4      // input from user control knob
#define VOLT_INPUT      A0      // read the actual DC input voltage
#define ISENSE_INPUT    A5      // read ADC1 from the GTI
#define ISENSE_COEFF    (1023./5./14.5)    // divide ACD reading by this to get amps
//#define VOLT_IN_COEFF   38.0    // divide ADC reading by this to get voltage
#define VOLT_IN_COEFF 13.179  // divide ADC reading by this to get voltage
#define GTI_VSENSE_COEFF   255.0    // mutiply desired voltage by this to get PWM value
#define ADC_OVERSAMPLE  25      // average analog inputs over this many cycles

byte lastPwmVal = 0; // remember what we wrote last

void setup() {
  Serial.begin(57600);
  Serial.println(VERSIONSTRING);
  pinMode(GTI_VSENSE,OUTPUT);
  //pinMode(GTI_ISENSE,OUTPUT);
  setPwmFrequency(GTI_VSENSE,1); // set PWM to high-frequency
}

void loop() {
  float volt_input = avgAnalogRead(VOLT_INPUT) / VOLT_IN_COEFF; // get input voltage
  float amps_input = avgAnalogRead(ISENSE_INPUT) / ISENSE_COEFF; // get sensed amperage
  float knob_input = avgAnalogRead(KNOB_INPUT) / 1023; // get knob position 0.0-1.0
  if (volt_input < CUTOUT_VOLTAGE) {
    setOutputVoltage(knob_input);
  } else {
    setOutputVoltage(0); // turn off inverter above safe voltages
  }

  Serial.print("\tReal voltage: ");
  Serial.print(volt_input,1);
  Serial.print("\tknob position: ");
  Serial.print(knob_input,4);
  Serial.print("\tpwmVal: ");
  Serial.print(lastPwmVal);
  Serial.print("\tOutputVoltage: ");
  Serial.print(lastPwmVal / GTI_VSENSE_COEFF,3);
  Serial.println();
  delay(100);
}

void setOutputVoltage(float outputVoltage) {
  byte pwmVal = constrain((outputVoltage*GTI_VSENSE_COEFF),0,255);
  if (pwmVal != lastPwmVal) { // let's not analogWrite unnecessarily
    analogWrite(GTI_VSENSE,pwmVal);
    lastPwmVal = pwmVal;
  }
}

float avgAnalogRead(int pinNumber) {
  unsigned adder = 0; // store ADC samples here
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
