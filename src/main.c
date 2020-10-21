/*
Copyright (c) 2017 FBLabs

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "conio.h"
#include "strings.h"
#include "mem.h"
#include "bios.h"
#include "msxdos.h"
#include "mapper.h"
#include "interface.h"


/* Structs */

/* Constants */
const unsigned char *EXPTBL = (volatile unsigned char *)0xFCC1;	// slots expanded or not
const unsigned char *DSKSLT = (volatile unsigned char *)0xF348;	// slotid diskrom


/* Global vars */
unsigned char *HTIMI = (volatile unsigned char *)0xFD9F;

static TDevInfo devInfo;
static unsigned char numMprPages, mprSegments[8], curSegm;
static unsigned char hooks, pfi, autodetect, onlyErase, resetAtEnd;
static unsigned char buffer[64], pause;
static unsigned char c, t1, t2, slot, swId, isMain, isSlave, sizeRom;
static int fhandle, i, r;
static unsigned long fileSize;


/******************************************************************************/
static void restoreHooks() {
	// Restore hook
	*HTIMI = hooks;
}

/******************************************************************************/
static void giveAPause() {
	if (pause == 1) {
		puts(pauseMsg);
		getchar();
		puts(crlf);
	}
}

/******************************************************************************/
int main(char** argv, int argc) {
	puts(title1);
	puts(title2);

	if (argc < 1) {
showUsage:
		puts(usage1);
		puts(usage2);
		puts(usage3);
		return 1;
	}
	pfi = 0;
	onlyErase = 0;
	autodetect = 0;
	pause = 1;
	for (i = 0; i < argc; i++) {
		if (argv[i][0] == '/') {
			if (argv[i][1] == 'h' || argv[i][1] == 'H') {
				puts(usage1);
				puts(usage2);
				puts(usage3);
				return 0;
			} else if (argv[i][1] == 'e' || argv[i][1] == 'E') {
				onlyErase = 1;
				++pfi;
			} else if (argv[i][1] == 'a' || argv[i][1] == 'A') {
				autodetect = 1;
				++pfi;
			} else {
				goto showUsage;
			}
		}
	}
	if (pfi == argc && onlyErase == 0) {
		goto showUsage;
	}

	// Save hook
	hooks = *HTIMI;
	// Temporary disable hooks
	*HTIMI = 0xC9;

	if (autodetect == 0) {
		puts(whatslot);
		while(1) {
			c = getchar();
			if (c >= '0' && c <= '3') {
				break;
			}
		}
		putchar(c);
		puts(crlf);
		slot = c - '0';
		if ((*(EXPTBL+slot) & 0x80) == 0x80) {
			puts(whatsubslot);
			while(1) {
				c = getchar();
				if (c >= '0' && c <= '3') {
					break;
				}
			}
			putchar(c);
			puts(crlf);
			c -= '0';
			slot |= 0x80 | (c << 2);
		}
		if (detectInterface(slot) == 0) {
			slot = 0xFF;
		}
		puts(crlf);
	} else {
		// Find interface
		puts(searching);
		slot = (*EXPTBL) & 0x80;
		while (1) {
			if (slot == 0x8F || slot == 0x03) {
				slot = 0xFF;
				break;
			}
			if (detectInterface(slot) == 1) {
				puts(found3);
				putdec8(slot & 0x03);
				if ((slot & 0x80) == 0x80) {
					putchar('.');
					putdec8((slot & 0x0C) >> 2);
				}
				puts(crlf);
				break;
			}
			// Next slot
			if (slot & 0x80) {
				if ((slot & 0x0C) != 0x0C) {
					slot += 0x04;
					continue;
				}
			}
			slot = (slot & 0x03) + 1;
			slot |= (*(EXPTBL+slot)) & 0x80;
		}
	}

	if (slot == 0xFF) {
		restoreHooks();
		puts(notfound);
		return 4;
	}

	// Detects MSXDOS version
	msxdos_init();

	/* If is MSXDOS1, we can not verify which is the main DOS, we assume it is. */
	if (dosversion < 2 || *DSKSLT == slot) {
		isMain = 1;
	} else {
		isMain = 0;
	}

	isSlave = 0;
	if (dosversion == 0x82) {			// Is Nextor, check devices
		for (c = 0; c < 16; c++) {
			if (0 == getDeviceInfo(c, &devInfo)) {
				if (devInfo.slotNum == slot) {
					isSlave = 1;
				}
			}
		}
	} else {
		isSlave = 1;					// Forces a reset
	}

	if (onlyErase == 1) {
		if (isMain == 1 || isSlave == 1) {
			puts(confirmReset0);
			puts(confirmReset1);
			puts(confirmReset3);
			clearKeyBuf();
			c = getchar();
			putchar(c);
			puts(crlf);
			pause = 0;
			if (c == 'y' || c == 'Y') {
				__asm__("di");
				eraseFlash(slot);
				resetSystem();
			}
			restoreHooks();
			return 0;
		} else {
			giveAPause();
			eraseFlash(slot);
			restoreHooks();
			return 0;
		}
	}

	c = mpInit();
	if (c != 0) {
		puts(errorNoExtBios);
		numMprPages = numMapperPages();
	} else {
		numMprPages = mpVars->numFree;
	}
	if (numMprPages < getNumMemPages()) {
		puts(noMemAvailable);
		restoreHooks();
		return 3;
	}
	// Saves the current segment;
	curSegm = getCurSegFrame1();

	// Try open file
	fhandle = open(argv[pfi], O_RDONLY);
	if (fhandle == -1) {
		puts(openingError);
		restoreHooks();
		return 4;
	}

	if (dosversion < 2) {
		fileSize = dos1GetFilesize();
	} else {
		fileSize = lseek(fhandle, 0, SEEK_END);
	}
	sizeRom = getRomSize(fileSize);
	if (sizeRom == 0) {
		puts(filesizeError);
		restoreHooks();
		return 5;
	}
	/* Only for MSXDOS2 */
	if (dosversion > 1) {
		c = verifySwId(fhandle, buffer);
		if (c == 1) {
			goto readErr;
		} else if (c == 2) {
			puts(errorWrongDrv);
			restoreHooks();
			return 7;
		} else if (c == 3) {
			puts(errorNotNxtDrv);
			restoreHooks();
			return 7;
		} else if (c == 4) {
			puts(errorSeek);
			restoreHooks();
			return 7;
		}
	}

	for (i = 0; i < sizeRom; i++) {
		mprSegments[i] = allocUserSegment();
		if (mprSegments[i] == 0) {
			puts(errorAllocMapper);
			close(fhandle);
			restoreHooks();
			return 10;
		}
	}
	puts(readingFile);
	c = 0;
	for (i = 0; i < sizeRom; i++) {
		putchar(ce[c]);
		putchar(8);
		c = (c + 1) & 0x03;
		t1 = mprSegments[i];
		putSegFrame1(t1);
		r = read(fhandle, (void *)0x4000, 16384);
		putSegFrame1(curSegm);
		if (r != 16384) {
readErr:
			puts(readingError0);
			puthex8(last_error);
			puts(readingError1);
			close(fhandle);
			restoreHooks();
			return 11;
		}
	}
	puts(ok0);
	close(fhandle);

	resetAtEnd = 0;
	if (isMain == 1 || isSlave == 1) {
		puts(confirmReset0);
		puts(confirmReset2);
		puts(confirmReset3);
		clearKeyBuf();
		c = getchar();
		putchar(c);
		puts(crlf);
		pause = 0;
		if (c == 'y' || c == 'Y') {
			resetAtEnd = 1;
		} else {
			close(fhandle);
			restoreHooks();
			return 0;
		}
	}

	giveAPause();

	__asm__("di");
	if (getAutoErase() == 0) {
		eraseFlash(slot);
	}

	puts(writingFlash);
	for (i = 0; i < sizeRom; i++) {
		if (writeBlock(slot, mprSegments[i], curSegm, i) == 0) {
			break;
		}
	}
	__asm__("ei");
	if (i != sizeRom) {
		puts(errorWriting);
		eraseFlash(slot);
		puts(systemHalted);
		__asm__("di");
		__asm__("halt");
	} else {
		putchar(' ');
		puts(ok0);
	}
	if (resetAtEnd == 1) {
		puts(anyKeyToReset);
		clearKeyBuf();
		getchar();
		resetSystem();
	}
	restoreHooks();
	return 0;
}
