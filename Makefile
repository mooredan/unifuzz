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


ARCH=64

all : unifuzz.dylib

unifuzz.dylib : unifuzz.c \
                wine/libs/wine/collation.o \
                wine/libs/wine/wctype.o \
                wine/dlls/kernel32/locale.o \
                wine/libs/wine/sortkey.o
	gcc -m$(ARCH) -g -fPIC -dynamiclib \
                          -Iwine/include \
                          -Wall \
                          unifuzz.c \
                          wine/libs/wine/collation.o \
                          wine/libs/wine/wctype.o \
                          wine/dlls/kernel32/locale.o \
                          wine/libs/wine/sortkey.o -o unifuzz.dylib

wine/libs/wine/sortkey.o : 
	make -C wine/libs/wine ARCH=$(ARCH) $(lastword $(subsr /, ,$@))

wine/libs/wine/collation.o : 
	make -C wine/libs/wine ARCH=$(ARCH) $(lastword $(subst /, ,$@)) 

wine/libs/wine/wctype.o : 
	make -C wine/libs/wine ARCH=$(ARCH) $(lastword $(subst /, ,$@)) 

wine/dlls/kernel32/locale.o : 
	make -C wine/dlls/kernel32 ARCH=$(ARCH) $(lastword $(subst /, ,$@)) 


.PHONY: clean
clean :
	- @ make -C wine/libs/wine clean
	- @ make -C wine/dlls/kernel32 clean
	- @ rm -f unifuzz.dylib
	- @ rm -rf unifuzz.dylib.dSYM/
