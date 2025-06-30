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

all : collation.o sortkey.o wctype.o

sortkey.o : sortkey.c
	clang -m$(ARCH) -c -o sortkey.o sortkey.c \
              -I../../../wine/include \
              -D__WINESRC__ \
              -DWINE_UNICODE_API="" -D_REENTRANT -fPIC \
              -pipe -fno-strict-aliasing \
              -Wdeclaration-after-statement  -Wempty-body \
              -Wignored-qualifiers  -Wstrict-prototypes \
              -Wtype-limits  -Wvla \
              -Wwrite-strings  -Wpointer-arith \
              -Wno-pragma-pack \
              -gdwarf-2 -gstrict-dwarf \
              -fno-omit-frame-pointer -std=gnu89 \
              -g -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0

collation.o : collation.c
	clang -m$(ARCH) -c -o collation.o collation.c \
              -D__WINESRC__ -DWINE_UNICODE_API="" -D_REENTRANT \
              -fPIC -pipe -fno-strict-aliasing -Wdeclaration-after-statement -Wempty-body \
              -Wignored-qualifiers -Wstrict-prototypes -Wtype-limits -Wvla -Wwrite-strings -Wpointer-arith \
              -gdwarf-2 -gstrict-dwarf -fno-omit-frame-pointer -std=gnu89 -g -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0


wctype.o : wctype.c
	clang -m$(ARCH) -c -o wctype.o wctype.c \
              -I../../../wine/include \
              -D__WINESRC__ -DWINE_UNICODE_API="" -D_REENTRANT -fPIC \
              -pipe -fno-strict-aliasing -Wdeclaration-after-statement -Wempty-body -Wignored-qualifiers \
              -Wstrict-prototypes -Wtype-limits -Wvla -Wwrite-strings -Wpointer-arith \
              -Wno-pragma-pack \
              -gdwarf-2 -gstrict-dwarf \
              -fno-omit-frame-pointer -std=gnu89 -g -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0

.PHONY: clean
clean :
	@ rm -f sortkey.o
	@ rm -f collation.o
	@ rm -f wctype.o
