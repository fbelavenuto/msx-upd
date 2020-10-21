#
# Copyright (c) 2017 FBLabs
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY#  without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

CP = cp
RM = rm -f
MD = mkdir
CC = sdcc
AS = sdasz80
LD = sdcc
OC = objcopy --input-target=ihex --output-target=binary

SDIR = src
LDIR = ../msxclib/lib
IDIR = ../msxclib/inc
ODIR = obj

CFLAGS = -mz80 --opt-code-size --fomit-frame-pointer -I$(IDIR) -Iinc -I..
AFLAGS = -I$(IDIR) -Iinc -I..
LDFLAGS = -mz80 --code-loc 0x0180 --data-loc 0 --no-std-crt0

_OBJS1 = crt0.rel bios.rel msxdos.rel getchar.rel putchar.rel conio.rel mem.rel mapper.rel strings.rel main.rel sdxc.rel
OBJS1 = $(patsubst %,$(ODIR)/%,$(_OBJS1))
_OBJS2 = crt0.rel bios.rel msxdos.rel getchar.rel putchar.rel conio.rel mem.rel mapper.rel strings.rel main.rel sdmapper.rel
OBJS2 = $(patsubst %,$(ODIR)/%,$(_OBJS2))
_OBJS3 = crt0.rel bios.rel msxdos.rel getchar.rel putchar.rel conio.rel mem.rel mapper.rel strings.rel main.rel ide.rel
OBJS3 = $(patsubst %,$(ODIR)/%,$(_OBJS3))

FBL: $(ODIR) FBL-UPD.COM
SDM: $(ODIR) SDM-UPD.COM
IDE: $(ODIR) IDE-UPD.COM

all: FBL SDM IDE

FBL-UPD.COM: FBL-UPD.ihx
FBL-UPD.ihx: $(OBJS1)
	$(LD) $(LDFLAGS) -o $@ $(OBJS1)

SDM-UPD.COM: SDM-UPD.ihx
SDM-UPD.ihx: $(OBJS2)
	$(LD) $(LDFLAGS) -o $@ $(OBJS2)

IDE-UPD.COM: IDE-UPD.ihx
IDE-UPD.ihx: $(OBJS3)
	$(LD) $(LDFLAGS) -o $@ $(OBJS3)

.PHONY: clean

clean:
	$(RM) $(ODIR)/* *.map *.lk *.noi *.com *.ihx

$(ODIR):
	$(MD) $(ODIR)

$(ODIR)/%.rel: $(SDIR)/%.s
	$(AS) $(AFLAGS) -o $@ $<

$(ODIR)/%.rel: $(SDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(ODIR)/%.rel: $(LDIR)/%.s
	$(AS) $(AFLAGS) -o $@ $<

$(ODIR)/%.rel: $(LDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.COM: %.ihx
	$(OC) $< $@
