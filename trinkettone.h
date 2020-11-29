#ifndef _GOT_TRINKETTONE_H

#define _GOT_TRINKETTONE_H

#ifdef _INCLUDE_TRINKETTONE

void trinketToneInit();
void trinketTone(int pin, unsigned int freq);
void trinketNoTone(int pin);

#define TONE   trinketTone
#define NOTONE trinketNoTone

#else

#define TONE   tone
#define NOTONE noTone

#endif

#endif
