/* Host opcode support for Beebem */
/* Written by J.G.Harston         */

#ifndef HOST_HEADER
#define HOST_HEADER
extern char EmulatorTrap;
int host_emt(int rts);  // &03
int host_byte(int rts); // &13
int host_word(int rts); // &23
int host_wrch(int rts); // &33
int host_rdch(int rts); // &43
int host_file(int rts); // &53
int host_args(int rts); // &63
int host_bget(int rts); // &73
int host_bput(int rts); // &83
int host_gbpb(int rts); // &93
int host_find(int rts); // &A3
int host_quit(int rts); // &B3
int host_lang(int rts); // &C3
int host_D3(int rts);   // &D3
int host_E3(int rts);   // &E3
int host_F3(int rts);   // &F3
int host_fsc(int dorts);
#endif
