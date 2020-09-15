/* Host opcode support for Beebem */
/* Written by J.G.Harston         */

#ifndef HOST_HEADER
#define HOST_HEADER

extern DWORD EmulatorTrap;

int host_emt(bool dorts);  // &03
int host_byte(bool dorts); // &13
int host_word(bool dorts); // &23
int host_wrch(bool dorts); // &33
int host_rdch(bool dorts); // &43
int host_file(bool dorts); // &53
int host_args(bool dorts); // &63
int host_bget(bool dorts); // &73
int host_bput(bool dorts); // &83
int host_gbpb(bool dorts); // &93
int host_find(bool dorts); // &A3
int host_lang(bool dorts); // &C3
int host_D3(bool dorts);   // &D3
int host_E3(bool dorts);   // &E3
int host_F3(bool dorts);   // &F3
int host_fsc(bool dorts);

#endif
