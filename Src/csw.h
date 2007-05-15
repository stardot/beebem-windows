/*
 *  csw.h
 *  BeebEm3
 *
 *  Created by Jon Welch on 27/08/2006.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

#define BUFFER_LEN	256

void LoadCSW(char *file);
void CloseCSW(void);
int csw_data(void);
int csw_poll(int clock);
void map_csw_file(void);

extern FILE *csw_file;
extern unsigned char file_buf[BUFFER_LEN];
extern unsigned char *csw_buff;
extern unsigned char *sourcebuff;

extern int csw_tonecount;
extern int csw_state;
extern int csw_datastate;
extern int csw_bit;
extern int csw_pulselen;
extern int csw_ptr;
extern unsigned long csw_bufflen;
extern int csw_byte;
extern int csw_pulsecount;
extern int bit_count;
extern unsigned char CSWOpen;
extern int CSW_BUF;
extern int CSW_CYCLES;

