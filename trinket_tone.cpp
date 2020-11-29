#include <Arduino.h>

#include "keyerconfig.h"

#ifdef _INCLUDE_TRINKETTONE

#include "trinkettone.h"

typedef struct
{
    int pin;
    unsigned int freq;
} trinketTone_t;

trinketTone_t toneData;

// Tone function for Trinket
//  Same function parameters as normal tone library
//  Uses Timer 1
//  Supports one GPIO pin at a time

// Timer 1 interrupt handler
ISR(TIMER1_COMPA_vect)
{
    // Toggle tone pin
    bitWrite(PINB, toneData.pin, 1);
}

void trinketToneInit()
{
    toneData.freq = 0;
}

void trinketTone(int pin, unsigned int freq)
{
    unsigned long int ocr;
    unsigned char prescaler;

    // Set which pin to be toggled on interrupt
    // pins 0 - 4 on Trinket
    toneData.pin = pin;

    // Configure timer
    // Only bother if frequency is different from last time (or it's the first time)
    if(toneData.freq != freq)
    {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
      
        toneData.freq = freq;
        
        // scan through prescalars to find the best combination
        // of compare register and prescaler values
        ocr = F_CPU / freq / 2;
        prescaler = 1;
        while(ocr > 255)
        {
            prescaler++;
            ocr /= 2;
        }        

        bitWrite(TIMSK, OCIE1A, 0);

        // Load output compare register
        OCR1C = ocr - 1;

        // Load prescaler to start timer
        TCCR1 = 0x80 | prescaler;
    }
  
    // Enable interrupt
    bitWrite(TIMSK, OCIE1A, 1);
}

void trinketNoTone(int pin)
{
    // Stop tone by disabling interrupt
    bitWrite(TIMSK, OCIE1A, 0);
    digitalWrite(toneData.pin, LOW);
}

#endif

