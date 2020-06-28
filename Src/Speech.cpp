/**********************************************************************************************

TMS5220 interface

Based on code written by Frank Palazzolo and others

***********************************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "6502core.h"
#include "beebsound.h"
#include "beebmem.h"
#include "beebwin.h"
#include "sysvia.h"
#include "via.h"
#include "main.h"
#include "debug.h"

#include "Speech.h"

/* TMS5220 ROM Tables */

/* This is the energy lookup table (4-bits -> 10-bits) */

const static unsigned short energytable[0x10]={
	0x0000,0x00C0,0x0140,0x01C0,0x0280,0x0380,0x0500,0x0740,
	0x0A00,0x0E40,0x1440,0x1C80,0x2840,0x38C0,0x5040,0x7FC0};

/* This is the pitch lookup table (6-bits -> 8-bits) */

const static unsigned short pitchtable [0x40]={
	0x0000,0x1000,0x1100,0x1200,0x1300,0x1400,0x1500,0x1600,
	0x1700,0x1800,0x1900,0x1A00,0x1B00,0x1C00,0x1D00,0x1E00,
	0x1F00,0x2000,0x2100,0x2200,0x2300,0x2400,0x2500,0x2600,
	0x2700,0x2800,0x2900,0x2A00,0x2B00,0x2D00,0x2F00,0x3100,
	0x3300,0x3500,0x3600,0x3900,0x3B00,0x3D00,0x3F00,0x4200,
	0x4500,0x4700,0x4900,0x4D00,0x4F00,0x5100,0x5500,0x5700,
	0x5C00,0x5F00,0x6300,0x6600,0x6A00,0x6E00,0x7300,0x7700,
	0x7B00,0x8000,0x8500,0x8A00,0x8F00,0x9500,0x9A00,0xA000};

/* These are the reflection coefficient lookup tables */

/* K1 is (5-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k1table  [0x20]={
	(short)0x82C0,(short)0x8380,(short)0x83C0,(short)0x8440,(short)0x84C0,(short)0x8540,(short)0x8600,(short)0x8780,
	(short)0x8880,(short)0x8980,(short)0x8AC0,(short)0x8C00,(short)0x8D40,(short)0x8F00,(short)0x90C0,(short)0x92C0,
	(short)0x9900,(short)0xA140,(short)0xAB80,(short)0xB840,(short)0xC740,(short)0xD8C0,(short)0xEBC0,0x0000,
	0x1440,0x2740,0x38C0,0x47C0,0x5480,0x5EC0,0x6700,0x6D40};

/* K2 is (5-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k2table    [0x20]={
	(short)0xAE00,(short)0xB480,(short)0xBB80,(short)0xC340,(short)0xCB80,(short)0xD440,(short)0xDDC0,(short)0xE780,
	(short)0xF180,(short)0xFBC0,0x0600,0x1040,0x1A40,0x2400,0x2D40,0x3600,
	0x3E40,0x45C0,0x4CC0,0x5300,0x5880,0x5DC0,0x6240,0x6640,
	0x69C0,0x6CC0,0x6F80,0x71C0,0x73C0,0x7580,0x7700,0x7E80};

/* K3 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k3table    [0x10]={
	(short)0x9200,(short)0x9F00,(short)0xAD00,(short)0xBA00,(short)0xC800,(short)0xD500,(short)0xE300,(short)0xF000,
	(short)0xFE00,0x0B00,0x1900,0x2600,0x3400,0x4100,0x4F00,0x5C00};

/* K4 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k4table    [0x10]={
	(short)0xAE00,(short)0xBC00,(short)0xCA00,(short)0xD800,(short)0xE600,(short)0xF400,0x0100,0x0F00,
	0x1D00,0x2B00,0x3900,0x4700,0x5500,0x6300,0x7100,0x7E00};

/* K5 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k5table    [0x10]={
	(short)0xAE00,(short)0xBA00,(short)0xC500,(short)0xD100,(short)0xDD00,(short)0xE800,(short)0xF400,(short)0xFF00,
	0x0B00,0x1700,0x2200,0x2E00,0x3900,0x4500,0x5100,0x5C00};

/* K6 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k6table    [0x10]={
	(short)0xC000,(short)0xCB00,(short)0xD600,(short)0xE100,(short)0xEC00,(short)0xF700,0x0300,0x0E00,
	0x1900,0x2400,0x2F00,0x3A00,0x4500,0x5000,0x5B00,0x6600};

/* K7 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k7table    [0x10]={
	(short)0xB300,(short)0xBF00,(short)0xCB00,(short)0xD700,(short)0xE300,(short)0xEF00,(short)0xFB00,0x0700,
	0x1300,0x1F00,0x2B00,0x3700,0x4300,0x4F00,0x5A00,0x6600};

/* K8 is (3-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k8table    [0x08]={
	(short)0xC000,(short)0xD800,(short)0xF000,0x0700,0x1F00,0x3700,0x4F00,0x6600};

/* K9 is (3-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k9table    [0x08]={
	(short)0xC000,(short)0xD400,(short)0xE800,(short)0xFC00,0x1000,0x2500,0x3900,0x4D00};

/* K10 is (3-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k10table   [0x08]={
	(short)0xCD00,(short)0xDF00,(short)0xF100,0x0400,0x1600,0x2000,0x3B00,0x4D00};

/* chirp table */

static char chirptable[41]={
	0x00, 0x2a, (char)0xd4, 0x32,
	(char)0xb2, 0x12, 0x25, 0x14,
	0x02, (char)0xe1, (char)0xc5, 0x02,
	0x5f, 0x5a, 0x05, 0x0f,
	0x26, (char)0xfc, (char)0xa5, (char)0xa5,
	(char)0xd6, (char)0xdd, (char)0xdc, (char)0xfc,
	0x25, 0x2b, 0x22, 0x21,
	0x0f, (char)0xff, (char)0xf8, (char)0xee,
	(char)0xed, (char)0xef, (char)0xf7, (char)0xf6,
	(char)0xfa, 0x00, 0x03, 0x02,
	0x01
};

/* interpolation coefficients */

static char interp_coeff[8] = {
	8, 8, 8, 4, 4, 2, 2, 1
};

#define DEBUG_5220	0

/* Static function prototypes */
static void process_command(struct tms5220 *tms);
static int extract_bits(struct tms5220 *tms, int count);
static int parse_frame(struct tms5220 *tms, int the_first_frame);
static void check_buffer_low(struct tms5220 *tms);
static void set_interrupt_state(struct tms5220 *tms, int state);
//static void tms5220_update(void *param, stream_sample_t **inputs, stream_sample_t **buffer, int length);
static void SpeechError(const char *fmt, ...);

bool SpeechDefault;
bool SpeechEnabled;
struct tms5220_info *tms5220;
unsigned char phrom_rom[16][16384];

/*----------------------------------------------------------------------------*/
void BeebReadPhroms(void) {
	FILE *InFile,*RomCfg;
	char TmpPath[256];
	char fullname[256];
	int romslot = 0xf;
	char RomNameBuf[80];
	char *RomName=RomNameBuf;
	unsigned char sc;
	
	/* Read all ROM files in the BeebFile directory */
	// This section rewritten for V.1.32 to take account of roms.cfg file.
	strcpy(TmpPath,mainWin->GetUserDataPath());
	strcat(TmpPath,"phroms.cfg");
	
	fprintf(stderr, "Opening %s\n", TmpPath);
	
	RomCfg=fopen(TmpPath,"rt");
	
	if (RomCfg==NULL) {
		SpeechError("Cannot open PHROM Configuration file phroms.cfg\n");
		return;
	}

	// read phroms
	for (romslot=15;romslot>=0;romslot--) {
		fgets(RomName,80,RomCfg);
		if (strchr(RomName, 13)) *strchr(RomName, 13) = 0;
		if (strchr(RomName, 10)) *strchr(RomName, 10) = 0;
		strcpy(fullname,RomName);
		if ((RomName[0]!='\\') && (RomName[1]!=':')) {
			strcpy(fullname,mainWin->GetUserDataPath());
			strcat(fullname,"Phroms/");
			strcat(fullname,RomName);
		}
			
//		fprintf(stderr, "PHROM Name = '%s', '%s'\n", RomName, fullname);

		if (strncmp(RomName,"EMPTY",5)!=0)
		{
			for (sc = 0; fullname[sc]; sc++) 
				if (fullname[sc] == '\\') 
					fullname[sc] = '/';
			InFile = fopen(fullname,"rb");
			if (InFile!=NULL)
			{
				fread(phrom_rom[romslot], 1, 16384, InFile);
				fclose(InFile);
			}
			else 
			{
				SpeechError("Cannot open specified PHROM: %s\n",fullname);
			}
		}
	 }
	fclose(RomCfg);
}

void my_irq(int state)
{
}

int my_read(int count)
{
int val;
int addr;
int phrom;
	
	phrom = (tms5220->chip->phrom_address >> 14) & 0xf; 

//	fprintf(stderr, "In my_read with addr = 0x%04x, phrom = 0x%02x, count = %d", addr, phrom, count);

	val = 0;
	while (count--)
	{
		addr = tms5220->chip->phrom_address & 0x3fff;
		val = (val << 1) | ((phrom_rom[phrom][addr] >> tms5220->chip->phrom_bits_taken) & 1);
		tms5220->chip->phrom_bits_taken++;
		if (tms5220->chip->phrom_bits_taken >= 8)
		{
			tms5220->chip->phrom_address++;
			tms5220->chip->phrom_bits_taken = 0;
		}
	}

//	fprintf(stderr, ", returning 0x%02x\n", val);
	return val;
}

void my_load_address(int data)
{
//	fprintf(stderr, "In my_load_address with data = %d\n", data);
	tms5220->chip->phrom_address >>= 4;
	tms5220->chip->phrom_address &= 0x0ffff;
	tms5220->chip->phrom_address |= (data << 16);
//	fprintf(stderr, "In my_load_address with data = 0x%02x, new address = 0x%05x\n", data, tms5220->chip->phrom_address);
}

void my_read_and_branch(void)
{
//	fprintf(stderr, "In my_read_and_branch\n");
}


/**********************************************************************************************

tms5220_start -- allocate buffers and reset the 5220

***********************************************************************************************/

void tms5220_start(void)
{
	int clock = 640000;
	
	SpeechEnabled = false;
	tms5220 = NULL;
	
	BeebReadPhroms();

	tms5220 = (tms5220_info *) malloc(sizeof(*tms5220));
	memset(tms5220, 0, sizeof(*tms5220));
	tms5220->clock = clock;
	
	tms5220->chip = tms5220_create();
	if (!tms5220->chip)
	{
		free(tms5220);
		tms5220 = NULL;
		return;
	}

    /* reset the 5220 */
    tms5220_reset_chip(tms5220->chip);

    /* set the initial frequency */
    tms5220_set_frequency(clock);
    tms5220->source_pos = 0;
    tms5220->last_sample = tms5220->curr_sample = 0;

	SpeechEnabled = true;
}

/**********************************************************************************************

tms5220_stop -- free buffers

***********************************************************************************************/

void tms5220_stop(void)
{
	if (tms5220)
	{
		tms5220_destroy(tms5220->chip);
		free(tms5220);
	}

	SpeechEnabled = false;
	tms5220 = NULL;
}

/**********************************************************************************************

tms5220_data_w -- write data to the sound chip

***********************************************************************************************/

void tms5220_data_w( int data )
{
	if (SpeechEnabled && tms5220 != NULL)
		tms5220_data_write(tms5220->chip, data);
}

/**********************************************************************************************

tms5220_status_r -- read status or data from the sound chip

***********************************************************************************************/

int tms5220_status_r( void )
{
	if (SpeechEnabled && tms5220 != NULL)
	    return tms5220_status_read(tms5220->chip);
	else
		return 0;
}

/**********************************************************************************************

tms5220_ready_r -- return the not ready status from the sound chip

***********************************************************************************************/

int tms5220_ready_r(void)
{
	if (SpeechEnabled && tms5220 != NULL)
	    return tms5220_ready_read(tms5220->chip);
	else
		return 0;
}

/**********************************************************************************************

tms5220_int_r -- return the int status from the sound chip

***********************************************************************************************/

int tms5220_int_r(void)
{
	if (SpeechEnabled && tms5220 != NULL)
	    return tms5220_int_read(tms5220->chip);
	else
		return 0;
}

/**********************************************************************************************

tms5220_update -- update the sound chip so that it is in sync with CPU execution

***********************************************************************************************/

void tms5220_update(unsigned char *buff, int length)
{
	struct tms5220_info *info = tms5220;
	INT16 sample_data[MAX_SAMPLE_CHUNK], *curr_data = sample_data;
	INT16 prev = info->last_sample, curr = info->curr_sample;
	unsigned char *buffer = buff;
	UINT32 final_pos;
	UINT32 new_samples;
	INT32 samp;
	int len = length;
	
	/* finish off the current sample */
	if (info->source_pos > 0)
	{

		/* interpolate */
		while (length > 0 && info->source_pos < FRAC_ONE)
		{
			samp = (((INT32)prev * (INT32)(FRAC_ONE - info->source_pos)) + ((INT32)curr * (INT32)info->source_pos)) >> FRAC_BITS;
//			samp = ((samp + 32768) >> 8) & 255;
//			fprintf(stderr, "Sample = %d\n", samp);
			*buffer++ = samp;
			info->source_pos += info->source_step;
			length--;
		}
		
		/* if we're over, continue; otherwise, we're done */
		if (info->source_pos >= FRAC_ONE)
			info->source_pos -= FRAC_ONE;
		else
		{
			if (buffer - buff != len)
				fprintf(stderr, "Here for some reason - mismatch in num of samples = %d, %d\n", len, buffer - buff);
			tms5220_process(info->chip, sample_data, 0);
			return;
		}
	}
	
	/* compute how many new samples we need */
	final_pos = info->source_pos + length * info->source_step;
	new_samples = (final_pos + FRAC_ONE - 1) >> FRAC_BITS;
	if (new_samples > MAX_SAMPLE_CHUNK)
		new_samples = MAX_SAMPLE_CHUNK;
	
	/* generate them into our buffer */
	tms5220_process(info->chip, sample_data, new_samples);
	prev = curr;
	curr = *curr_data++;
	
	/* then sample-rate convert with linear interpolation */
	while (length > 0)
	{
		/* interpolate */
		while (length > 0 && info->source_pos < FRAC_ONE)
		{
			samp = (((INT32)prev * (INT32)(FRAC_ONE - info->source_pos)) + ((INT32)curr * (INT32)info->source_pos)) >> FRAC_BITS;
//			samp = ((samp + 32768) >> 8) & 255;
//			fprintf(stderr, "Sample = %d\n", samp);
			*buffer++ = samp;
			info->source_pos += info->source_step;
			length--;
		}
		
		/* if we're over, grab the next samples */
		if (info->source_pos >= FRAC_ONE)
		{
			info->source_pos -= FRAC_ONE;
			prev = curr;
			curr = *curr_data++;
			if ((curr_data - sample_data) > MAX_SAMPLE_CHUNK)
				curr_data = sample_data;
		}
	}
	
	/* remember the last samples */
	info->last_sample = prev;
	info->curr_sample = curr;

	if (buffer - buff != len)
		fprintf(stderr, "At end of update - mismatch in num of samples = %d, %d\n", len, buffer - buff);
}

/**********************************************************************************************

tms5220_set_frequency -- adjusts the playback frequency

***********************************************************************************************/

void tms5220_set_frequency(int frequency)
{
//	tms5220->source_step = (UINT32)((double)(frequency / 80) * (double)FRAC_ONE / (double) 44100.0);
	tms5220->source_step = (UINT32)((double)(frequency / 80) * (double)FRAC_ONE / (double) SoundSampleRate);

//	fprintf(stderr, "Source Step = %d, FRAC_ONE = %d, FRAC_BITS = %d\n", tms5220->source_step, FRAC_ONE, FRAC_BITS);
}

struct tms5220 *tms5220_create(void)
{
	struct tms5220 *tms;
	
	tms = (struct tms5220 *) malloc(sizeof(*tms));
	memset(tms, 0, sizeof(*tms));
	return tms;
}


void tms5220_destroy(struct tms5220 *chip)
{
	free(chip);
}

/**********************************************************************************************

tms5220_reset -- resets the TMS5220

***********************************************************************************************/

void tms5220_reset_chip(struct tms5220 *chip)
{
	struct tms5220 *tms = chip;
	
	/* initialize the FIFO */
	/*memset(tms->fifo, 0, sizeof(tms->fifo));*/
	tms->fifo_head = tms->fifo_tail = tms->fifo_count = tms->fifo_bits_taken = tms->phrom_bits_taken = 0;
	
	/* initialize the chip state */
	/* Note that we do not actually clear IRQ on start-up : IRQ is even raised if tms->buffer_empty or tms->buffer_low are 0 */
	tms->tms5220_speaking = tms->speak_external = tms->talk_status = tms->first_frame = tms->last_frame = tms->irq_pin = 0;
	my_irq(0);
	tms->buffer_empty = tms->buffer_low = 1;
	
	tms->RDB_flag = FALSE;
	
	/* initialize the energy/pitch/k states */
	tms->old_energy = tms->new_energy = tms->current_energy = tms->target_energy = 0;
	tms->old_pitch = tms->new_pitch = tms->current_pitch = tms->target_pitch = 0;
	memset(tms->old_k, 0, sizeof(tms->old_k));
	memset(tms->new_k, 0, sizeof(tms->new_k));
	memset(tms->current_k, 0, sizeof(tms->current_k));
	memset(tms->target_k, 0, sizeof(tms->target_k));
	
	/* initialize the sample generators */
	tms->interp_count = tms->sample_count = tms->pitch_count = 0;
	tms->randbit = 0;
	memset(tms->u, 0, sizeof(tms->u));
	memset(tms->x, 0, sizeof(tms->x));
	
	tms->phrom_address = 0;
	
}

void logerror(char *text, ...)
{
va_list argptr;
	
	va_start(argptr, text);
	vfprintf(stderr, text, argptr);
	va_end(argptr);
}

/**********************************************************************************************

tms5220_data_write -- handle a write to the TMS5220

***********************************************************************************************/

void tms5220_data_write(struct tms5220 *chip, int data)
{
	struct tms5220 *tms = chip;
	
    /* add this byte to the FIFO */
    if (tms->fifo_count < FIFO_SIZE)
    {
        tms->fifo[tms->fifo_tail] = data;
        tms->fifo_tail = (tms->fifo_tail + 1) % FIFO_SIZE;
        tms->fifo_count++;
		
		/* if we were speaking, then we're no longer empty */
		if (tms->speak_external)
			tms->buffer_empty = 0;
		
        if (DEBUG_5220) logerror("Added byte to FIFO (size=%2d)\n", tms->fifo_count);
    }
    else
    {
        if (DEBUG_5220) logerror("Ran out of room in the FIFO! - PC = 0x%04x\n", ProgramCounter);
    }
	
    /* update the buffer low state */
    check_buffer_low(tms);
	
	if (! tms->speak_external)
		/* R Nabet : we parse commands at once.  It is necessary for such commands as read. */
		process_command (tms/*data*/);
}


/**********************************************************************************************

tms5220_status_read -- read status or data from the TMS5220

From the data sheet:
bit 0 = TS - Talk Status is active (high) when the VSP is processing speech data.
Talk Status goes active at the initiation of a Speak command or after nine
bytes of data are loaded into the FIFO following a Speak External command. It
goes inactive (low) when the stop code (Energy=1111) is processed, or
immediately by a buffer empty condition or a reset command.
bit 1 = BL - Buffer Low is active (high) when the FIFO buffer is more than half empty.
Buffer Low is set when the "Last-In" byte is shifted down past the half-full
boundary of the stack. Buffer Low is cleared when data is loaded to the stack
so that the "Last-In" byte lies above the half-full boundary and becomes the
ninth data byte of the stack.
bit 2 = BE - Buffer Empty is active (high) when the FIFO buffer has run out of data
while executing a Speak External command. Buffer Empty is set when the last bit
of the "Last-In" byte is shifted out to the Synthesis Section. This causes
Talk Status to be cleared. Speed is terminated at some abnormal point and the
Speak External command execution is terminated.

***********************************************************************************************/

int tms5220_status_read(struct tms5220 *chip)
{
	struct tms5220 *tms = chip;
	
	if (tms->RDB_flag)
	{	/* if last command was read, return data register */
		tms->RDB_flag = FALSE;
		return(tms->data_register);
	}
else
{	/* read status */

/* clear the interrupt pin */
set_interrupt_state(tms, 0);

 if (DEBUG_5220) logerror("Status read: TS=%d BL=%d BE=%d\n", tms->talk_status, tms->buffer_low, tms->buffer_empty);

return (tms->talk_status << 7) | (tms->buffer_low << 6) | (tms->buffer_empty << 5);
}
}

/**********************************************************************************************

tms5220_ready_read -- returns the ready state of the TMS5220

***********************************************************************************************/

int tms5220_ready_read(struct tms5220 *chip)
{
	struct tms5220 *tms = chip;
    return (tms->fifo_count < FIFO_SIZE-1);
}

/**********************************************************************************************

tms5220_int_read -- returns the interrupt state of the TMS5220

***********************************************************************************************/

int tms5220_int_read(struct tms5220 *chip)
{
	struct tms5220 *tms = chip;
    return tms->irq_pin;
}

/**********************************************************************************************

tms5220_process -- fill the buffer with a specific number of samples

***********************************************************************************************/

void tms5220_process(struct tms5220 *chip, INT16 *buffer, unsigned int size)
{
	struct tms5220 *tms = chip;
    int buf_count=0;
    int i, interp_period;
	
tryagain:
		
		/* if we're not speaking, parse commands */
		/*while (!tms->speak_external && tms->fifo_count > 0)
        process_command(tms);*/
		
		/* if we're empty and still not speaking, fill with nothingness */
		if ((!tms->tms5220_speaking) && (!tms->last_frame))
			goto empty;
	
    /* if we're to speak, but haven't started, wait for the 9th byte */
	if (!tms->talk_status && tms->speak_external)
    {
        if (tms->fifo_count < 9)
			goto empty;
		
        tms->talk_status = 1;
		tms->first_frame = 1;	/* will cause the first frame to be parsed */
		tms->buffer_empty = 0;
	}
	
#if 0
	/* we are to speak, yet we fill with 0s until start of next frame */
	if (tms->first_frame)
	{
		while ((size > 0) && ((tms->sample_count != 0) || (tms->interp_count != 0)))
		{
			tms->sample_count = (tms->sample_count + 1) % 200;
			tms->interp_count = (tms->interp_count + 1) % 25;
			buffer[buf_count] = 0x80;	/* should be (-1 << 8) ??? (cf note in data sheet, p 10, table 4) */
			buf_count++;
			size--;
		}
	}
#endif
	
    /* loop until the buffer is full or we've stopped speaking */
	while ((size > 0) && tms->talk_status)
    {
        int current_val;
		
        /* if we're ready for a new frame */
        if ((tms->interp_count == 0) && (tms->sample_count == 0))
        {
            /* Parse a new frame */
			if (!parse_frame(tms, tms->first_frame))
				break;
			tms->first_frame = 0;
			
            /* Set old target as new start of frame */
            tms->current_energy = tms->old_energy;
            tms->current_pitch = tms->old_pitch;
            for (i = 0; i < 10; i++)
                tms->current_k[i] = tms->old_k[i];
			
            /* is this a zero energy frame? */
            if (tms->current_energy == 0)
            {
                /*printf("processing frame: zero energy\n");*/
                tms->target_energy = 0;
                tms->target_pitch = tms->current_pitch;
                for (i = 0; i < 10; i++)
                    tms->target_k[i] = tms->current_k[i];
            }
			
            /* is this a stop frame? */
            else if (tms->current_energy == (energytable[15] >> 6))
            {
                /*printf("processing frame: stop frame\n");*/
                tms->current_energy = energytable[0] >> 6;
                tms->target_energy = tms->current_energy;
				/*tms->interp_count = tms->sample_count =*/ tms->pitch_count = 0;
				tms->last_frame = 0;
				if (tms->tms5220_speaking)
					/* new speech command in progress */
					tms->first_frame = 1;
				else
				{
					/* really stop speaking */
					tms->talk_status = 0;
					
					/* generate an interrupt if necessary */
					set_interrupt_state(tms, 1);
				}
				
                /* try to fetch commands again */
                goto tryagain;
            }
            else
            {
                /* is this the ramp down frame? */
                if (tms->new_energy == (energytable[15] >> 6))
                {
                    /*printf("processing frame: ramp down\n");*/
                    tms->target_energy = 0;
                    tms->target_pitch = tms->current_pitch;
                    for (i = 0; i < 10; i++)
                        tms->target_k[i] = tms->current_k[i];
                }
                /* Reset the step size */
                else
                {
                    /*printf("processing frame: Normal\n");*/
                    /*printf("*** Energy = %d\n",tms->current_energy);*/
                    /*printf("proc: %d %d\n",last_fbuf_head,fbuf_head);*/
					
                    tms->target_energy = tms->new_energy;
                    tms->target_pitch = tms->new_pitch;
					
                    for (i = 0; i < 4; i++)
                        tms->target_k[i] = tms->new_k[i];
                    if (tms->current_pitch == 0)
                        for (i = 4; i < 10; i++)
                        {
                            tms->target_k[i] = tms->current_k[i] = 0;
                        }
							else
								for (i = 4; i < 10; i++)
									tms->target_k[i] = tms->new_k[i];
                }
            }
        }
        else if (tms->interp_count == 0)
        {
            /* Update values based on step values */
            /*printf("\n");*/
			
			interp_period = tms->sample_count / 25;
            tms->current_energy += (tms->target_energy - tms->current_energy) / interp_coeff[interp_period];
            if (tms->old_pitch != 0)
                tms->current_pitch += (tms->target_pitch - tms->current_pitch) / interp_coeff[interp_period];
			
            /*printf("*** Energy = %d\n",tms->current_energy);*/
			
            for (i = 0; i < 10; i++)
            {
                tms->current_k[i] += (tms->target_k[i] - tms->current_k[i]) / interp_coeff[interp_period];
            }
        }
		
		current_val = 0x00;

        if (tms->old_energy == 0)
        {
            /* generate silent samples here */
            current_val = 0x00;
        }
        else if (tms->old_pitch == 0)
        {
            /* generate unvoiced samples here */
            tms->randbit = (rand() % 2) * 2 - 1;
            current_val = (tms->randbit * tms->current_energy) / 4;
        }
        else
        {
            /* generate voiced samples here */
            if (tms->pitch_count < sizeof(chirptable))
                current_val = (chirptable[tms->pitch_count] * tms->current_energy) / 256;
            else
                current_val = 0x00;
        }
		
        /* Lattice filter here */
		
        tms->u[10] = current_val;
		
        for (i = 9; i >= 0; i--)
        {
            tms->u[i] = tms->u[i+1] - ((tms->current_k[i] * tms->x[i]) / 32768);
        }
        for (i = 9; i >= 1; i--)
        {
            tms->x[i] = tms->x[i-1] + ((tms->current_k[i-1] * tms->u[i-1]) / 32768);
        }
		
        tms->x[0] = tms->u[0];
		
        /* clipping, just like the chip */
		
        if (tms->u[0] > 511)
           buffer[buf_count] = 255;
         else if (tms->u[0] < -512)
            buffer[buf_count] = 0;
        else
            buffer[buf_count] = (tms->u[0] >> 2) + 128;
		
        /* Update all counts */
		
        size--;
        tms->sample_count = (tms->sample_count + 1) % 200;
		
        if (tms->current_pitch != 0)
            tms->pitch_count = (tms->pitch_count + 1) % tms->current_pitch;
        else
            tms->pitch_count = 0;
		
        tms->interp_count = (tms->interp_count + 1) % 25;
        buf_count++;
    }
	
empty:
		
		while (size > 0)
		{
			tms->sample_count = (tms->sample_count + 1) % 200;
			tms->interp_count = (tms->interp_count + 1) % 25;
			buffer[buf_count] = 0x80;	/* should be (-1 << 8) ??? (cf note in data sheet, p 10, table 4) */
			buf_count++;
			size--;
		}
}



/**********************************************************************************************

process_command -- extract a byte from the FIFO and interpret it as a command

***********************************************************************************************/

static void process_command(struct tms5220 *tms)
{
    unsigned char cmd;
	
    /* if there are stray bits, ignore them */
	if (tms->fifo_bits_taken)
	{
		tms->fifo_bits_taken = 0;
        tms->fifo_count--;
        tms->fifo_head = (tms->fifo_head + 1) % FIFO_SIZE;
    }
	
    /* grab a full byte from the FIFO */
    if (tms->fifo_count > 0)
    {
		cmd = tms->fifo[tms->fifo_head];
		tms->fifo_count--;
		tms->fifo_head = (tms->fifo_head + 1) % FIFO_SIZE;
		
		/* parse the command */
		switch (cmd & 0x70)
		{
			case 0x10 : /* read byte */
				tms->phrom_bits_taken = 0;
				tms->data_register = my_read(8);	/* read one byte from speech ROM... */
				tms->RDB_flag = TRUE;
				break;
				
			case 0x30 : /* read and branch */
				if (DEBUG_5220) logerror("read and branch command received\n");
				tms->RDB_flag = FALSE;
				my_read_and_branch();
				break;
					
			case 0x40 : /* load address */
				/* tms5220 data sheet says that if we load only one 4-bit nibble, it won't work.
				This code does not care about this. */
				my_load_address(cmd & 0x0f);
				break;
						
			case 0x50 : /* speak */
				tms->tms5220_speaking = 1;
				tms->speak_external = 0;
				if (! tms->last_frame)
				{
					tms->first_frame = 1;
				}
					tms->talk_status = 1;  /* start immediately */
				break;
							
			case 0x60 : /* speak external */
				tms->tms5220_speaking = tms->speak_external = 1;
							
				tms->RDB_flag = FALSE;
								
				/* according to the datasheet, this will cause an interrupt due to a BE condition */
				if (!tms->buffer_empty)
				{
					tms->buffer_empty = 1;
					set_interrupt_state(tms, 1);
				}
									
				tms->talk_status = 0;	/* wait to have 8 bytes in buffer before starting */
				break;
								
			case 0x70 : /* reset */
				tms5220_reset_chip(tms);
				break;
        }
    }
	
    /* update the buffer low state */
    check_buffer_low(tms);
}



/**********************************************************************************************

extract_bits -- extract a specific number of bits from the FIFO

***********************************************************************************************/

static int extract_bits(struct tms5220 *tms, int count)
{
    int val = 0;
	
	if (tms->speak_external)
	{
		/* extract from FIFO */
    	while (count--)
    	{
        	val = (val << 1) | ((tms->fifo[tms->fifo_head] >> tms->fifo_bits_taken) & 1);
        	tms->fifo_bits_taken++;
        	if (tms->fifo_bits_taken >= 8)
        	{
        	    tms->fifo_count--;
        	    tms->fifo_head = (tms->fifo_head + 1) % FIFO_SIZE;
        	    tms->fifo_bits_taken = 0;
        	}
    	}
    }
	else
	{
		/* extract from speech ROM */
		val = my_read(count);
	}
	
    return val;
}



/**********************************************************************************************

parse_frame -- parse a new frame's worth of data; returns 0 if not enough bits in buffer

***********************************************************************************************/

static int parse_frame(struct tms5220 *tms, int the_first_frame)
{
	int bits = 0;	/* number of bits in FIFO (speak external only) */
	int indx, i, rep_flag;
	
	if (! the_first_frame)
	{
		/* remember previous frame */
		tms->old_energy = tms->new_energy;
		tms->old_pitch = tms->new_pitch;
		for (i = 0; i < 10; i++)
			tms->old_k[i] = tms->new_k[i];
	}
	
    /* clear out the new frame */
    tms->new_energy = 0;
    tms->new_pitch = 0;
    for (i = 0; i < 10; i++)
        tms->new_k[i] = 0;
	
    /* if the previous frame was a stop frame, don't do anything */
	if ((! the_first_frame) && (tms->old_energy == (energytable[15] >> 6)))
		/*return 1;*/
	{
		tms->buffer_empty = 1;
		return 1;
	}
	
	if (tms->speak_external)
    	/* count the total number of bits available */
		bits = tms->fifo_count * 8 - tms->fifo_bits_taken;
	
    /* attempt to extract the energy index */
	if (tms->speak_external)
	{
		bits -= 4;
		if (bits < 0)
			goto ranout;
	}
    indx = extract_bits(tms, 4);
    tms->new_energy = energytable[indx] >> 6;
	
	/* if the index is 0 or 15, we're done */
	if (indx == 0 || indx == 15)
	{
		if (DEBUG_5220) logerror("  (4-bit energy=%d frame)\n",tms->new_energy);
		
		/* clear tms->fifo if stop frame encountered */
		if (indx == 15)
		{
			tms->fifo_head = tms->fifo_tail = tms->fifo_count = tms->fifo_bits_taken = tms->phrom_bits_taken = 0;
			tms->speak_external = tms->tms5220_speaking = 0;
			tms->last_frame = 1;
		}
		goto done;
	}
	
    /* attempt to extract the repeat flag */
	if (tms->speak_external)
	{
		bits -= 1;
		if (bits < 0)
			goto ranout;
	}
    rep_flag = extract_bits(tms, 1);
	
    /* attempt to extract the pitch */
	if (tms->speak_external)
	{
		bits -= 6;
		if (bits < 0)
			goto ranout;
	}
    indx = extract_bits(tms, 6);
    tms->new_pitch = pitchtable[indx] / 256;
	
    /* if this is a repeat frame, just copy the k's */
    if (rep_flag)
    {
        for (i = 0; i < 10; i++)
            tms->new_k[i] = tms->old_k[i];
		
        if (DEBUG_5220) logerror("  (11-bit energy=%d pitch=%d rep=%d frame)\n", tms->new_energy, tms->new_pitch, rep_flag);
        goto done;
    }
	
    /* if the pitch index was zero, we need 4 k's */
    if (indx == 0)
    {
        /* attempt to extract 4 K's */
		if (tms->speak_external)
		{
			bits -= 18;
			if (bits < 0)
				goto ranout;
		}
        tms->new_k[0] = k1table[extract_bits(tms, 5)];
        tms->new_k[1] = k2table[extract_bits(tms, 5)];
        tms->new_k[2] = k3table[extract_bits(tms, 4)];
		tms->new_k[3] = k4table[extract_bits(tms, 4)];
		
        if (DEBUG_5220) logerror("  (29-bit energy=%d pitch=%d rep=%d 4K frame)\n", tms->new_energy, tms->new_pitch, rep_flag);
        goto done;
    }
	
    /* else we need 10 K's */
	if (tms->speak_external)
	{
		bits -= 39;
		if (bits < 0)
			goto ranout;
	}
	
    tms->new_k[0] = k1table[extract_bits(tms, 5)];
    tms->new_k[1] = k2table[extract_bits(tms, 5)];
    tms->new_k[2] = k3table[extract_bits(tms, 4)];
	tms->new_k[3] = k4table[extract_bits(tms, 4)];
    tms->new_k[4] = k5table[extract_bits(tms, 4)];
    tms->new_k[5] = k6table[extract_bits(tms, 4)];
    tms->new_k[6] = k7table[extract_bits(tms, 4)];
    tms->new_k[7] = k8table[extract_bits(tms, 3)];
    tms->new_k[8] = k9table[extract_bits(tms, 3)];
    tms->new_k[9] = k10table[extract_bits(tms, 3)];
	
    if (DEBUG_5220) logerror("  (50-bit energy=%d pitch=%d rep=%d 10K frame)\n", tms->new_energy, tms->new_pitch, rep_flag);
	
done:
		if (DEBUG_5220)
		{
			if (tms->speak_external)
				logerror("Parsed a frame successfully in FIFO - %d bits remaining\n", bits);
			else
				logerror("Parsed a frame successfully in ROM\n");
		}
	
	if (the_first_frame)
	{
		/* if this is the first frame, no previous frame to take as a starting point */
		tms->old_energy = tms->new_energy;
		tms->old_pitch = tms->new_pitch;
		for (i = 0; i < 10; i++)
			tms->old_k[i] = tms->new_k[i];
    }
	
    /* update the tms->buffer_low status */
    check_buffer_low(tms);
    return 1;
	
ranout:
		
	if (DEBUG_5220) logerror("Ran out of bits on a parse!\n");
	
    /* this is an error condition; mark the buffer empty and turn off speaking */
    tms->buffer_empty = 1;
	tms->talk_status = tms->speak_external = tms->tms5220_speaking = the_first_frame = tms->last_frame = 0;
    tms->fifo_count = tms->fifo_head = tms->fifo_tail = 0;
	
	tms->RDB_flag = FALSE;
	
    /* generate an interrupt if necessary */
    set_interrupt_state(tms, 1);
    return 0;
}



/**********************************************************************************************

check_buffer_low -- check to see if the buffer low flag should be on or off

***********************************************************************************************/

static void check_buffer_low(struct tms5220 *tms)
{
    /* did we just become low? */
    if (tms->fifo_count <= 8)
    {
        /* generate an interrupt if necessary */
        if (!tms->buffer_low)
            set_interrupt_state(tms, 1);
        tms->buffer_low = 1;
		
        if (DEBUG_5220) logerror("Buffer low set\n");
    }
	
    /* did we just become full? */
    else
    {
        tms->buffer_low = 0;
		
        if (DEBUG_5220) logerror("Buffer low cleared\n");
    }
}



/**********************************************************************************************

set_interrupt_state -- generate an interrupt

***********************************************************************************************/

static void set_interrupt_state(struct tms5220 *tms, int state)
{

	if (state != tms->irq_pin)
    	my_irq(state);
    tms->irq_pin = state;

}

/*********************************************************************************************/

static void SpeechError(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	mainWin->Report(MessageType::Error, fmt, ap);
	va_end(ap);
}
