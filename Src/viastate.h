#ifndef VS_HEADER
#define VS_HEADER

typedef struct {
  unsigned char ora,orb;
  unsigned char ira,irb;
  unsigned char ddra,ddrb;
  unsigned char acr,pcr;
  unsigned char ifr,ier;
  int timer1c,timer2c; /* NOTE: Timers descrement at 2MHz and values are */
  int timer1l,timer2l; /*   fixed up on read/write - latches hold 1MHz values*/
  int timer1hasshot; /* True if we have already caused an interrupt for one shot mode */
  int timer2hasshot; /* True if we have already caused an interrupt for one shot mode */
  int timer1adjust; // Adjustment for 1.5 cycle counts, every other interrupt, it becomes 2 cycles instead of one
  int timer2adjust; // Adjustment for 1.5 cycle counts, every other interrupt, it becomes 2 cycles instead of one
} VIAState;

#endif