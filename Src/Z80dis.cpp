/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public
License along with this program; if not, write to the Free
Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA  02110-1301, USA.
****************************************************************/

#include <stdio.h>
#include <string.h>

#include "Z80mem.h"
#include "Z80.h"
#include "Tube.h"

int Z80_Disassemble(int adr, char *s)
{
	unsigned char a = ReadZ80Mem(adr);
	unsigned char d = (a >> 3) & 7;
	unsigned char e = a & 7;

	static const char* const reg[8] = {"B", "C", "D", "E", "H", "L", "(HL)", "A"};
	static const char* const dreg[4] = {"BC", "DE", "HL", "SP"};
	static const char* const cond[8] = {"NZ", "Z", "NC", "C", "PO", "PE", "P", "M"};
	static const char* const arith[8] = {"ADD\tA,", "ADC\tA,", "SUB\t", "SBC\tA,", "AND\t", "XOR\t", "OR\t", "CP\t"};
	char stemp[80]; // temp.String for sprintf()
	char ireg[3]; // temp.Indexregister
	int size = 1;

	switch(a & 0xC0) {
	case 0x00:
		switch(e) {
		case 0x00:
			switch(d) {
			case 0x00:
				strcpy(s,"NOP");
				break;
			case 0x01:
				strcpy(s,"EX\tAF,AF'");
				break;
			case 0x02:
				strcpy(s,"DJNZ\t");
				sprintf(stemp,"%4.4Xh",adr+2+(signed char)ReadZ80Mem(adr+1));strcat(s,stemp);
				size = 2;
				break;
			case 0x03:
				strcpy(s,"JR\t");
				sprintf(stemp,"%4.4Xh",adr+2+(signed char)ReadZ80Mem(adr+1));strcat(s,stemp);
				size = 2;
				break;
			default:
				strcpy(s,"JR\t");
				strcat(s,cond[d & 3]);
				strcat(s,",");
				sprintf(stemp,"%4.4Xh",adr+2+(signed char)ReadZ80Mem(adr+1));strcat(s,stemp);
				size = 2;
				break;
			}
			break;
		case 0x01:
			if(a & 0x08) {
				strcpy(s,"ADD\tHL,");
				strcat(s,dreg[d >> 1]);
			} else {
				strcpy(s,"LD\t");
				strcat(s,dreg[d >> 1]);
				strcat(s,",");
				sprintf(stemp,"%4.4Xh",ReadZ80Mem(adr+1)+(ReadZ80Mem(adr+2)<<8));strcat(s,stemp);
				size = 3;
			}
			break;
		case 0x02:
			switch(d) {
			case 0x00:
				strcpy(s,"LD\t(BC),A");
				break;
			case 0x01:
				strcpy(s,"LD\tA,(BC)");
				break;
			case 0x02:
				strcpy(s,"LD\t(DE),A");
				break;
			case 0x03:
				strcpy(s,"LD\tA,(DE)");
				break;
			case 0x04:
				strcpy(s,"LD\t(");
				sprintf(stemp,"%4.4Xh",ReadZ80Mem(adr+1)+(ReadZ80Mem(adr+2)<<8));strcat(s,stemp);
				strcat(s,"),HL");
				size = 3;
				break;
			case 0x05:
				strcpy(s,"LD\tHL,(");
				sprintf(stemp,"%4.4Xh",ReadZ80Mem(adr+1)+(ReadZ80Mem(adr+2)<<8));strcat(s,stemp);
				strcat(s,")");
				size = 3;
				break;
			case 0x06:
				strcpy(s,"LD\t(");
				sprintf(stemp,"%4.4Xh",ReadZ80Mem(adr+1)+(ReadZ80Mem(adr+2)<<8));strcat(s,stemp);
				strcat(s,"),A");
				size = 3;
				break;
			case 0x07:
				strcpy(s,"LD\tA,(");
				sprintf(stemp,"%4.4Xh",ReadZ80Mem(adr+1)+(ReadZ80Mem(adr+2)<<8));strcat(s,stemp);
				strcat(s,")");
				size = 3;
				break;
			}
			break;
		case 0x03:
			if(a & 0x08)
				strcpy(s,"DEC\t");
			else
				strcpy(s,"INC\t");
			strcat(s,dreg[d >> 1]);
			break;
		case 0x04:
			strcpy(s,"INC\t");
			strcat(s,reg[d]);
			break;
		case 0x05:
			strcpy(s,"DEC\t");
			strcat(s,reg[d]);
			break;
		case 0x06:				// LD	d,n
			strcpy(s,"LD\t");
			strcat(s,reg[d]);
			strcat(s,",");
			sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
			size = 2;
			break;
		case 0x07:
			{
			static const char* const str[8] = {"RLCA","RRCA","RLA","RRA","DAA","CPL","SCF","CCF"};
			strcpy(s,str[d]);
			}
			break;
		}
		break;
	case 0x40:							// LD	d,s
		if(d == e) {
			strcpy(s,"HALT");
		} else {
			strcpy(s,"LD\t");
			strcat(s,reg[d]);
			strcat(s,",");
			strcat(s,reg[e]);
		}
		break;
	case 0x80:
		strcpy(s,arith[d]);
		strcat(s,reg[e]);
		break;
	case 0xC0:
		switch(e) {
		case 0x00:
			strcpy(s,"RET\t");
			strcat(s,cond[d]);
			break;
		case 0x01:
			if(d & 1) {
				switch(d >> 1) {
				case 0x00:
					strcpy(s,"RET");
					break;
				case 0x01:
					strcpy(s,"EXX");
					break;
				case 0x02:
					strcpy(s,"JP\t(HL)");
					break;
				case 0x03:
					strcpy(s,"LD\tSP,HL");
					break;
				}
			} else {
				strcpy(s,"POP\t");
				if((d >> 1)==3)
					strcat(s,"AF");
				else
					strcat(s,dreg[d >> 1]);
			}
			break;
		case 0x02:
			strcpy(s,"JP\t");
			strcat(s,cond[d]);
			strcat(s,",");
			sprintf(stemp,"%4.4Xh",ReadZ80Mem(adr+1)+(ReadZ80Mem(adr+2)<<8));strcat(s,stemp);
			size = 3;
			break;
		case 0x03:
			switch(d) {
			case 0x00:
				strcpy(s,"JP\t");
				sprintf(stemp,"%4.4Xh",ReadZ80Mem(adr+1)+(ReadZ80Mem(adr+2)<<8));strcat(s,stemp);
				size = 3;
				break;
			case 0x01:					// 0xCB
				a = ReadZ80Mem(++adr);		// Erweiterungsopcode holen
				d = (a >> 3) & 7;
				e = a & 7;
				stemp[1] = 0;			// temp.String = 1 Zeichen
				switch(a & 0xC0) {
				case 0x00:
					{
					static const char* const str[8] = {"RLC","RRC","RL","RR","SLA","SRA","???","SRL"};
					strcpy(s,str[d]);
					}
					strcat(s,"\t");
					strcat(s,reg[e]);
					break;
				case 0x40:
					strcpy(s,"BIT\t");
					stemp[0] = d+'0';strcat(s,stemp);
					strcat(s,",");
					strcat(s,reg[e]);
					break;
				case 0x80:
					strcpy(s,"RES\t");
					stemp[0] = d+'0';strcat(s,stemp);
					strcat(s,",");
					strcat(s,reg[e]);
					break;
				case 0xC0:
					strcpy(s,"SET\t");
					stemp[0] = d+'0';strcat(s,stemp);
					strcat(s,",");
					strcat(s,reg[e]);
					break;
				}
				size = 2;
				break;
			case 0x02:
				strcpy(s,"OUT\t(");
				sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
				strcat(s,"),A");
				size = 2;
				break;
			case 0x03:
				strcpy(s,"IN\tA,(");
				sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
				strcat(s,")");
				size = 2;
				break;
			case 0x04:
				strcpy(s,"EX\t(SP),HL");
				break;
			case 0x05:
				strcpy(s,"EX\tDE,HL");
				break;
			case 0x06:
				strcpy(s,"DI");
				break;
			case 0x07:
				strcpy(s,"EI");
				break;
			}
			break;
		case 0x04:
			strcpy(s,"CALL\t");
			strcat(s,cond[d]);
			strcat(s,",");
			sprintf(stemp,"%4.4Xh",ReadZ80Mem(adr+1)+(ReadZ80Mem(adr+2)<<8));strcat(s,stemp);
			size = 3;
			break;
		case 0x05:
			if(d & 1) {
				switch(d >> 1) {
				case 0x00:
					strcpy(s,"CALL\t");
					sprintf(stemp,"%4.4Xh",ReadZ80Mem(adr+1)+(ReadZ80Mem(adr+2)<<8));strcat(s,stemp);
					size = 3;
					break;
				case 0x02:				// 0xED
					a = ReadZ80Mem(++adr);	// Erweiterungsopcode holen
					d = (a >> 3) & 7;
					e = a & 7;
					size = 2;
					switch(a & 0xC0) {
					case 0x40:
						switch(e) {
						case 0x00:
							strcpy(s,"IN\t");
							strcat(s,reg[d]);
							strcat(s,",(C)");
							break;
						case 0x01:
							strcpy(s,"OUT\t(C),");
							strcat(s,reg[d]);
							break;
						case 0x02:
							if(d & 1)
								strcpy(s,"ADC");
							else
								strcpy(s,"SBC");
							strcat(s,"\tHL,");
							strcat(s,dreg[d >> 1]);
							break;
						case 0x03:
							if(d & 1) {
								strcpy(s,"LD\t");
								strcat(s,dreg[d >> 1]);
								strcat(s,",(");
								sprintf(stemp,"%4.4Xh",ReadZ80Mem(adr+1)+(ReadZ80Mem(adr+2)<<8));strcat(s,stemp);
								strcat(s,")");
								size += 2;
							} else {
								strcpy(s,"LD\t(");
								sprintf(stemp,"%4.4Xh",ReadZ80Mem(adr+1)+(ReadZ80Mem(adr+2)<<8));strcat(s,stemp);
								strcat(s,"),");
								strcat(s,dreg[d >> 1]);
								size += 2;
							}
							break;
						case 0x04:
							{
							static const char* const str[8] = {"NEG","???","???","???","???","???","???","???"};
							strcpy(s,str[d]);
							}
							break;
						case 0x05:
							{
							static const char* const str[8] = {"RETN","RETI","???","???","???","???","???","???"};
							strcpy(s,str[d]);
							}
							break;
						case 0x06:
							strcpy(s,"IM\t");
							stemp[0] = d + '0' - 1; stemp[1] = 0;
							strcat(s,stemp);
							break;
						case 0x07:
							{
							static const char* const str[8] = {"LD\tI,A","???","LD\tA,I","???","RRD","RLD","???","???"};
							strcpy(s,str[d]);
							}
							break;
						}
						break;
					case 0x80:
						{
							static const char* const str[32] = {
								"LDI", "CPI", "INI", "OUTI", "???", "???", "???", "???",
								"LDD", "CPD", "IND", "OUTD", "???", "???", "???", "???",
								"LDIR", "CPIR", "INIR", "OTIR", "???", "???", "???", "???",
								"LDDR", "CPDR", "INDR", "OTDR", "???", "???", "???", "???"
							};
							strcpy(s,str[a & 0x1F]);
						}
						break;
					}
					break;
				default:				// 0x01 (0xDD) = IX, 0x03 (0xFD) = IY
					strcpy(ireg,(a & 0x20)?"IY":"IX");
					a = ReadZ80Mem(++adr);	// Erweiterungsopcode holen
					size = 2;
					switch(a) {
					case 0x09:
						strcpy(s,"ADD\t");
						strcat(s,ireg);
						strcat(s,",BC");
						break;
					case 0x19:
						strcpy(s,"ADD\t");
						strcat(s,ireg);
						strcat(s,",DE");
						break;
					case 0x21:
						strcpy(s,"LD\t");
						strcat(s,ireg);
						strcat(s,",");
						sprintf(stemp,"%4.4Xh",ReadZ80Mem(adr+1)+(ReadZ80Mem(adr+2)<<8));strcat(s,stemp);
						size += 2;
						break;
					case 0x22:
						strcpy(s,"LD\t(");
						sprintf(stemp,"%4.4Xh",ReadZ80Mem(adr+1)+(ReadZ80Mem(adr+2)<<8));strcat(s,stemp);
						strcat(s,"),");
						strcat(s,ireg);
						size += 2;
						break;
					case 0x23:
						strcpy(s,"INC\t");
						strcat(s,ireg);
						break;
					case 0x29:
						strcpy(s,"ADD\t");
						strcat(s,ireg);
						strcat(s,",");
						strcat(s,ireg);
						break;
					case 0x2A:
						strcpy(s,"LD\t");
						strcat(s,ireg);
						strcat(s,",(");
						sprintf(stemp,"%4.4Xh",ReadZ80Mem(adr+1)+(ReadZ80Mem(adr+2)<<8));strcat(s,stemp);
						strcat(s,")");
						size += 2;
						break;
					case 0x2B:
						strcpy(s,"DEC\t");
						strcat(s,ireg);
						break;
					case 0x34:
						strcpy(s,"INC\t(");
						strcat(s,ireg);
						strcat(s,"+");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
						strcat(s,")");
						size += 1;
						break;
					case 0x35:
						strcpy(s,"DEC\t(");
						strcat(s,ireg);
						strcat(s,"+");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
						strcat(s,")");
						size += 1;
						break;
					case 0x36:
						strcpy(s,"LD\t(");
						strcat(s,ireg);
						strcat(s,"+");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
						strcat(s,"),");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+2));strcat(s,stemp);
						size += 2;
						break;
					case 0x39:
						strcpy(s,"ADD\t");
						strcat(s,ireg);
						strcat(s,",SP");
						break;
					case 0x46:
					case 0x4E:
					case 0x56:
					case 0x5E:
					case 0x66:
					case 0x6E:
						strcpy(s,"LD\t");
						strcat(s,reg[(a>>3)&7]);
						strcat(s,",(");
						strcat(s,ireg);
						strcat(s,"+");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
						strcat(s,")");
						size += 1;
						break;
					case 0x70:
					case 0x71:
					case 0x72:
					case 0x73:
					case 0x74:
					case 0x75:
					case 0x77:
						strcpy(s,"LD\t(");
						strcat(s,ireg);
						strcat(s,"+");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
						strcat(s,"),");
						strcat(s,reg[a & 7]);
						size += 1;
						break;
					case 0x7E:
						strcpy(s,"LD\tA,(");
						strcat(s,ireg);
						strcat(s,"+");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
						strcat(s,")");
						size += 1;
						break;
					case 0x86:
						strcpy(s,"ADD\tA,(");
						strcat(s,ireg);
						strcat(s,"+");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
						strcat(s,")");
						size += 1;
						break;
					case 0x8E:
						strcpy(s,"ADC\tA,(");
						strcat(s,ireg);
						strcat(s,"+");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
						strcat(s,")");
						size += 1;
						break;
					case 0x96:
						strcpy(s,"SUB\t(");
						strcat(s,ireg);
						strcat(s,"+");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
						strcat(s,")");
						size += 1;
						break;
					case 0x9E:
						strcpy(s,"SBC\tA,(");
						strcat(s,ireg);
						strcat(s,"+");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
						strcat(s,")");
						size += 1;
						break;
					case 0xA6:
						strcpy(s,"AND\tA,(");
						strcat(s,ireg);
						strcat(s,"+");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
						strcat(s,")");
						size += 1;
						break;
					case 0xAE:
						strcpy(s,"XOR\tA,(");
						strcat(s,ireg);
						strcat(s,"+");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
						strcat(s,")");
						size += 1;
						break;
					case 0xB6:
						strcpy(s,"OR\tA,(");
						strcat(s,ireg);
						strcat(s,"+");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
						strcat(s,")");
						size += 1;
						break;
					case 0xBE:
						strcpy(s,"CP\tA,(");
						strcat(s,ireg);
						strcat(s,"+");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
						strcat(s,")");
						size += 1;
						break;
					case 0xE1:
						strcpy(s,"POP\t");
						strcat(s,ireg);
						break;
					case 0xE3:
						strcpy(s,"EX\t(SP),");
						strcat(s,ireg);
						break;
					case 0xE5:
						strcpy(s,"PUSH\t");
						strcat(s,ireg);
						break;
					case 0xE9:
						strcpy(s,"JP\t(");
						strcat(s,ireg);
						strcat(s,")");
						break;
					case 0xF9:
						strcpy(s,"LD\tSP,");
						strcat(s,ireg);
						break;
					case 0xCB:
						a = ReadZ80Mem(adr+2);	// weiteren Unteropcode
						d = (a >> 3) & 7;
						stemp[1] = 0;
						switch(a & 0xC0) {
						case 0x00:
							{
								static const char* const str[8] = {
									"RLC", "RRC", "RL", "RR", "SLA", "SRA", "???", "SRL"
								};
								strcpy(s,str[d]);
							}
							strcat(s,"\t");
							break;
						case 0x40:
							strcpy(s,"BIT\t");
							stemp[0] = d + '0';
							strcat(s,stemp);
							strcat(s,",");
							break;
						case 0x80:
							strcpy(s,"RES\t");
							stemp[0] = d + '0';
							strcat(s,stemp);
							strcat(s,",");
							break;
						case 0xC0:
							strcpy(s,"SET\t");
							stemp[0] = d + '0';
							strcat(s,stemp);
							strcat(s,",");
							break;
						}
						strcat(s,"(");
						strcat(s,ireg);
						strcat(s,"+");
						sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
						strcat(s,")");
						size += 2;
						break;
					}
					break;
				}
			} else {
				strcpy(s,"PUSH\t");
				if((d >> 1)==3)
					strcat(s,"AF");
				else
					strcat(s,dreg[d >> 1]);
			}
			break;
		case 0x06:
			strcpy(s,arith[d]);
			sprintf(stemp,"%2.2Xh",ReadZ80Mem(adr+1));strcat(s,stemp);
			size += 1;
			break;
		case 0x07:
			strcpy(s,"RST\t");
			sprintf(stemp,"%2.2Xh",a & 0x38);strcat(s,stemp);
			break;
		}
		break;
	}

	return size;
}
