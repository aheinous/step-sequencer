#include <Arduino.h>


const uint8_t CLK_OUT = 1<<4;
volatile uint8_t & CLK_OUT_PORT = PORTD;

const uint8_t POT_nENABLE = 1<<5;
const uint8_t LED_nENABLE = 1<<4;
const uint8_t MUX_ADDR = 0xF;
volatile uint8_t & MUX_PORT = PORTB;


int ratePin = A3;


static inline void setBitsHigh(volatile uint8_t & port, uint8_t mask){
	port |= mask;
}

static inline void clearBits(volatile uint8_t & port, uint8_t mask){
	port &= ~mask;
}

static inline void setMaskedBitsTo(volatile uint8_t & port, uint8_t mask, uint8_t value){
		port = (port & ~mask) | (value & mask);
}


void setup(){
	Serial.begin(115200);
	Serial.println("Step Sequencer Start");


	DDRD |= CLK_OUT;
	DDRB |= (POT_nENABLE, LED_nENABLE, MUX_ADDR) ;
	// setBitsHigh(DDRD, CLK_OUT)

	initADC();
	startADCRead();
}






volatile bool adcValAvail;
volatile int adcVal; // never read without checking adcValAvail


ISR(ADC_vect){
	// Must read low first
	adcVal = ADCL | (ADCH << 8);

	adcValAvail = true;
}


void initADC(){
  	// Set MUX3..0 in ADMUX to read from AD3
	setMaskedBitsTo(ADMUX, 0xF, 3);

	// Set the Prescaler to 128 (16000KHz/128 = 125KHz) (Slowest possible)
	// Above 200KHz 10-bit results are not reliable.
	setMaskedBitsTo(ADCSRA, 0x7, 0x7);

	// ADEN: enable ADC
	// ADIE: enable interrupt
	setBitsHigh(ADCSRA, bit(ADEN) | bit(ADIE));

	interrupts();
}

void startADCRead(){
	adcValAvail = false;
	setBitsHigh(ADCSRA, bit(ADSC));
}


// Processor loop
void loop(){

  // Check to see if the value has been updated
  if (adcValAvail){
   
    // Perform whatever updating needed
    Serial.print("got.: ");
    Serial.println(adcVal);
    // adcValAvail = 0;
	startADCRead();
  }
 
  // Whatever else you would normally have running in loop().
 
}


