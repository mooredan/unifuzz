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

# =========================
# Platform-Aware Makefile for unifuzz
# =========================

# =========================
# Paths and Inputs
# =========================
NAME = unifuzz
SRC = $(NAME).c
OBJ = $(NAME).o
OUT = unifuzz

WINE_BASE = wine
WINE_INCLUDE = -I$(WINE_BASE)/include -I$(WINE_BASE)/include/wine
WINE_SRCS = \
    $(WINE_BASE)/libs/wine/sortkey.c \
    $(WINE_BASE)/libs/wine/collation.c \
    $(WINE_BASE)/libs/wine/wctype.c \
    $(WINE_BASE)/dlls/kernel32/locale.c

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S), Darwin)
   TARGET_OS := macos
   TARGET_ARCH := $(UNAME_M)
   TGT = $(addsuffix .dylib, $(NAME))
   GCC_OPTS = -dynamiclib
   WINE_OBJS =
else ifeq ($(UNAME_S), Linux)
   TARGET_OS := linux
   ARCH := 64
   # TARGET_ARCH := $(UNAME_M)
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

# # Allow override for cross-compiling to macOS from Linux
# CC ?= clang
# CFLAGS ?=
# LDFLAGS ?=



# =========================
# Targets
# =========================

.PHONY: all clean info publish test

all : $(TGT)

$(TGT) : $(SRC)  $(WINE_OBJS)
	$(CC) -m$(ARCH) -g -fPIC \
              -Wall \
	      $(WINE_INCLUDE) \
              $(GCC_OPTS) \
              $(SRC) \
              $(WINE_OBJS) \
              -o $@


# $(OUTFILE): $(OBJ) $(WINE_OBJS) $
# 	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f *.o $(NAME).so $(NAME).dylib
	find $(WINE_BASE) -name '*.o' -delete

info:
	@echo "Target OS:    $(TARGET_OS)"
	@echo "Target ARCH:  $(TARGET_ARCH)"
	@echo "Compiler:     $(CC)"
	@echo "Output File:  $(TGT)"
	@echo "WINE_OBJS:    $(WINE_OBJS)"
	@echo "CFLAGS:       $(CFLAGS)"

# =========================
# Publish Target
# =========================

DIST_DIR = dist
PLATFORM_TAG = $(TARGET_OS)-$(TARGET_ARCH)

publish: all
	mkdir -p $(DIST_DIR)
	cp $(OUTFILE) $(DIST_DIR)/$(OUT)-$(PLATFORM_TAG)$(suffix $(OUTFILE))
	@echo "\u2705 Published: $(DIST_DIR)/$(OUT)-$(PLATFORM_TAG)$(suffix $(OUTFILE))"
