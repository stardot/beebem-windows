/* clock rate = 80 * output sample rate,     */
/* usually 640000 for 8000 Hz sample rate or */
/* usually 800000 for 10000 Hz sample rate.  */

#define MAX_SAMPLE_CHUNK	8000

#define FRAC_BITS			14
#define FRAC_ONE			(1 << FRAC_BITS)
#define FRAC_MASK			(FRAC_ONE - 1)

#define UINT32			unsigned int
#define UINT16			unsigned short
#define UINT8			unsigned char

#define INT32			int
#define INT16			short
#define INT8			signed char

/* the state of the streamed output */
struct tms5220_info
{
	int last_sample, curr_sample;
	unsigned int source_step;
	unsigned int source_pos;
	int clock;
	struct tms5220 *chip;
};

struct tms5220
{
	/* these contain data that describes the 128-bit data FIFO */
#define FIFO_SIZE 16
	UINT8 fifo[FIFO_SIZE];
	UINT8 fifo_head;
	UINT8 fifo_tail;
	UINT8 fifo_count;
	UINT8 fifo_bits_taken;
	UINT8 phrom_bits_taken;
	
	
	/* these contain global status bits */
	/*
	 R Nabet : speak_external is only set when a speak external command is going on.
	 tms5220_speaking is set whenever a speak or speak external command is going on.
	 Note that we really need to do anything in tms5220_process and play samples only when
	 tms5220_speaking is true.  Else, we can play nothing as well, which is a
	 speed-up...
	 */
	UINT8 tms5220_speaking;	/* Speak or Speak External command in progress */
	UINT8 speak_external;	/* Speak External command in progress */
	UINT8 talk_status; 		/* tms5220 is really currently speaking */
	UINT8 first_frame;		/* we have just started speaking, and we are to parse the first frame */
	UINT8 last_frame;		/* we are doing the frame of sound */
	UINT8 buffer_low;		/* FIFO has less than 8 bytes in it */
	UINT8 buffer_empty;		/* FIFO is empty*/
	UINT8 irq_pin;			/* state of the IRQ pin (output) */
	
	/* these contain data describing the current and previous voice frames */
	UINT16 old_energy;
	UINT16 old_pitch;
	int old_k[10];
	
	UINT16 new_energy;
	UINT16 new_pitch;
	int new_k[10];
	
	
	/* these are all used to contain the current state of the sound generation */
	UINT16 current_energy;
	UINT16 current_pitch;
	int current_k[10];
	
	UINT16 target_energy;
	UINT16 target_pitch;
	int target_k[10];
	
	UINT8 interp_count;		/* number of interp periods (0-7) */
	UINT8 sample_count;		/* sample number within interp (0-24) */
	int pitch_count;
	
	int u[11];
	int x[10];
	
	INT8 randbit;
	
	int phrom_address;
	
	UINT8 data_register;				/* data register, used by read command */
	int RDB_flag;					/* whether we should read data register or status register */
	
};

int tms5220_ready_r(void);
double tms5220_time_to_ready(void);
int tms5220_int_r(void);

void tms5220_set_frequency(int frequency);

struct tms5220 *tms5220_create(void);
void tms5220_destroy(struct tms5220 *chip);

void tms5220_reset_chip(struct tms5220 *chip);

void tms5220_data_write(struct tms5220 *chip, int data);
int tms5220_status_read(struct tms5220 *chip);
int tms5220_ready_read(struct tms5220 *chip);
int tms5220_cycles_to_ready(struct tms5220 *chip);
int tms5220_int_read(struct tms5220 *chip);

void tms5220_process(struct tms5220 *chip, short int *buffer, unsigned int size);

void tms5220_start(void);
void tms5220_stop(void);
void tms5220_data_w(int data);
int tms5220_status_r(void);
int tms5220_ready_r(void);
int tms5220_int_r(void);
void tms5220_update(unsigned char *buff, int length);

extern int SpeechDefault;
extern struct tms5220_info *tms5220;
extern int SpeechEnabled;
