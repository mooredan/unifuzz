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

WINE_BASE = wine

NAME = unifuzz
SRC = $(addsuffix .c, $(NAME))

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S), Darwin)
   TGT = $(addsuffix .dylib, $(NAME))
   GCC_OPTS = -dynamiclib
   WINE_OBJS =
else ifeq ($(UNAME_S), Linux)
   TGT = $(addsuffix .so, $(NAME))
   GCC_OPTS = -shared
   WINE_INCLUDE = -I$(WINE_BASE)/include -I$(WINE_BASE)/include/wine
   WINE_SRCS = \
       $(WINE_BASE)/libs/wine/sortkey.c \
       $(WINE_BASE)/libs/wine/collation.c \
       $(WINE_BASE)/libs/wine/wctype.c \
       $(WINE_BASE)/dlls/kernel32/locale.c
   WINE_OBJS = $(WINE_SRCS:.c=.o)
   CC = gcc
   CFLAGS += -m$(ARCH) -fPIC -Wall -O2 \
      $(WINE_INCLUDE) \
      -D__WINESRC__ \
      -D_KERNEL32_ \
      -D_NORMALIZE_ \
      -D_REENTRANT \
      -U_FORTIFY_SOURCE  \
      -D_FORTIFY_SOURCE=0 \
      -DWINE_UNICODE_API=""

#               -pipe \
#               -fno-strict-aliasing \
#               -gdwarf-2 \
#               -gstrict-dwarf \
#               -fno-omit-frame-pointer \
#               -std=gnu89 \



else
    $(error Unsupported OS: $(UNAME_S))
endif

all : $(TGT)

$(TGT) : $(SRC)  $(WINE_OBJS)
	$(CC) -m$(ARCH) -g -fPIC \
              -Iwine/include \
              -Wall \
              $(GCC_OPTS) \
              $(SRC) \
              $(WINE_OBJS) \
              -o $@


# wine/libs/wine/sortkey.o :
# 	$(MAKE) -C wine/libs/wine ARCH=$(ARCH) $(lastword $(subsr /, ,$@))
# 
# wine/libs/wine/collation.o :
# 	$(MAKE) -C wine/libs/wine ARCH=$(ARCH) $(lastword $(subst /, ,$@))
# 
# wine/libs/wine/wctype.o :
# 	$(MAKE) -C wine/libs/wine ARCH=$(ARCH) $(lastword $(subst /, ,$@))
# 
# wine/dlls/kernel32/locale.o :
# 	$(MAKE) -C wine/dlls/kernel32 ARCH=$(ARCH) $(lastword $(subst /, ,$@))


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
	rm -f *.o $(OUT).so $(OUT).dylib
	find $(WINE_BASE) -name '*.o' -delete
	- @ rm -f unifuzz.dylib
	- @ rm -f unifuzz.so
	- @ rm -rf unifuzz.dylib.dSYM/
	- @ rm -f $(TGT)-unifuzz.c.*
