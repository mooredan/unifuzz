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

.PHONY: all clean info publish test testdb

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
	- @ rm -f *.o $(NAME).so $(NAME).dylib
	- @ rm -f test.sql testdb.sql
	- @ rm -f test_output.txt testdb_output.txt
	@ if [ -d $(WINE_BASE) ]; then find $(WINE_BASE) -name '*.o' -delete; fi

info:
	@echo "Target OS:    $(TARGET_OS)"
	@echo "Target ARCH:  $(TARGET_ARCH)"
	@echo "Compiler:     $(CC)"
	@echo "Output File:  $(TGT)"
	@echo "WINE_OBJS:    $(WINE_OBJS)"
	@echo "CFLAGS:       $(CFLAGS)"



# =========================
# Testing 
# =========================
test: $(TGT)
	@echo "Running SQLite extension test for $(TGT)..."
	@echo ".load ./$(TGT)" > test.sql
	@echo "-- Scalar function tests" >> test.sql
	@echo "SELECT proper('john doe') = 'John Doe';" >> test.sql
	@echo "SELECT flip('abc123') = '321cba';" >> test.sql
	@echo "SELECT like('foobar', 'FOO%');" >> test.sql
	@echo "SELECT unaccent('crème brûlée') = 'creme brulee';" >> test.sql
	@echo "-- Collation test: RMNOCASE" >> test.sql
	@echo "DROP TABLE IF EXISTS test_collate;" >> test.sql
	@echo "CREATE TABLE test_collate(name TEXT COLLATE RMNOCASE);" >> test.sql
	@echo "INSERT INTO test_collate(name) VALUES ('abc'), ('ABC');" >> test.sql
	@echo "SELECT COUNT(*) = 2 FROM test_collate WHERE name = 'abc';" >> test.sql
	@echo "-- Optional: Extended function checks" >> test.sql
	@echo "SELECT lower('ÉLAN') = 'élan';" >> test.sql
	@echo "SELECT upper('groß') = 'GROSS';" >> test.sql
	@echo "SELECT name FROM pragma_function_list WHERE name IN ('lower', 'upper', 'ascii', 'space', 'case');" >> test.sql
	@sqlite3 < test.sql > test_output.txt 2>&1 \
	  && echo "✅ SQLite extension test passed" \
	  || (cat test_output.txt; echo "❌ SQLite test failed"; exit 1)
	@rm -f test.sql test_output.txt

testdb: $(TGT) testdata.rmtree
	@echo "Running real RootsMagic test against testdata.rmtree..."
	@echo ".load ./$(TGT)" > testdb.sql
	@echo "REINDEX RMNOCASE;" >> testdb.sql
	@echo "-- RMNOCASE collation test" >> testdb.sql
	@echo "SELECT 'PASS' WHERE 'straße' COLLATE RMNOCASE = 'STRASSE';" >> testdb.sql
	@echo "-- lower/upper" >> testdb.sql
	@echo "SELECT lower('ÜBER') = 'über';" >> testdb.sql
	@echo "SELECT upper('über') = 'ÜBER';" >> testdb.sql
	@echo "-- proper casing" >> testdb.sql
	@echo "SELECT proper('john DOE') = 'John Doe';" >> testdb.sql
	@echo "-- flip string" >> testdb.sql
	@echo "SELECT flip('abcd') = 'dcba';" >> testdb.sql
	@echo "-- RMNOCASE should match both upper/lower case" >> testdb.sql
	@echo "SELECT DISTINCT Surname FROM NameTable ORDER BY Surname COLLATE RMNOCASE LIMIT 10;" >> testdb.sql
	@echo "-- Case-insensitive equality test" >> testdb.sql
	@echo "SELECT COUNT(*) FROM NameTable WHERE Surname COLLATE RMNOCASE = 'smith';" >> testdb.sql
	@echo "-- LIKE override test" >> testdb.sql
	@echo "SELECT COUNT(*) FROM NameTable WHERE Given LIKE 'jo%';" >> testdb.sql
	@echo "-- ascii conversion" >> testdb.sql
	@echo "-- check for optional functions" >> testdb.sql
	@echo "SELECT name FROM pragma_function_list WHERE name IN ('ascii', 'space');" >> testdb.sql
	@echo "-- Unicode LIKE override" >> testdb.sql
	@echo "SELECT 'MATCH' WHERE 'Søren' LIKE 'sø%';" >> testdb.sql
	@sqlite3 testdata.rmtree < testdb.sql > testdb_output.txt 2>&1 && \
	  echo "✅ testdb passed" || \
	  (cat testdb_output.txt; echo "❌ testdb failed"; exit 1)
	@rm -f testdb.sql


# =========================
# Publish Target
# =========================
DIST_DIR = dist
PLATFORM_TAG = $(TARGET_OS)-$(TARGET_ARCH)

publish: all
	mkdir -p $(DIST_DIR)
	cp $(OUTFILE) $(DIST_DIR)/$(OUT)-$(PLATFORM_TAG)$(suffix $(OUTFILE))
	@echo "\u2705 Published: $(DIST_DIR)/$(OUT)-$(PLATFORM_TAG)$(suffix $(OUTFILE))"
