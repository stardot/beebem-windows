UINT8 new_cpu_readop(offs_t A);
UINT8 new_cpu_readop_arg(offs_t A);

static unsigned EA;
static UINT16 EO; /* HJB 12/13/98 effective offset of the address (before segment is added) */

static unsigned EA_000(void) { i86_ICount-=7; EO=(WORD)(I.regs.w[BX]+I.regs.w[SI]); EA=DefaultBase(DS)+EO; return EA; }
static unsigned EA_001(void) { i86_ICount-=8; EO=(WORD)(I.regs.w[BX]+I.regs.w[DI]); EA=DefaultBase(DS)+EO; return EA; }
static unsigned EA_002(void) { i86_ICount-=8; EO=(WORD)(I.regs.w[BP]+I.regs.w[SI]); EA=DefaultBase(SS)+EO; return EA; }
static unsigned EA_003(void) { i86_ICount-=7; EO=(WORD)(I.regs.w[BP]+I.regs.w[DI]); EA=DefaultBase(SS)+EO; return EA; }
static unsigned EA_004(void) { i86_ICount-=5; EO=I.regs.w[SI]; EA=DefaultBase(DS)+EO; return EA; }
static unsigned EA_005(void) { i86_ICount-=5; EO=I.regs.w[DI]; EA=DefaultBase(DS)+EO; return EA; }
static unsigned EA_006(void) { i86_ICount-=6; EO=FETCHOP; EO+=FETCHOP<<8; EA=DefaultBase(DS)+EO; return EA; }
static unsigned EA_007(void) { i86_ICount-=5; EO=I.regs.w[BX]; EA=DefaultBase(DS)+EO; return EA; }

static unsigned EA_100(void) { i86_ICount-=11; EO=(WORD)(I.regs.w[BX]+I.regs.w[SI]+(INT8)FETCHOP); EA=DefaultBase(DS)+EO; return EA; }
static unsigned EA_101(void) { i86_ICount-=12; EO=(WORD)(I.regs.w[BX]+I.regs.w[DI]+(INT8)FETCHOP); EA=DefaultBase(DS)+EO; return EA; }
static unsigned EA_102(void) { i86_ICount-=12; EO=(WORD)(I.regs.w[BP]+I.regs.w[SI]+(INT8)FETCHOP); EA=DefaultBase(SS)+EO; return EA; }
static unsigned EA_103(void) { i86_ICount-=11; EO=(WORD)(I.regs.w[BP]+I.regs.w[DI]+(INT8)FETCHOP); EA=DefaultBase(SS)+EO; return EA; }
static unsigned EA_104(void) { i86_ICount-=9; EO=(WORD)(I.regs.w[SI]+(INT8)FETCHOP); EA=DefaultBase(DS)+EO; return EA; }
static unsigned EA_105(void) { i86_ICount-=9; EO=(WORD)(I.regs.w[DI]+(INT8)FETCHOP); EA=DefaultBase(DS)+EO; return EA; }
static unsigned EA_106(void) { i86_ICount-=9; EO=(WORD)(I.regs.w[BP]+(INT8)FETCHOP); EA=DefaultBase(SS)+EO; return EA; }
static unsigned EA_107(void) { i86_ICount-=9; EO=(WORD)(I.regs.w[BX]+(INT8)FETCHOP); EA=DefaultBase(DS)+EO; return EA; }

static unsigned EA_200(void) { i86_ICount-=11; EO=FETCHOP; EO+=FETCHOP<<8; EO+=I.regs.w[BX]+I.regs.w[SI]; EA=DefaultBase(DS)+(WORD)EO; return EA; }
static unsigned EA_201(void) { i86_ICount-=12; EO=FETCHOP; EO+=FETCHOP<<8; EO+=I.regs.w[BX]+I.regs.w[DI]; EA=DefaultBase(DS)+(WORD)EO; return EA; }
static unsigned EA_202(void) { i86_ICount-=12; EO=FETCHOP; EO+=FETCHOP<<8; EO+=I.regs.w[BP]+I.regs.w[SI]; EA=DefaultBase(SS)+(WORD)EO; return EA; }
static unsigned EA_203(void) { i86_ICount-=11; EO=FETCHOP; EO+=FETCHOP<<8; EO+=I.regs.w[BP]+I.regs.w[DI]; EA=DefaultBase(SS)+(WORD)EO; return EA; }
static unsigned EA_204(void) { i86_ICount-=9; EO=FETCHOP; EO+=FETCHOP<<8; EO+=I.regs.w[SI]; EA=DefaultBase(DS)+(WORD)EO; return EA; }
static unsigned EA_205(void) { i86_ICount-=9; EO=FETCHOP; EO+=FETCHOP<<8; EO+=I.regs.w[DI]; EA=DefaultBase(DS)+(WORD)EO; return EA; }
static unsigned EA_206(void) { i86_ICount-=9; EO=FETCHOP; EO+=FETCHOP<<8; EO+=I.regs.w[BP]; EA=DefaultBase(SS)+(WORD)EO; return EA; }
static unsigned EA_207(void) { i86_ICount-=9; EO=FETCHOP; EO+=FETCHOP<<8; EO+=I.regs.w[BX]; EA=DefaultBase(DS)+(WORD)EO; return EA; }

static unsigned (*GetEA[192])(void)={
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,

	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,
	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,
	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,
	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,
	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,
	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,
	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,
	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,

	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207,
	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207,
	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207,
	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207,
	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207,
	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207,
	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207,
	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207
};

static struct {
	struct {
		WREGS w[256];
		BREGS b[256];
	} reg;
	struct {
		WREGS w[256];
		BREGS b[256];
	} RM;
} Mod_RM;

#define RegWord(ModRM) I.regs.w[Mod_RM.reg.w[ModRM]]
#define RegByte(ModRM) I.regs.b[Mod_RM.reg.b[ModRM]]

#define GetRMWord(ModRM) \
	((ModRM) >= 0xc0 ? I.regs.w[Mod_RM.RM.w[ModRM]] : ( (*GetEA[ModRM])(), ReadWord( EA ) ))

#define PutbackRMWord(ModRM,val) \
{ \
	if (ModRM >= 0xc0) I.regs.w[Mod_RM.RM.w[ModRM]]=val; \
    else WriteWord(EA,val); \
}

#define GetnextRMWord ReadWord(EA+2)

#define GetRMWordOffset(offs) \
		ReadWord(EA-EO+(UINT16)(EO+offs))

#define GetRMByteOffset(offs) \
		ReadByte(EA-EO+(UINT16)(EO+offs))

#define PutRMWord(ModRM,val)				\
{											\
	if (ModRM >= 0xc0)						\
		I.regs.w[Mod_RM.RM.w[ModRM]]=val;	\
	else {									\
		(*GetEA[ModRM])();					\
		WriteWord( EA ,val);				\
	}										\
}

#define PutRMWordOffset(offs, val) \
		WriteWord( EA-EO+(UINT16)(EO+offs), val)

#define PutRMByteOffset(offs, val) \
		WriteByte( EA-EO+(UINT16)(EO+offs), val)

#define PutImmRMWord(ModRM) 				\
{											\
	WORD val;								\
	if (ModRM >= 0xc0)						\
		FETCHWORD(I.regs.w[Mod_RM.RM.w[ModRM]]) \
	else {									\
		(*GetEA[ModRM])();					\
		FETCHWORD(val)						\
		WriteWord( EA , val);				\
	}										\
}

#define GetRMByte(ModRM) \
	((ModRM) >= 0xc0 ? I.regs.b[Mod_RM.RM.b[ModRM]] : ReadByte( (*GetEA[ModRM])() ))

#define PutRMByte(ModRM,val)				\
{											\
	if (ModRM >= 0xc0)						\
		I.regs.b[Mod_RM.RM.b[ModRM]]=val;	\
	else									\
		WriteByte( (*GetEA[ModRM])() ,val); \
}

#define PutImmRMByte(ModRM) 				\
{											\
	if (ModRM >= 0xc0)						\
		I.regs.b[Mod_RM.RM.b[ModRM]]=FETCH; \
	else {									\
		(*GetEA[ModRM])();					\
		WriteByte( EA , FETCH );			\
	}										\
}

#define PutbackRMByte(ModRM,val)			\
{											\
	if (ModRM >= 0xc0)						\
		I.regs.b[Mod_RM.RM.b[ModRM]]=val;	\
	else									\
		WriteByte(EA,val);					\
}

#define DEF_br8(dst,src)					\
	unsigned ModRM = FETCHOP;				\
	unsigned src = RegByte(ModRM);			\
    unsigned dst = GetRMByte(ModRM)

#define DEF_wr16(dst,src)					\
	unsigned ModRM = FETCHOP;				\
	unsigned src = RegWord(ModRM);			\
    unsigned dst = GetRMWord(ModRM)

#define DEF_r8b(dst,src)					\
	unsigned ModRM = FETCHOP;				\
	unsigned dst = RegByte(ModRM);			\
    unsigned src = GetRMByte(ModRM)

#define DEF_r16w(dst,src)					\
	unsigned ModRM = FETCHOP;				\
	unsigned dst = RegWord(ModRM);			\
    unsigned src = GetRMWord(ModRM)

#define DEF_ald8(dst,src)					\
	unsigned src = FETCHOP; 				\
	unsigned dst = I.regs.b[AL]

#define DEF_axd16(dst,src)					\
	unsigned src = FETCHOP; 				\
	unsigned dst = I.regs.w[AX];			\
    src += (FETCH << 8)
