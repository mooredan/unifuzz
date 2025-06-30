#     unifuzz SQLite loadable extension (Makefile) 
#
#     Copyright (C) 2017  Daniel Moore
# 
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
# 
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
# 
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see [http://www.gnu.org/licenses/].
# 
#     Daniel Moore 
#     Tigard, Oregon, USA
#     email: mooredan@suncup.net

ARCH ?= 64

all : locale.o

locale.o : locale.c
	clang -m$(ARCH) -c -o locale.o locale.c \
	      -I. \
              -I../../../wine/include \
              -D__WINESRC__ \
              -D_KERNEL32_ \
              -D_NORMALIZE_ \
              -D_REENTRANT \
              -fPIC \
              -pipe \
              -fno-strict-aliasing \
              -Wdeclaration-after-statement \
              -Wempty-body -Wignored-qualifiers -Wstrict-prototypes \
              -Wtype-limits -Wvla -Wwrite-strings \
              -Wpointer-arith \
              -Wno-pragma-pack \
              -gdwarf-2 -gstrict-dwarf \
              -fno-omit-frame-pointer -std=gnu89 \
              -g -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0


.PHONY: clean
clean :
	@ rm -f locale.o
