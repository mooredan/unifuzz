#     unifuzz SQLite loadable extension (Makefile)
#
#     Copyright (C) 2017,2025  Daniel Moore
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


ARCH := 64



UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Linux)
   TGT = unifuzz.so
   GCC_OPTS = -shared
endif
ifeq ($(UNAME_S), Darwin)
   TGT = unifuzz.dylib
   GCC_OPTS = -dynamiclib
endif



all : $(TGT)

$(TGT) : unifuzz.c \
                wine/libs/wine/collation.o \
                wine/libs/wine/wctype.o \
                wine/dlls/kernel32/locale.o \
                wine/libs/wine/sortkey.o
	gcc -m$(ARCH) -g -fPIC \
                          -Iwine/include \
                          -Wall \
                          $(GCC_OPTS) \
                          unifuzz.c \
                          wine/libs/wine/collation.o \
                          wine/libs/wine/wctype.o \
                          wine/dlls/kernel32/locale.o \
                          wine/libs/wine/sortkey.o -o $@

wine/libs/wine/sortkey.o :
	$(MAKE) -C wine/libs/wine ARCH=$(ARCH) $(lastword $(subsr /, ,$@))

wine/libs/wine/collation.o :
	$(MAKE) -C wine/libs/wine ARCH=$(ARCH) $(lastword $(subst /, ,$@))

wine/libs/wine/wctype.o :
	$(MAKE) -C wine/libs/wine ARCH=$(ARCH) $(lastword $(subst /, ,$@))

wine/dlls/kernel32/locale.o :
	$(MAKE) -C wine/dlls/kernel32 ARCH=$(ARCH) $(lastword $(subst /, ,$@))


.PHONY: publish
publish : $(TGT)
	@ if [ ! -d release ]; then mkdir release; fi
	@ if [ -r release/$(TGT) ]; then \
           if [ $$(diff -q $(TGT) release/$(TGT)) ]; then \
              cp -v $(TGT) release/$(TGT);\
           fi; \
        else \
           cp -v $(TGT) release/$(TGT);\
        fi


.PHONY: clean
clean :
	- @ $(MAKE) -C wine/libs/wine clean
	- @ $(MAKE) -C wine/dlls/kernel32 clean
	- @ rm -f unifuzz.dylib
	- @ rm -f unifuzz.so
	- @ rm -rf unifuzz.dylib.dSYM/
	- @ rm -f $(TGT)-unifuzz.c.*
