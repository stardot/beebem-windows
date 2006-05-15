/****************************************************************************
*             real mode i286 emulator v1.4 by Fabrice Frances               *
*               (initial work based on David Hedley's pcemu)                *
****************************************************************************/

// file will be included in all cpu variants
// put non i86 instructions in own files (i286, i386, nec)
// function renaming will be added when neccessary
// timing value should move to separate array

static void PREFIX86(_add_br8)(void);
static void PREFIX86(_add_wr16)(void);
static void PREFIX86(_add_r8b)(void);
static void PREFIX86(_add_r16w)(void);
static void PREFIX86(_add_ald8)(void);
static void PREFIX86(_add_axd16)(void);
static void PREFIX86(_push_es)(void);
static void PREFIX86(_pop_es)(void);
static void PREFIX86(_or_br8)(void);
static void PREFIX86(_or_r8b)(void);
static void PREFIX86(_or_wr16)(void);
static void PREFIX86(_or_r16w)(void);
static void PREFIX86(_or_ald8)(void);
static void PREFIX86(_or_axd16)(void);
static void PREFIX86(_push_cs)(void);
static void PREFIX86(_adc_br8)(void);
static void PREFIX86(_adc_wr16)(void);
static void PREFIX86(_adc_r8b)(void);
static void PREFIX86(_adc_r16w)(void);
static void PREFIX86(_adc_ald8)(void);
static void PREFIX86(_adc_axd16)(void);
static void PREFIX86(_push_ss)(void);
static void PREFIX86(_pop_ss)(void);
static void PREFIX86(_sbb_br8)(void);
static void PREFIX86(_sbb_wr16)(void);
static void PREFIX86(_sbb_r8b)(void);
static void PREFIX86(_sbb_r16w)(void);
static void PREFIX86(_sbb_ald8)(void);
static void PREFIX86(_sbb_axd16)(void);
static void PREFIX86(_push_ds)(void);
static void PREFIX86(_pop_ds)(void);
static void PREFIX86(_and_br8)(void);
static void PREFIX86(_and_r8b)(void);
static void PREFIX86(_and_wr16)(void);
static void PREFIX86(_and_r16w)(void);
static void PREFIX86(_and_ald8)(void);
static void PREFIX86(_and_axd16)(void);
static void PREFIX86(_es)(void);
static void PREFIX86(_daa)(void);
static void PREFIX86(_sub_br8)(void);
static void PREFIX86(_sub_wr16)(void);
static void PREFIX86(_sub_r8b)(void);
static void PREFIX86(_sub_r16w)(void);
static void PREFIX86(_sub_ald8)(void);
static void PREFIX86(_sub_axd16)(void);
static void PREFIX86(_cs)(void);
static void PREFIX86(_das)(void);
static void PREFIX86(_xor_br8)(void);
static void PREFIX86(_xor_r8b)(void);
static void PREFIX86(_xor_wr16)(void);
static void PREFIX86(_xor_r16w)(void);
static void PREFIX86(_xor_ald8)(void);
static void PREFIX86(_xor_axd16)(void);
static void PREFIX86(_ss)(void);
static void PREFIX86(_aaa)(void);
static void PREFIX86(_cmp_br8)(void);
static void PREFIX86(_cmp_wr16)(void);
static void PREFIX86(_cmp_r8b)(void);
static void PREFIX86(_cmp_r16w)(void);
static void PREFIX86(_cmp_ald8)(void);
static void PREFIX86(_cmp_axd16)(void);
static void PREFIX86(_ds)(void);
static void PREFIX86(_aas)(void);
static void PREFIX86(_inc_ax)(void);
static void PREFIX86(_inc_cx)(void);
static void PREFIX86(_inc_dx)(void);
static void PREFIX86(_inc_bx)(void);
static void PREFIX86(_inc_sp)(void);
static void PREFIX86(_inc_bp)(void);
static void PREFIX86(_inc_si)(void);
static void PREFIX86(_inc_di)(void);
static void PREFIX86(_dec_ax)(void);
static void PREFIX86(_dec_cx)(void);
static void PREFIX86(_dec_dx)(void);
static void PREFIX86(_dec_bx)(void);
static void PREFIX86(_dec_sp)(void);
static void PREFIX86(_dec_bp)(void);
static void PREFIX86(_dec_si)(void);
static void PREFIX86(_dec_di)(void);
static void PREFIX86(_push_ax)(void);
static void PREFIX86(_push_cx)(void);
static void PREFIX86(_push_dx)(void);
static void PREFIX86(_push_bx)(void);
static void PREFIX86(_push_sp)(void);
static void PREFIX86(_push_bp)(void);
static void PREFIX86(_push_si)(void);
static void PREFIX86(_push_di)(void);
static void PREFIX86(_pop_ax)(void);
static void PREFIX86(_pop_cx)(void);
static void PREFIX86(_pop_dx)(void);
static void PREFIX86(_pop_bx)(void);
static void PREFIX86(_pop_sp)(void);
static void PREFIX86(_pop_bp)(void);
static void PREFIX86(_pop_si)(void);
static void PREFIX86(_pop_di)(void);
static void PREFIX86(_jo)(void);
static void PREFIX86(_jno)(void);
static void PREFIX86(_jb)(void);
static void PREFIX86(_jnb)(void);
static void PREFIX86(_jz)(void);
static void PREFIX86(_jnz)(void);
static void PREFIX86(_jbe)(void);
static void PREFIX86(_jnbe)(void);
static void PREFIX86(_js)(void);
static void PREFIX86(_jns)(void);
static void PREFIX86(_jp)(void);
static void PREFIX86(_jnp)(void);
static void PREFIX86(_jl)(void);
static void PREFIX86(_jnl)(void);
static void PREFIX86(_jle)(void);
static void PREFIX86(_jnle)(void);
static void PREFIX86(_80pre)(void);
static void PREFIX86(_82pre)(void);
static void PREFIX86(_81pre)(void);
static void PREFIX86(_83pre)(void);
static void PREFIX86(_test_br8)(void);
static void PREFIX86(_test_wr16)(void);
static void PREFIX86(_xchg_br8)(void);
static void PREFIX86(_xchg_wr16)(void);
static void PREFIX86(_mov_br8)(void);
static void PREFIX86(_mov_r8b)(void);
static void PREFIX86(_mov_wr16)(void);
static void PREFIX86(_mov_r16w)(void);
static void PREFIX86(_mov_wsreg)(void);
static void PREFIX86(_lea)(void);
static void PREFIX86(_mov_sregw)(void);
static void PREFIX86(_invalid)(void);
static void PREFIX86(_popw)(void);
static void PREFIX86(_nop)(void);
static void PREFIX86(_xchg_axcx)(void);
static void PREFIX86(_xchg_axdx)(void);
static void PREFIX86(_xchg_axbx)(void);
static void PREFIX86(_xchg_axsp)(void);
static void PREFIX86(_xchg_axbp)(void);
static void PREFIX86(_xchg_axsi)(void);
static void PREFIX86(_xchg_axdi)(void);
static void PREFIX86(_cbw)(void);
static void PREFIX86(_cwd)(void);
static void PREFIX86(_call_far)(void);
static void PREFIX86(_pushf)(void);
static void PREFIX86(_popf)(void);
static void PREFIX86(_sahf)(void);
static void PREFIX86(_lahf)(void);
static void PREFIX86(_mov_aldisp)(void);
static void PREFIX86(_mov_axdisp)(void);
static void PREFIX86(_mov_dispal)(void);
static void PREFIX86(_mov_dispax)(void);
static void PREFIX86(_movsb)(void);
static void PREFIX86(_movsw)(void);
static void PREFIX86(_cmpsb)(void);
static void PREFIX86(_cmpsw)(void);
static void PREFIX86(_test_ald8)(void);
static void PREFIX86(_test_axd16)(void);
static void PREFIX86(_stosb)(void);
static void PREFIX86(_stosw)(void);
static void PREFIX86(_lodsb)(void);
static void PREFIX86(_lodsw)(void);
static void PREFIX86(_scasb)(void);
static void PREFIX86(_scasw)(void);
static void PREFIX86(_mov_ald8)(void);
static void PREFIX86(_mov_cld8)(void);
static void PREFIX86(_mov_dld8)(void);
static void PREFIX86(_mov_bld8)(void);
static void PREFIX86(_mov_ahd8)(void);
static void PREFIX86(_mov_chd8)(void);
static void PREFIX86(_mov_dhd8)(void);
static void PREFIX86(_mov_bhd8)(void);
static void PREFIX86(_mov_axd16)(void);
static void PREFIX86(_mov_cxd16)(void);
static void PREFIX86(_mov_dxd16)(void);
static void PREFIX86(_mov_bxd16)(void);
static void PREFIX86(_mov_spd16)(void);
static void PREFIX86(_mov_bpd16)(void);
static void PREFIX86(_mov_sid16)(void);
static void PREFIX86(_mov_did16)(void);
static void PREFIX86(_ret_d16)(void);
static void PREFIX86(_ret)(void);
static void PREFIX86(_les_dw)(void);
static void PREFIX86(_lds_dw)(void);
static void PREFIX86(_mov_bd8)(void);
static void PREFIX86(_mov_wd16)(void);
static void PREFIX86(_retf_d16)(void);
static void PREFIX86(_retf)(void);
static void PREFIX86(_int3)(void);
static void PREFIX86(_int)(void);
static void PREFIX86(_into)(void);
static void PREFIX86(_iret)(void);
static void PREFIX86(_rotshft_b)(void);
static void PREFIX86(_rotshft_w)(void);
static void PREFIX86(_rotshft_bcl)(void);
static void PREFIX86(_rotshft_wcl)(void);
static void PREFIX86(_aam)(void);
static void PREFIX86(_aad)(void);
static void PREFIX86(_xlat)(void);
static void PREFIX86(_escape)(void);
static void PREFIX86(_loopne)(void);
static void PREFIX86(_loope)(void);
static void PREFIX86(_loop)(void);
static void PREFIX86(_jcxz)(void);
static void PREFIX86(_inal)(void);
static void PREFIX86(_inax)(void);
static void PREFIX86(_outal)(void);
static void PREFIX86(_outax)(void);
static void PREFIX86(_call_d16)(void);
static void PREFIX86(_jmp_d16)(void);
static void PREFIX86(_jmp_far)(void);
static void PREFIX86(_jmp_d8)(void);
static void PREFIX86(_inaldx)(void);
static void PREFIX86(_inaxdx)(void);
static void PREFIX86(_outdxal)(void);
static void PREFIX86(_outdxax)(void);
static void PREFIX86(_lock)(void);
static void PREFIX86(_repne)(void);
static void PREFIX86(_repe)(void);
static void PREFIX86(_hlt)(void);
static void PREFIX86(_cmc)(void);
static void PREFIX86(_f6pre)(void);
static void PREFIX86(_f7pre)(void);
static void PREFIX86(_clc)(void);
static void PREFIX86(_stc)(void);
static void PREFIX86(_cli)(void);
static void PREFIX86(_sti)(void);
static void PREFIX86(_cld)(void);
static void PREFIX86(_std)(void);
static void PREFIX86(_fepre)(void);
static void PREFIX86(_ffpre)(void);
static void PREFIX86(_wait)(void);

static void PREFIX186(_pusha)(void);
static void PREFIX186(_popa)(void);
static void PREFIX186(_bound)(void);
static void PREFIX186(_push_d16)(void);
static void PREFIX186(_imul_d16)(void);
static void PREFIX186(_push_d8)(void);
static void PREFIX186(_imul_d8)(void);
static void PREFIX186(_rotshft_bd8)(void);
static void PREFIX186(_rotshft_wd8)(void);
static void PREFIX186(_enter)(void);
static void PREFIX186(_leave)(void);
static void PREFIX186(_insb)(void);
static void PREFIX186(_insw)(void);
static void PREFIX186(_outsb)(void);
static void PREFIX186(_outsw)(void);

/* changed instructions */
static void PREFIX186(_repne)(void);
static void PREFIX186(_repe)(void);


static void (*PREFIX186(_instruction)[256])(void) =
{
	 PREFIX186(_add_br8),           /* 0x00 */
	 PREFIX186(_add_wr16),          /* 0x01 */
	 PREFIX186(_add_r8b),           /* 0x02 */
	 PREFIX186(_add_r16w),          /* 0x03 */
	 PREFIX186(_add_ald8),          /* 0x04 */
	 PREFIX186(_add_axd16),         /* 0x05 */
	 PREFIX186(_push_es),           /* 0x06 */
	 PREFIX186(_pop_es),            /* 0x07 */
	 PREFIX186(_or_br8),            /* 0x08 */
	 PREFIX186(_or_wr16),           /* 0x09 */
	 PREFIX186(_or_r8b),            /* 0x0a */
	 PREFIX186(_or_r16w),           /* 0x0b */
	 PREFIX186(_or_ald8),           /* 0x0c */
	 PREFIX186(_or_axd16),          /* 0x0d */
	 PREFIX186(_push_cs),           /* 0x0e */
	 PREFIX186(_invalid),
	 PREFIX186(_adc_br8),           /* 0x10 */
	 PREFIX186(_adc_wr16),          /* 0x11 */
	 PREFIX186(_adc_r8b),           /* 0x12 */
	 PREFIX186(_adc_r16w),          /* 0x13 */
	 PREFIX186(_adc_ald8),          /* 0x14 */
	 PREFIX186(_adc_axd16),         /* 0x15 */
	 PREFIX186(_push_ss),           /* 0x16 */
	 PREFIX186(_pop_ss),            /* 0x17 */
	 PREFIX186(_sbb_br8),           /* 0x18 */
	 PREFIX186(_sbb_wr16),          /* 0x19 */
	 PREFIX186(_sbb_r8b),           /* 0x1a */
	 PREFIX186(_sbb_r16w),          /* 0x1b */
	 PREFIX186(_sbb_ald8),          /* 0x1c */
	 PREFIX186(_sbb_axd16),         /* 0x1d */
	 PREFIX186(_push_ds),           /* 0x1e */
	 PREFIX186(_pop_ds),            /* 0x1f */
	 PREFIX186(_and_br8),           /* 0x20 */
	 PREFIX186(_and_wr16),          /* 0x21 */
	 PREFIX186(_and_r8b),           /* 0x22 */
	 PREFIX186(_and_r16w),          /* 0x23 */
	 PREFIX186(_and_ald8),          /* 0x24 */
	 PREFIX186(_and_axd16),         /* 0x25 */
	 PREFIX186(_es),                /* 0x26 */
	 PREFIX186(_daa),               /* 0x27 */
	 PREFIX186(_sub_br8),           /* 0x28 */
	 PREFIX186(_sub_wr16),          /* 0x29 */
	 PREFIX186(_sub_r8b),           /* 0x2a */
	 PREFIX186(_sub_r16w),          /* 0x2b */
	 PREFIX186(_sub_ald8),          /* 0x2c */
	 PREFIX186(_sub_axd16),         /* 0x2d */
	 PREFIX186(_cs),                /* 0x2e */
	 PREFIX186(_das), 		/* 0x2f */
	 PREFIX186(_xor_br8),           /* 0x30 */
	 PREFIX186(_xor_wr16),          /* 0x31 */
	 PREFIX186(_xor_r8b),           /* 0x32 */
	 PREFIX186(_xor_r16w),          /* 0x33 */
	 PREFIX186(_xor_ald8),          /* 0x34 */
	 PREFIX186(_xor_axd16),         /* 0x35 */
	 PREFIX186(_ss),                /* 0x36 */
	 PREFIX186(_aaa), 		/* 0x37 */
	 PREFIX186(_cmp_br8),           /* 0x38 */
	 PREFIX186(_cmp_wr16),          /* 0x39 */
	 PREFIX186(_cmp_r8b),           /* 0x3a */
	 PREFIX186(_cmp_r16w),          /* 0x3b */
	 PREFIX186(_cmp_ald8),          /* 0x3c */
	 PREFIX186(_cmp_axd16),         /* 0x3d */
	 PREFIX186(_ds),                /* 0x3e */
	 PREFIX186(_aas), 		/* 0x3f */
	 PREFIX186(_inc_ax),            /* 0x40 */
	 PREFIX186(_inc_cx),            /* 0x41 */
	 PREFIX186(_inc_dx),            /* 0x42 */
	 PREFIX186(_inc_bx),            /* 0x43 */
	 PREFIX186(_inc_sp),            /* 0x44 */
	 PREFIX186(_inc_bp),            /* 0x45 */
	 PREFIX186(_inc_si),            /* 0x46 */
	 PREFIX186(_inc_di),            /* 0x47 */
	 PREFIX186(_dec_ax),            /* 0x48 */
	 PREFIX186(_dec_cx),            /* 0x49 */
	 PREFIX186(_dec_dx),            /* 0x4a */
	 PREFIX186(_dec_bx),            /* 0x4b */
	 PREFIX186(_dec_sp),            /* 0x4c */
	 PREFIX186(_dec_bp),            /* 0x4d */
	 PREFIX186(_dec_si),            /* 0x4e */
	 PREFIX186(_dec_di),            /* 0x4f */
	 PREFIX186(_push_ax),           /* 0x50 */
	 PREFIX186(_push_cx),           /* 0x51 */
	 PREFIX186(_push_dx),           /* 0x52 */
	 PREFIX186(_push_bx),           /* 0x53 */
	 PREFIX186(_push_sp),           /* 0x54 */
	 PREFIX186(_push_bp),           /* 0x55 */
	 PREFIX186(_push_si),           /* 0x56 */
	 PREFIX186(_push_di),           /* 0x57 */
	 PREFIX186(_pop_ax),            /* 0x58 */
	 PREFIX186(_pop_cx),            /* 0x59 */
	 PREFIX186(_pop_dx),            /* 0x5a */
	 PREFIX186(_pop_bx),            /* 0x5b */
	 PREFIX186(_pop_sp),            /* 0x5c */
	 PREFIX186(_pop_bp),            /* 0x5d */
	 PREFIX186(_pop_si),            /* 0x5e */
	 PREFIX186(_pop_di),            /* 0x5f */
	 PREFIX186(_pusha),             /* 0x60 */
	 PREFIX186(_popa),              /* 0x61 */
	 PREFIX186(_bound),             /* 0x62 */
	 PREFIX186(_invalid),
	 PREFIX186(_invalid),
	 PREFIX186(_invalid),
	 PREFIX186(_invalid),
	 PREFIX186(_invalid),
	 PREFIX186(_push_d16),          /* 0x68 */
	 PREFIX186(_imul_d16),          /* 0x69 */
	 PREFIX186(_push_d8),           /* 0x6a */
	 PREFIX186(_imul_d8),           /* 0x6b */
	 PREFIX186(_insb),              /* 0x6c */
	 PREFIX186(_insw),              /* 0x6d */
	 PREFIX186(_outsb),             /* 0x6e */
	 PREFIX186(_outsw),             /* 0x6f */
	 PREFIX186(_jo),                /* 0x70 */
	 PREFIX186(_jno),               /* 0x71 */
	 PREFIX186(_jb),                /* 0x72 */
	 PREFIX186(_jnb),               /* 0x73 */
	 PREFIX186(_jz),                /* 0x74 */
	 PREFIX186(_jnz),               /* 0x75 */
	 PREFIX186(_jbe),               /* 0x76 */
	 PREFIX186(_jnbe),              /* 0x77 */
	 PREFIX186(_js),                /* 0x78 */
	 PREFIX186(_jns),               /* 0x79 */
	 PREFIX186(_jp),                /* 0x7a */
	 PREFIX186(_jnp),               /* 0x7b */
	 PREFIX186(_jl),                /* 0x7c */
	 PREFIX186(_jnl),               /* 0x7d */
	 PREFIX186(_jle),               /* 0x7e */
	 PREFIX186(_jnle),              /* 0x7f */
	 PREFIX186(_80pre),             /* 0x80 */
	 PREFIX186(_81pre),             /* 0x81 */
	 PREFIX186(_82pre), 			/* 0x82 */
	 PREFIX186(_83pre),             /* 0x83 */
	 PREFIX186(_test_br8),          /* 0x84 */
	 PREFIX186(_test_wr16),         /* 0x85 */
	 PREFIX186(_xchg_br8),          /* 0x86 */
	 PREFIX186(_xchg_wr16),         /* 0x87 */
	 PREFIX186(_mov_br8),           /* 0x88 */
	 PREFIX186(_mov_wr16),          /* 0x89 */
	 PREFIX186(_mov_r8b),           /* 0x8a */
	 PREFIX186(_mov_r16w),          /* 0x8b */
	 PREFIX186(_mov_wsreg),         /* 0x8c */
	 PREFIX186(_lea),               /* 0x8d */
	 PREFIX186(_mov_sregw),         /* 0x8e */
	 PREFIX186(_popw),              /* 0x8f */
	 PREFIX186(_nop),               /* 0x90 */
	 PREFIX186(_xchg_axcx),         /* 0x91 */
	 PREFIX186(_xchg_axdx),         /* 0x92 */
	 PREFIX186(_xchg_axbx),         /* 0x93 */
	 PREFIX186(_xchg_axsp),         /* 0x94 */
	 PREFIX186(_xchg_axbp),         /* 0x95 */
	 PREFIX186(_xchg_axsi),         /* 0x97 */
	 PREFIX186(_xchg_axdi),         /* 0x97 */
	 PREFIX186(_cbw),               /* 0x98 */
	 PREFIX186(_cwd),               /* 0x99 */
	 PREFIX186(_call_far),          /* 0x9a */
	 PREFIX186(_wait),              /* 0x9b */
	 PREFIX186(_pushf),             /* 0x9c */
	 PREFIX186(_popf),              /* 0x9d */
	 PREFIX186(_sahf),              /* 0x9e */
	 PREFIX186(_lahf),              /* 0x9f */
	 PREFIX186(_mov_aldisp),        /* 0xa0 */
	 PREFIX186(_mov_axdisp),        /* 0xa1 */
	 PREFIX186(_mov_dispal),        /* 0xa2 */
	 PREFIX186(_mov_dispax),        /* 0xa3 */
	 PREFIX186(_movsb),             /* 0xa4 */
	 PREFIX186(_movsw),             /* 0xa5 */
	 PREFIX186(_cmpsb),             /* 0xa6 */
	 PREFIX186(_cmpsw),             /* 0xa7 */
	 PREFIX186(_test_ald8),         /* 0xa8 */
	 PREFIX186(_test_axd16),        /* 0xa9 */
	 PREFIX186(_stosb),             /* 0xaa */
	 PREFIX186(_stosw),             /* 0xab */
	 PREFIX186(_lodsb),             /* 0xac */
	 PREFIX186(_lodsw),             /* 0xad */
	 PREFIX186(_scasb),             /* 0xae */
	 PREFIX186(_scasw),             /* 0xaf */
	 PREFIX186(_mov_ald8),          /* 0xb0 */
	 PREFIX186(_mov_cld8),          /* 0xb1 */
	 PREFIX186(_mov_dld8),          /* 0xb2 */
	 PREFIX186(_mov_bld8),          /* 0xb3 */
	 PREFIX186(_mov_ahd8),          /* 0xb4 */
	 PREFIX186(_mov_chd8),          /* 0xb5 */
	 PREFIX186(_mov_dhd8),          /* 0xb6 */
	 PREFIX186(_mov_bhd8),          /* 0xb7 */
	 PREFIX186(_mov_axd16),         /* 0xb8 */
	 PREFIX186(_mov_cxd16),         /* 0xb9 */
	 PREFIX186(_mov_dxd16),         /* 0xba */
	 PREFIX186(_mov_bxd16),         /* 0xbb */
	 PREFIX186(_mov_spd16),         /* 0xbc */
	 PREFIX186(_mov_bpd16),         /* 0xbd */
	 PREFIX186(_mov_sid16),         /* 0xbe */
	 PREFIX186(_mov_did16),         /* 0xbf */
	 PREFIX186(_rotshft_bd8),       /* 0xc0 */
	 PREFIX186(_rotshft_wd8),       /* 0xc1 */
	 PREFIX186(_ret_d16),           /* 0xc2 */
	 PREFIX186(_ret),               /* 0xc3 */
	 PREFIX186(_les_dw),            /* 0xc4 */
	 PREFIX186(_lds_dw),            /* 0xc5 */
	 PREFIX186(_mov_bd8),           /* 0xc6 */
	 PREFIX186(_mov_wd16),          /* 0xc7 */
	 PREFIX186(_enter),             /* 0xc8 */
	 PREFIX186(_leave),             /* 0xc9 */
	 PREFIX186(_retf_d16),          /* 0xca */
	 PREFIX186(_retf),              /* 0xcb */
	 PREFIX186(_int3),              /* 0xcc */
	 PREFIX186(_int),               /* 0xcd */
	 PREFIX186(_into),              /* 0xce */
	 PREFIX186(_iret),              /* 0xcf */
	 PREFIX186(_rotshft_b),         /* 0xd0 */
	 PREFIX186(_rotshft_w),         /* 0xd1 */
	 PREFIX186(_rotshft_bcl),       /* 0xd2 */
	 PREFIX186(_rotshft_wcl),       /* 0xd3 */
	 PREFIX186(_aam),               /* 0xd4 */
	 PREFIX186(_aad),               /* 0xd5 */
	 PREFIX186(_invalid),
	 PREFIX186(_xlat),              /* 0xd7 */
	 PREFIX186(_escape),            /* 0xd8 */
	 PREFIX186(_escape),            /* 0xd9 */
	 PREFIX186(_escape),            /* 0xda */
	 PREFIX186(_escape),            /* 0xdb */
	 PREFIX186(_escape),            /* 0xdc */
	 PREFIX186(_escape),            /* 0xdd */
	 PREFIX186(_escape),            /* 0xde */
	 PREFIX186(_escape),            /* 0xdf */
	 PREFIX186(_loopne),            /* 0xe0 */
	 PREFIX186(_loope),             /* 0xe1 */
	 PREFIX186(_loop),              /* 0xe2 */
	 PREFIX186(_jcxz),              /* 0xe3 */
	 PREFIX186(_inal),              /* 0xe4 */
	 PREFIX186(_inax),              /* 0xe5 */
	 PREFIX186(_outal),             /* 0xe6 */
	 PREFIX186(_outax),             /* 0xe7 */
	 PREFIX186(_call_d16),          /* 0xe8 */
	 PREFIX186(_jmp_d16),           /* 0xe9 */
	 PREFIX186(_jmp_far),           /* 0xea */
	 PREFIX186(_jmp_d8),            /* 0xeb */
	 PREFIX186(_inaldx),            /* 0xec */
	 PREFIX186(_inaxdx),            /* 0xed */
	 PREFIX186(_outdxal),           /* 0xee */
	 PREFIX186(_outdxax),           /* 0xef */
	 PREFIX186(_lock),              /* 0xf0 */
	 PREFIX186(_invalid),           /* 0xf1 */
	 PREFIX186(_repne),             /* 0xf2 */
	 PREFIX186(_repe),              /* 0xf3 */
	 PREFIX186(_hlt), 		/* 0xf4 */
	 PREFIX186(_cmc),               /* 0xf5 */
	 PREFIX186(_f6pre),             /* 0xf6 */
	 PREFIX186(_f7pre),             /* 0xf7 */
	 PREFIX186(_clc),               /* 0xf8 */
	 PREFIX186(_stc),               /* 0xf9 */
	 PREFIX186(_cli),               /* 0xfa */
	 PREFIX186(_sti),               /* 0xfb */
	 PREFIX186(_cld),               /* 0xfc */
	 PREFIX186(_std),               /* 0xfd */
	 PREFIX186(_fepre),             /* 0xfe */
	 PREFIX186(_ffpre)             /* 0xff */
};

#define TABLE186 PREFIX186(_instruction)[FETCHOP]();
