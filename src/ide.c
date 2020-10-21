/*
Copyright (c) 2017-2019 FBLabs

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
#include "mapper.h"
#include "interface.h"

/* Constants */
const char *title1 =			"FBLabs IDE programmer utility\r\n";
const char *usage2 =			"     ide-upd /opts <filename.ext>\r\n"
								"Example: ide-upd /a DRIVER.ROM\r\n"
								"         ide-upd /e\r\n";
static const char *found1 =		"Found ";
static const char *foundide =	"IDE 128KB";
static const char *foundciel =	"IDE 512KB";
static const char *found2 =		" interface";
static unsigned char banks[8] = { 0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0 };

/* Variables */
static int i;
static unsigned char c, t1, t2;
static unsigned char flashIdMan, flashIdProd, alg, autoErase;
static unsigned char hwId, swId, mySlot;
static unsigned char *source, *dest, *sourceChk;

/* Private Functions */

/******************************************************************************/
static unsigned char flashIdent(unsigned char manId, unsigned char prodId)
{
	autoErase = 0;
	alg = ALGBYTE;
	if (manId == 0x01) {				// AMD
		if (prodId == 0x20) {			// AM29F010
			return 1;
		} else if (prodId == 0xA4) {	// AM29F040
			return 2;
		}
	} else if (manId == 0x1F) {			// Atmel
		if (prodId == 0xD5) {			// AT29C010 (page)
			autoErase = 1;
			alg = ALGPAGE;
			return 1;
		} else if (prodId == 0x17) {	// AT49F010
			return 1;
		} else if (prodId == 0x13) {	// AT49F040
			return 2;
		}
	} else if (manId == 0x37) {			// AMIC
		if (prodId == 0x86) {			// A29040B
			return 2;
		}
	} else if (manId == 0x52) {			// Alliance
		if (prodId == 0xA4) {			// AS29F040
			return 2;
		}
	} else if (manId == 0xBF) {			// SST
		if (prodId == 0x07) {			// SST29EE010 (page)
			autoErase = 1;
			alg = ALGPAGE;
			return 1;
		} else if (prodId == 0xB5) {	// SST39SF010A
			return 1;
		} else if (prodId == 0xB7) {	// SST39SF040
			return 2;
		}
	} else if (manId == 0xDA) {			// Winbond
		if (prodId == 0xA1) {			// W39F010
			return 1;
		}
	}
	return 0;
}

/******************************************************************************/
static void flashSendCmd(unsigned char cmd)
{
	poke(0x4104, 0x82);		// Bank 1, write enabled
	poke(0x9555, 0xAA);
	poke(0x4104, 0x02);		// Bank 0, write enabled
	poke(0xAAAA, 0x55);
	poke(0x4104, 0x82);		// Bank 1, write enabled
	poke(0x9555, cmd);
}

/******************************************************************************/
static void flashSectorEraseCmd(unsigned char bank)
{
	poke(0x4104, 0x82);		// Bank 1, write enabled
	poke(0x9555, 0xAA);
	poke(0x4104, 0x02);		// Bank 0, write enabled
	poke(0xAAAA, 0x55);
	if (bank == 0) {
		poke(0x4104, 0x02);	// Bank 0, write enabled
	} else {
		poke(0x4104, 0x22);	// Bank 3, write enabled
	}
	poke(0x8000, FLASHCMD_ERASESECTOR);
}

/******************************************************************************/
static unsigned char writeHalfBlock(int bank) {
	putSlotFrame1(mySlot);
	putSlotFrame2(mySlot);
	t1 = 0;
	source = (unsigned char *)0x2000;
	sourceChk = dest - 0x4000;
	c = banks[bank] | 0x02;
	while ((unsigned int)source < 0x4000) {
		flashSendCmd(FLASHCMD_WRITEBYTE);
		poke(0x4104, c);
		if (alg == ALGBYTE) {
			*dest = *source;			// write byte
		} else {
			for (i = 0; i < 127; i++) {	// write 128-byte
				*dest = *source;
				++dest;
				++source;
			}
			*dest = *source;
			sourceChk += 127;
		}
		i = 3800;
		while (--i != 0) {
			if (*sourceChk == *source) {	// if equal byte was written
				break;
			}
		}
		if (i == 0) {					// timeout
			t1 = 1;						// error
			goto exit;
		}
		++dest;
		++source;
		++sourceChk;
	}
exit:
	putRamFrame1();
	putRamFrame2();
	return t1;
}

/******************************************************************************/
static void waitErase(void)
{
	c = 0;
	t2 = 50;
	while (--t2 != 0) {
		__asm__("ei");
		__asm__("halt");
		__asm__("di");
		t1 = peek(0x8000);
		t2 = peek(0x8000);
		if (t1 == t2) {
			break;
		}
		putchar(ce[c]);
		putchar(8);
		c = (c + 1) & 0x03;
	}
}

/* Public Functions */

/******************************************************************************/
unsigned char detectInterface(unsigned char slot)
{
	putSlotFrame1(slot);
	putSlotFrame2(slot);
	flashSendCmd(FLASHCMD_SOFTRESET);
	flashSendCmd(FLASHCMD_SOFTIDENTRY);
	flashIdMan = peek(0x4000);
	flashIdProd = peek(0x4001);
	flashSendCmd(FLASHCMD_SOFTRESET);
	putRamFrame1();
	putRamFrame2();
	poke(0x4104, 0xC0);
	hwId = flashIdent(flashIdMan, flashIdProd);
	if (hwId == 1) {
		puts(found1);
		puts(foundide);
		puts(found2);
		return 1;
	} else if (hwId == 2) {
		puts(found1);
		puts(foundciel);
		puts(found2);
		return 1;
	}
	return 0;
}

/******************************************************************************/
unsigned char getNumMemPages(void) {
	return (hwId == 1) ? 4 : 8;
}

/******************************************************************************/
unsigned char getAutoErase(void) {
	return autoErase;
}

/******************************************************************************/
unsigned char getRomSize(unsigned long fileSize) {
	if (fileSize == 65536) {
		return 4;
	} else if (fileSize == 131072) {
		return 8;
	}
	return 0;
}

/******************************************************************************/
unsigned char verifySwId(int fhandle, unsigned char *buffer) {
	return 0;
}

/******************************************************************************/
void eraseFlash(unsigned char slot)
{
	puts(erasingFlash);
	putSlotFrame1(slot);
	putSlotFrame2(slot);
	if (hwId == 1) {
		flashSendCmd(FLASHCMD_ERASE);
		flashSendCmd(FLASHCMD_ERASEALL);
	} else {
		flashSendCmd(FLASHCMD_ERASE);
		flashSectorEraseCmd(0);
		poke(0x4104, 0x00);
		waitErase();
		flashSendCmd(FLASHCMD_ERASE);
		flashSectorEraseCmd(1);
	}
	poke(0x4104, 0x00);
	waitErase();
	flashSendCmd(FLASHCMD_SOFTRESET);
	poke(0x4104, 0x00);
	putRamFrame1();
	putRamFrame2();
	puts(ok0);
}

/******************************************************************************/
unsigned char writeBlock(unsigned char slot, unsigned char segment,
						 unsigned char curSegm, unsigned char bank)
{
	mySlot = slot;
	dest = (unsigned char *)0x8000;
	putSegFrame1(segment);
	__asm__("push hl");
	__asm__("push de");
	__asm__("push bc");
	__asm__("ld hl, #0x4000");
	__asm__("ld de, #0x2000");
	__asm__("ld bc, #0x2000");
	__asm__("ldir");
	__asm__("pop bc");
	__asm__("pop de");
	__asm__("pop hl");
	putSegFrame1(curSegm);
	if (writeHalfBlock(bank) != 0) {
		return 0;
	}
	putchar('*');
	putSegFrame1(segment);
	__asm__("push hl");
	__asm__("push de");
	__asm__("push bc");
	__asm__("ld hl, #0x6000");
	__asm__("ld de, #0x2000");
	__asm__("ld bc, #0x2000");
	__asm__("ldir");
	__asm__("pop bc");
	__asm__("pop de");
	__asm__("pop hl");
	putSegFrame1(curSegm);
	if (writeHalfBlock(bank) != 0) {
		return 0;
	}
	putchar('*');
	return 1;
}
