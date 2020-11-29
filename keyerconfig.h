
#ifdef ARDUINO_AVR_TRINKET3

#define _INCLUDE_TRINKETTONE

#endif

// Board type is defined by IDE from boards.txt
// TRINKET3 means 3.3V or 5V Trinket board running at 8MHz
#ifdef ARDUINO_AVR_TRINKET3

// Trinket board gpio pins
#define     L_PIN      0        // Left paddle
#define     R_PIN      1        // Right paddle
#define     SPEED_PIN  1        // speed pot pin - '1' is gpio pin 2, see board documentation
#define     TONE_PIN   4        // Speaker
#define     TX_PIN     3        // Key transmitter

#else

// Nano/Uno gpio pins
#define     L_PIN      2        // Left paddle
#define     R_PIN      3        // Right paddle
#define     TONE_PIN   4        // Speaker
#define     TX_PIN     6        // Key transmitter
#define     LED_PIN    13       // on-board led
#define     SPEED_PIN  A0       // speed pot pin
#define     TONE_F_PIN A7       // sidetone frequency pin

#endif

// speed setting limits
#define     MIN_WPM    10       // minimum speed
#define     MAX_WPM    40       // maximum speed

#define     SIDETONE   700      // sideTONE pitch

#define     MIN_SIDETONE 500
#define     MAX_SIDETONE 2000

