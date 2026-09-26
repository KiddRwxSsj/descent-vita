/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.  
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/
/*
 * $Source: f:/miner/source/main/rcs/text.c $
 * $Revision: 2.0 $
 * $Author: john $
 * $Date: 1995/02/27 11:33:09 $
 * 
 * Code for localizable text
 * 
 * $Log: text.c $
 * Revision 2.0  1995/02/27  11:33:09  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.11  1994/12/14  12:53:23  matt
 * Improved error handling
 * 
 * Revision 1.10  1994/12/09  18:36:44  john
 * Added code to make text read from hogfile.
 * 
 * Revision 1.9  1994/12/08  20:56:34  john
 * More cfile stuff.
 * 
 * Revision 1.8  1994/12/08  17:20:06  yuan
 * Cfiling stuff.
 * 
 * Revision 1.7  1994/12/05  15:10:36  allender
 * support encoded descent.tex file (descent.txb)
 * 
 * Revision 1.6  1994/12/01  14:18:34  matt
 * Now support backslash chars in descent.tex file
 * 
 * Revision 1.5  1994/10/27  00:13:10  john
 * Took out cfile.
 * 
 * Revision 1.4  1994/07/11  15:33:49  matt
 * Put in command-line switch to load different text files
 * 
 * Revision 1.3  1994/07/10  09:56:25  yuan
 * #include <stdio.h> added for FILE type.
 * 
 * Revision 1.2  1994/07/09  22:48:14  matt
 * Added localizable text
 * 
 * Revision 1.1  1994/07/09  21:30:46  matt
 * Initial revision
 * 
 * 
 */


#pragma off (unreferenced)
static char rcsid[] = "$Id: text.c 2.0 1995/02/27 11:33:09 john Exp $";
#pragma on (unreferenced)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>

//#include "cfile.h"
#include "cfile.h"
#include "mem.h"
#include "error.h"

#include "inferno.h"
#include "text.h"
#include "args.h"
#include "compbit.h"

char *text;

char *Text_string[N_TEXT_STRINGS];

#ifdef SHAREWARE
#define FIRST_FALLBACK_STRING	514

static char *fallback_strings[N_TEXT_STRINGS - FIRST_FALLBACK_STRING] = {
	"done",
	"I am a",
	"CHEATER!",
	"Loading Data",
	"Alt-F2\t  Save Game",
	"Alt-F3\t  Load Game",
	"Only available in the registered version!",
	"Concussion",
	"Homing",
	"ProxBomb",
	"Smart",
	"Mega",
	"The mission %s needed to play\nthis demo could not be found.",
	"Show IDs on HUD",
	"That name is already in use.",
	"This mission can only be\nplayed in anarchy modes.",
	"Start without waiting",
	"Quit now?",
	"Mission not found.",
	"Select mission",
	"Error loading mission.",
	"Custom settings...",
	"Base address",
	"IRQ",
	"Reset to defaults",
	"Valid IRQ values are 2-7",
	"No UART detected at that address.",
	"Why can't we all just get along?",
	"Hey, I got a present for ya",
	"I got a hold of you!",
	"Take that!",
	"Mission: ",
	"Viewing distance",
	"Alternate Exit Found!\n\nProceeding to Secret Level!",
	"Show all players on automap",
	"killed by a robot",
	"Baud",
	"Consistency error.",
	"Press any key to continue",
	"Not enough space on the current\ndevice to save the demo.",
	"Reactor is invulnerable for",
	"That level is not available.",
	"Reactor life",
	"min",
	"Current IPX socket is",
	"This program requires MS-DOS",
	"or later.",
	"Not enough file handles!",
	"of the required file handles are available.",
	"Increase the FILES= setting",
	"in your CONFIG.SYS file.",
	"Available memory",
	"more bytes of DOS memory are needed.",
	"more bytes of virtual memory are needed.",
	"more bytes of extended memory are needed.",
	"Free some memory and try again.",
	"more bytes of physical memory are needed.",
	"Reconfigure your virtual memory.",
	"Initializing DPMI services",
	"Initializing critical error handler",
	"Enables i-glasses! head-mounted display",
	"Enables i-glasses! head tracking via COM port",
	"Enables Kasan's 3dMax stereo display (low res)",
	"in 320x200 interlaced mode",
	"Enables Kasan's 3dMax stereo display (high res)",
	"Press any key to continue...",
	"Use dynamic IPX socket changes",
	"Skip file handle checking",
	"Reading configuration file...",
	"Initializing timer system...",
	"Initializing keyboard...",
	"Initializing mouse...",
	"Mouse support disabled.",
	"Initializing joystick...",
	"Using slow joystick reading...",
	"Using polled joystick reading...",
	"Using BIOS joystick reading...",
	"Joystick support disabled.",
	"Initializing divide by zero handler...",
	"Initializing network",
	"Using IPX socket",
	"No IPX network found.",
	"Error opening IPX socket",
	"Not enough low memory for IPX buffers.",
	"Error initializing IPX.  Error code:",
	"Network disabled.",
	"Initializing graphics system...",
	"Error opening sound file",
	"Error locking sound file",
	"Sound error:",
	"Error locking drum file",
	"Error locking MIDI track map",
	"Error locking MIDI callback function",
	"External control",
	"Error opening COM port for i-glasses",
	"Initializing i-glasses tracking on COM port",
	"i-glasses head tracking enabled.",
	"Press ESC to abort",
	"Serial port failure",
	"Message",
	"Macro",
	"Error locking serial interrupt",
	"handler.",
	"Robot movement normal",
	"Robots move fast and fire continuously!",
	"Robot painting OFF",
	"Robot painting with texture %d",
};
#endif

free_text()
{
	free(text);
}

// rotates a byte left one bit, preserving the bit falling off the right
void
encode_rotate_left(char *c)
{
	int found;

	found = 0;
	if (*c & 0x80)
		found = 1;
	*c = *c << 1;
	if (found)
		*c |= 0x01;
}

//load all the text strings for Descent
void load_text()
{
	CFILE  *tfile;
	CFILE *ifile;
	int len,i, have_binary = 0;
	char *tptr;
	char *filename="descent.tex";

	if ((i=FindArg("-text"))!=0)
		filename = Args[i+1];

	if ((tfile = cfopen(filename,"rb")) == NULL) {
		filename="descent.txb";
		if ((ifile = cfopen(filename, "rb")) == NULL)
			Error("Cannot open file DESCENT.TEX or DESCENT.TXB");
		have_binary = 1;

		len = cfilelength(ifile);

		//MALLOC(text,char,len);//Won't compile... working on it..-KRB
		text=malloc((len+1)*sizeof(char));//my hack -KRB
		atexit(free_text);

		cfread(text,1,len,ifile);
		text[len] = 0;

		cfclose(ifile);

	} else {
		int c;
		char * p;

		len = cfilelength(tfile);

		//MALLOC(text,char,len);//Won't compile... working on it..-KRB
		text=malloc((len+2)*sizeof(char));//my hack -KRB

		atexit(free_text);

		//fread(text,1,len,tfile);
		p = text;
		do {
			c = cfgetc( tfile );
			if ( c != 13 )
				*p++ = c;
		} while ( c!=EOF );
		*p = 0;

		cfclose(tfile);
	}

	for (i=0,tptr=text;i<N_TEXT_STRINGS;i++) {
		char *p;

		Text_string[i] = tptr;

		tptr = strchr(tptr,'\n');

		if (!tptr) {
			#ifdef SHAREWARE
			// the 1.4 shareware file stops at 514 strings, the rest came later
			if (i >= FIRST_FALLBACK_STRING) {
				for (; i<N_TEXT_STRINGS; i++)
					Text_string[i] = fallback_strings[i - FIRST_FALLBACK_STRING];
				break;
			}
			#endif
			Error("Not enough strings in text file - expecting %d, found %d",N_TEXT_STRINGS,i);
		}

		if ( tptr ) *tptr++ = 0;

		if (have_binary) {
			for (p=Text_string[i]; p < tptr - 1; p++) {
				encode_rotate_left(p);
				*p = *p ^ BITMAP_TBL_XOR;
				encode_rotate_left(p);
			}
		}

		//scan for special chars (like \n)
		for (p=Text_string[i];p=strchr(p,'\\');) {
			char newchar;

			if (p[1] == 'n') newchar = '\n';
			else if (p[1] == 't') newchar = '\t';
			else if (p[1] == '\\') newchar = '\\';
			else
				Error("Unsupported key sequence <\\%c> on line %d of file <%s>",p[1],i+1,filename); 

			p[0] = newchar;
			strcpy(p+1,p+2);
			p++;
		}
 
	}

//	Assert(tptr==text+len || tptr==text+len-2);
	
}


