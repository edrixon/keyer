#include <EEPROM.h>

#include "keyerconfig.h"

#include "trinkettone.h"

/*
 * Runs on Arduino Nano or Uno 
 *  - choose which paddle is for dots by holding it when resetting
 *  - toggle mode between A and B by squeezing paddles when resetting
 *  - configuration stored in EEPROM
 *  - on-board LED shows key state
 *  - pot used to set speed between 10 and 40wpm
 *  - generates 700Hz sideTONE
 * 
 * Differences for Trinket
 * - no serial port debug messages
 * - remove on-board LED, use gpio as input for right paddle instead
 * 

 */

// Check value for eeprom data
#define     EEPROM_VALID 0x5a5a

//  keyerControl bit definitions
#define     DIT_L      0x01     // Dit latch
#define     DAH_L      0x02     // Dah latch
#define     DIT_PROC   0x04     // Dit is being processed

// paddleState bits
#define     PADDLE_L   0x01
#define     PADDLE_R   0x02

// state machine states
enum keyerState_t { IDLE, CHK_DIT, CHK_DAH, KEYED_PREP, KEYED, INTER_ELEMENT };

// keyer modes
enum iambic_t { IAMBICA, IAMBICB };

// EEPROM data
typedef struct 
{
    unsigned long int valid;
    unsigned char dotPaddle;
    unsigned char dashPaddle;
    iambic_t mode;
} eepromData_t;

// Timing related stuff
typedef struct
{
    unsigned long int dit;
    unsigned long int dah;
    unsigned long int bip;
    unsigned long int kTimer;
    unsigned int wpm;
} timing_t;

unsigned char  keyerControl;
keyerState_t   keyerState;
eepromData_t   eepromData;
unsigned char  anyPaddle;
timing_t       timings;
unsigned int   sidetone;

void setup()
{

#ifndef ARDUINO_AVR_TRINKET3

    serialShowPins();

    // Configure LED gpio pin on Nano board
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW); // turn the LED off

#endif

    // Keying output pin
    pinMode(TX_PIN, OUTPUT);
    digitalWrite(TX_PIN, LOW);  // un-keyed

    // Paddle input pins
    pinMode(L_PIN, INPUT_PULLUP);
    pinMode(R_PIN, INPUT_PULLUP);

    // Set inital keying speed
    timings.wpm = readSpeed();
    updateTimings();

#ifndef ARDUINO_AVR_TRINKET3

    // Set initial sidetone frequency
    sidetone = readSidetoneFreq();

    serialShowSpeed();

#else

    sidetone = SIDETONE;

#endif

    getEepromData();

    setPreferences();
  
#ifdef _INCLUDE_TRINKETTONE

    trinketToneInit();

#ifndef ARDUINO_AVR_TRINKET3

    Serial.println("Using trinketTone functions");

#endif

#endif

#ifndef ARDUINO_AVR_TRINKET3

    serialShowPaddles();
    serialShowMode();

#endif

    // Initialise state machine
    keyerState = IDLE;

    anyPaddle = (DIT_L | DAH_L);

    keyerControl = 0;

    // Beep to show ready to go
    TONE(TONE_PIN, sidetone);
    delay(timings.bip);
    NOTONE(TONE_PIN);
    delay(timings.bip);

#ifndef ARDUINO_AVR_TRINKET3

    Serial.println("Ready...");

#endif

}
 
void loop()
{
  unsigned int newSidetone;
  unsigned int newSpeed;
  
  switch(keyerState)
  {
    case IDLE:
        // Wait for direct or latched paddle press
        // keyerControl might already have DIT_L ot DIT_R set
        updatePaddleLatch();
        if(keyerControl & anyPaddle)
        {
            keyerState = CHK_DIT;
        }
        break;
 
    case CHK_DIT:
        // See if the dit paddle was pressed
        if(keyerControl & DIT_L)
        {
            keyerControl |= DIT_PROC;
            timings.kTimer = timings.dit;
            keyerState = KEYED_PREP;
        }
        else
        {
            keyerState = CHK_DAH;
        }
        break;
 
    case CHK_DAH:
        // See if dah paddle was pressed
        if(keyerControl & DAH_L)
        {
            timings.kTimer = timings.dah;
            keyerState = KEYED_PREP;
        }
        else
        {
            keyerState = IDLE;
        }
        break;
 
    case KEYED_PREP:
        // Assert key down, start timing, state shared for dit or dah
        keyDown();
        timings.kTimer += millis();         // set ktimer to interval end time
        keyerControl &= ~(anyPaddle);       // clear both paddle latch bits
        keyerState = KEYED;                 // next state
        break;
 
    case KEYED:
        // Wait for timer to expire
        if(millis() > timings.kTimer)
        {            // are we at end of key down ?
            keyUp();
            timings.kTimer = millis() + timings.dit;    // inter-element time
            keyerState = INTER_ELEMENT;     // next state
        }
        else
        {
              if(eepromData.mode == IAMBICB)
              {
                  updatePaddleLatch();           // early paddle latch in Iambic B mode
              }
        }
        break;
 
    case INTER_ELEMENT:
        // Insert time between dits/dahs
        updatePaddleLatch();               // latch paddle state
        if(millis() > timings.kTimer)
        {            // are we at end of inter-space ?
            if(keyerControl & DIT_PROC)
            {             // was it a dit or dah ?
                keyerControl &= ~(DIT_L | DIT_PROC);   // clear two bits
                keyerState = CHK_DAH;                  // dit done, check for dah
            }
            else
            {
                keyerControl &= ~(DAH_L);              // clear dah latch
                keyerState = IDLE;                     // go idle
            }
        }
        break;
    }

    // see if speed is changed
    newSpeed = readSpeed();
    if(newSpeed != timings.wpm)
    {
        timings.wpm = newSpeed;
        updateTimings();
    }

#ifndef ARDUINO_AVR_TRINKET3

    newSidetone = readSidetoneFreq();
    if(newSidetone != sidetone)
    {
        sidetone = newSidetone;
    }

#endif
}

void keyUp()
{
#ifndef ARDUINO_AVR_TRINKET3
    digitalWrite(LED_PIN, LOW);      // turn the LED off
#endif
    digitalWrite(TX_PIN, LOW);       // un-key transmitter
    NOTONE(TONE_PIN);                // stop sideTONE
}

void keyDown()
{
#ifndef ARDUINO_AVR_TRINKET3 
    digitalWrite(LED_PIN, HIGH);     // turn the LED on
#endif
    digitalWrite(TX_PIN, HIGH);      // key transmitter
    TONE(TONE_PIN, sidetone);        // start sideTONE
}

void updatePaddleLatch()
{
    if(digitalRead(eepromData.dotPaddle) == LOW)
    {
        keyerControl |= DIT_L;
    }
    
    if(digitalRead(eepromData.dashPaddle) == LOW)
    {
        keyerControl |= DAH_L;
    }
}
  
unsigned int readSpeed()
{
    return map(analogRead(SPEED_PIN), 10, 1000, MIN_WPM, MAX_WPM);
}

#ifndef ARDUINO_AVR_TRINKET3

unsigned int readSidetoneFreq()
{
    return map(analogRead(TONE_F_PIN), 10, 1000, MIN_SIDETONE, MAX_SIDETONE);
}

#endif

void updateTimings()
{
    timings.dit = 1200 / timings.wpm;
    timings.dah = timings.dit * 3;
    timings.bip = timings.dit / 4;
}

void setPreferences()
{
    unsigned char paddleState;
    
    // Check paddles to see if configuration is being changed
    paddleState = 0;
    if(digitalRead(L_PIN) == LOW)
    {
        paddleState |= PADDLE_L;
    }

    if(digitalRead(R_PIN) == LOW)
    {
        paddleState |= PADDLE_R;
    }

    if(paddleState & PADDLE_L)
    {
        if(paddleState & PADDLE_R)
        {
            // Paddles being squeezed, so toggle mode and don't change the paddle settings
            if(eepromData.mode == IAMBICB)
            {
                eepromData.mode = IAMBICA;
            }
            else
            {
                eepromData.mode = IAMBICB;
            }

            // Update eeprom
            writeEeprom((unsigned char *)&eepromData);

            // Send an 'A' or a 'B' in morse to indicate new mode
            showMode();
        }
        else
        {
            // Left paddle only, make that the dot paddle and don't change mode
            eepromPaddles(L_PIN, R_PIN);            
        }
    }
    else
    {
        if(paddleState & PADDLE_R)
        {
            // Right paddle only, make that the dot paddle and don't change mode
            eepromPaddles(R_PIN, L_PIN);         
        }
    }
} 

void getEepromData()
{
    // Get configuration from EEPROM
    readEeprom((unsigned char *)&eepromData);
    if(eepromData.valid != EEPROM_VALID)
    {
#ifndef ARDUINO_AVR_TRINKET3
        Serial.println("Loading eeprom defaults");
#endif
        // Load with defaults if eeprom is empty
        eepromData.mode = IAMBICB;
        eepromPaddles(L_PIN, R_PIN);
    }
}

void eepromPaddles(unsigned char dot, unsigned char dash)
{
#ifndef ARDUINO_AVR_TRINKET3
    Serial.println("  Paddles changed");
#endif
    eepromData.dotPaddle = dot;
    eepromData.dashPaddle = dash;
    writeEeprom((unsigned char *)&eepromData);
}

void readEeprom(unsigned char *dPtr)
{
    int addr;

    addr = 0;
    while(addr < sizeof(eepromData))
    {
        *dPtr = EEPROM.read(addr);

        dPtr++;
        addr++;
    }
}

void writeEeprom(unsigned char *dPtr)
{
    int addr;

    eepromData.valid = EEPROM_VALID;

    addr = 0;
    while(addr < sizeof(eepromData))
    {
        EEPROM.write(addr, *dPtr);

        dPtr++;
        addr++;
    }
}

#ifndef ARDUINO_AVR_TRINKET3

void serialShowPins()
{
    Serial.begin(9600);
    Serial.println("GD6XHG Arduino keyer");
    Serial.println("");
    Serial.println("  Pin allocation");
    Serial.print("   Tone : ");
    Serial.println(TONE_PIN);
    Serial.print("   Left : ");
    Serial.println(L_PIN);
    Serial.print("   Right: ");
    Serial.println(R_PIN);
    Serial.print("   Aux  : ");
    Serial.println(TX_PIN);
    Serial.println("");
}

void serialShowSpeed()
{
    Serial.print("  Keyer speed: ");
    Serial.println(timings.wpm);
    Serial.println("");
}

void serialShowMode()
{
    Serial.print("  Iambic mode: ");
    if(eepromData.mode == IAMBICB)
    {
        Serial.println("B");      
    }
    else
    {
        Serial.println("A");
    }
}

void serialShowPaddles(void)
{
    Serial.println("  Paddles");
    Serial.print("    Dot  : ");
    serialPaddle(eepromData.dotPaddle);
    Serial.print("    Dash : ");
    serialPaddle(eepromData.dashPaddle);
    Serial.println();
}

void serialPaddle(unsigned char paddle)
{

    if(paddle == L_PIN)
    {
        Serial.println("Left");
    }
    else
    {
        if(paddle == R_PIN)
        {
            Serial.println("Right");
        }
        else
        {
            Serial.println(paddle);
        }
    }
}

#endif

void sendDash()
{
    TONE(TONE_PIN, sidetone);
    sendWait(timings.dah);  
    NOTONE(TONE_PIN);
}

void sendDot()
{
    TONE(TONE_PIN, sidetone);
    sendWait(timings.dit);  
    NOTONE(TONE_PIN);
}

void sendWait(unsigned long int w)
{
    timings.kTimer = millis() + w;
    while(millis() < timings.kTimer);
}

void sendChar(unsigned char chMorse)
{
    int c;  

    // Find start of character
    c = 8;
    while((chMorse & 0x80) == 0 && c)
    {
        chMorse = chMorse << 1;    
        c--;
    }
    chMorse = chMorse << 1;
    c--;

    // Shift out dots and dashes until end of character
    while(c)
    {
        if(chMorse & 0x80)
        {
            sendDash();
        }
        else
        {
            sendDot();
        }
        sendWait(timings.dit);

        chMorse = chMorse << 1;
        c--;
    }
}

void showMode()
{
    unsigned char aMorse;
    unsigned char bMorse;

    aMorse = 0x05;
    bMorse = 0x18;

    if(eepromData.mode == IAMBICA)
    {
        sendChar(aMorse);  // Send 'A'
    }
    else
    {
        sendChar(bMorse);  // Send 'B'
    }

    delay(1000);
}



