#ifndef _UEF_H
#define _UEF_H

#define UEFMode_Callback	0
#define UEFMode_Poll		1

typedef int UEFFILE;
__declspec( dllexport ) int uef_errno;

/* some defines related to the status byte - these may change! */

#define UEF_MMASK			(3 << 16)
#define UEF_STARTBIT		(2 << 8)
#define UEF_STOPBIT			(1 << 8)
#define UEF_BYTEMASK		0xff

/* some macros for reading parts of the status byte */

#define UEF_HTONE			(0 << 16)
#define UEF_DATA			(1 << 16)
#define UEF_GAP				(2 << 16)

#define UEFRES_TYPE(x)		(x&UEF_MMASK)
#define UEFRES_BYTE(x)		(x&UEF_BYTEMASK)
#define UEFRES_10BIT(x)		(((x&UEF_BYTEMASK) << 1) | ((x&UEF_STARTBIT) ? 1 : 0) | ((x&UEF_STOPBIT) ? 0x200 : 0))
#define UEFRES_STARTBIT(x)	(x&UEF_STARTBIT)
#define UEFRES_STOPBIT(x)	(x&UEF_STOPBIT)

/* some possible return states */
#define UEF_OK	0

#define UEF_SETMODE_INVALID	-1

#define UEF_OPEN_NOTUEF		-1
#define UEF_OPEN_NOTTAPE	-2
#define UEF_OPEN_NOFILE		-3
#define UEF_OPEN_MEMERR		-4

/* setup */
extern "C" __declspec( dllexport ) int uef_setmode(int mde);
extern "C" __declspec( dllexport ) void uef_setclock(int beats);
extern "C" __declspec( dllexport ) void uef_zeroclock(UEFFILE file);
__declspec( dllexport ) int uef_setcallback(void (* func)(UEFFILE file, int event));
__declspec( dllexport ) int uef_getlowesttime(UEFFILE file, int time);

/* poll mode */
extern "C" __declspec( dllexport ) int uef_getdata(UEFFILE file, int time);

/* callback mode */
__declspec( dllexport ) void uef_keepalive(UEFFILE file);

/* pause / unpause */
__declspec( dllexport ) void uef_pause(UEFFILE file);
__declspec( dllexport ) void uef_unpause(UEFFILE file);

/* open & close */
extern "C" __declspec( dllexport ) UEFFILE uef_open(char *name);
extern "C" __declspec( dllexport ) void uef_close(UEFFILE file);

/* overran count management */
__declspec( dllexport ) void uef_resetoverrun(UEFFILE file);
__declspec( dllexport ) int uef_getoverrun(UEFFILE file);

#endif