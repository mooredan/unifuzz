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

WINE_BASE = wine
WINE_INCLUDE = -I$(WINE_BASE)/include -I$(WINE_BASE)/include/wine
WINE_SRCS = $(WINE_BASE)/libs/wine/sortkey.c \
            $(WINE_BASE)/libs/wine/collation.c \
            $(WINE_BASE)/libs/wine/wctype.c \
            $(WINE_BASE)/dlls/kernel32/locale.c

# If cross compiling to MacOSX from Linux
# location of the cross compiler
OSXCROSS_PATH := $(HOME)/src/osxcross

SQLITE3 := sqlite3
TEST_DB := testdata.rmtree 

FORCE_CROSS =? 0

CFLAGS =

ifeq ($(OS),Windows_NT)
   EXT = dll
   TARGET_OS := win 
   $(error OS: $(UNAME_S) not yet supported)
else
   UNAME_S = $(shell uname -s)
   UNAME_M = $(shell uname -m)
   ifeq ($(FORCE_CROSS), 1)
      UNAME_S = Darwin
      UNAME_M = arm64
   endif
   ifeq ($(UNAME_S), Darwin)
      TARGET_OS := macos
      EXT = dylib
      WINE_OBJS =
      ifeq ($(FORCE_CROSS), 1)
         CC := $(OSXCROSS_PATH)/target/bin/arm64-apple-darwin23.5-clang
      else
         CC := clang
      endif
      CFLAGS += -dynamiclib -undefined dynamic_lookup
      WINE_INCLUDE =
      # GCC_OPTS = -dynamiclib
   else ifeq ($(UNAME_S), Linux)
      TARGET_OS := linux
      EXT = so
      ARCH := 64
      GCC_OPTS = -shared
      WINE_INCLUDE = -I$(WINE_BASE)/include -I$(WINE_BASE)/include/wine
      WINE_SRCS = \
          $(WINE_BASE)/libs/wine/sortkey.c \
          $(WINE_BASE)/libs/wine/collation.c \
          $(WINE_BASE)/libs/wine/wctype.c \
          $(WINE_BASE)/dlls/kernel32/locale.c
      WINE_OBJS = $(WINE_SRCS:.c=.o)
      CC = gcc
      CFLAGS += -m$(ARCH) -g -fPIC -Wall -O2 \
         $(WINE_INCLUDE) \
         -D__WINESRC__ \
         -D_KERNEL32_ \
         -D_NORMALIZE_ \
         -D_REENTRANT \
         -U_FORTIFY_SOURCE  \
         -D_FORTIFY_SOURCE=0 \
         -DWINE_UNICODE_API=""
   else
      $(error Unsupported OS: $(UNAME_S))
   endif
endif

TGT = $(addsuffix .$(EXT), $(NAME))
DIST_DIR = dist
PLATFORM_TAG := $(TARGET_OS)-$(UNAME_M)


# =========================
# Targets
# =========================

.PHONY: all clean info publish

all: $(TGT)

$(TGT) : $(SRC)  $(WINE_OBJS)
	$(CC) $(CFLAGS) \
              -Wall \
              $(GCC_OPTS) \
              $(SRC) \
              $(WINE_OBJS) \
              -o $@


clean:
	- @ rm -f *.o $(NAME).so $(NAME).dylib
	- @ rm -f test.sql testdb.sql
	- @ rm -f test_output.txt testdb_output.txt
	@ if [ -d $(WINE_BASE) ]; then find $(WINE_BASE) -name '*.o' -delete; fi

info:
	@ echo "Target OS:    $(TARGET_OS)"
	@ echo "UNAME_M:      $(UNAME_M)"
	@ echo "Compiler:     $(CC)"
	@ echo "WINE_OBJS:    $(WINE_OBJS)"
	@ echo "CFLAGS:       $(CFLAGS)"
	@ echo "TGT:          $(TGT)"
	@ echo "PLATFORM_TAG: $(PLATFORM_TAG)"
	@ echo "FORCE_CROSS:  $(FORCE_CROSS)"


# =========================
# Testing 
# =========================

.PHONY: testall
testall: test testdb test_chrw

.PHONY: test
test: $(TGT)
	@ echo "Running SQLite extension test for $(TGT)..."
	@ echo ".load ./$(TGT)" > test.sql
	@ echo "-- Scalar function tests" >> test.sql
	@ echo "SELECT proper('john doe') = 'John Doe';" >> test.sql
	@ echo "SELECT flip('abc123') = '321cba';" >> test.sql
	@ echo "SELECT like('foobar', 'FOO%');" >> test.sql
	@ echo "SELECT unaccent('crème brûlée') = 'creme brulee';" >> test.sql
	@ echo "-- Collation test: RMNOCASE" >> test.sql
	@ echo "DROP TABLE IF EXISTS test_collate;" >> test.sql
	@ echo "CREATE TABLE test_collate(name TEXT COLLATE RMNOCASE);" >> test.sql
	@ echo "INSERT INTO test_collate(name) VALUES ('abc'), ('ABC');" >> test.sql
	@ echo "SELECT COUNT(*) = 2 FROM test_collate WHERE name = 'abc';" >> test.sql
	@ echo "-- Optional: Extended function checks" >> test.sql
	@ echo "SELECT lower('ÉLAN') = 'élan';" >> test.sql
	@ echo "SELECT upper('groß') = 'GROSS';" >> test.sql
	@ echo "SELECT name FROM pragma_function_list WHERE name IN ('lower', 'upper', 'ascii', 'space', 'case');" >> test.sql
	@ $(SQLITE3) < test.sql > test_output.txt 2>&1 \
	  && echo "✅ SQLite extension test passed" \
	  || (cat test_output.txt; echo "❌ SQLite test failed"; exit 1)
	@ rm -f test.sql test_output.txt

.PHONY: testdb
testdb: $(TGT) $(TEST_DB)
	@ echo "Running real RootsMagic test against $(TEST_DB)..."
	@ echo ".load ./$(TGT)" > testdb.sql
	@ echo "REINDEX RMNOCASE;" >> testdb.sql
	@ echo "-- RMNOCASE collation test" >> testdb.sql
	@ echo "SELECT 'PASS' WHERE 'straße' COLLATE RMNOCASE = 'STRASSE';" >> testdb.sql
	@ echo "-- lower/upper" >> testdb.sql
	@ echo "SELECT lower('ÜBER') = 'über';" >> testdb.sql
	@ echo "SELECT upper('über') = 'ÜBER';" >> testdb.sql
	@ echo "-- proper casing" >> testdb.sql
	@ echo "SELECT proper('john DOE') = 'John Doe';" >> testdb.sql
	@ echo "-- flip string" >> testdb.sql
	@ echo "SELECT flip('abcd') = 'dcba';" >> testdb.sql
	@ echo "-- RMNOCASE should match both upper/lower case" >> testdb.sql
	@ echo "SELECT DISTINCT Surname FROM NameTable ORDER BY Surname COLLATE RMNOCASE LIMIT 10;" >> testdb.sql
	@ echo "-- Case-insensitive equality test" >> testdb.sql
	@ echo "SELECT COUNT(*) FROM NameTable WHERE Surname COLLATE RMNOCASE = 'smith';" >> testdb.sql
	@ echo "-- LIKE override test" >> testdb.sql
	@ echo "SELECT COUNT(*) FROM NameTable WHERE Given LIKE 'jo%';" >> testdb.sql
	@ echo "-- ascii conversion" >> testdb.sql
	@ echo "-- check for optional functions" >> testdb.sql
	@ echo "SELECT name FROM pragma_function_list WHERE name IN ('ascii', 'space');" >> testdb.sql
	@ echo "-- Unicode LIKE override" >> testdb.sql
	@ echo "SELECT 'MATCH' WHERE 'Søren' LIKE 'sø%';" >> testdb.sql
	@ $(SQLITE3) $(TEST_DB) < testdb.sql > testdb_output.txt 2>&1 && \
	  echo "✅ testdb passed" || \
	  (cat testdb_output.txt; echo "❌ testdb failed"; exit 1)
	@ rm -f testdb.sql


.PHONY: test_chrw
test_chrw:
	@echo "Running CHRW function tests..."
	@$(SQLITE3) -batch -noheader $(TEST_DB) \
		".load ./$(TGT)" \
		"SELECT chrw(65) = 'A';" \
		"SELECT chrw(0xA9) = '©';" \
		"SELECT chrw(0xD800) = char(0xFFFD);" \
		"SELECT chrw(0xFFFF) = char(0xFFFD);" \
		"SELECT chrw(0x110000) = char(0xFFFD);" \
		"SELECT chrw(x'0001F4A9') = '💩';" \
		"SELECT chrw(NULL) IS NULL;" | grep -v '^[01]$$' || \
		(echo "❌ chrw test failed" && exit 1)



# =========================
# Publish Target
# =========================


publish: all
	@ echo "📦 Publishing to $(DIST_DIR)/$(PLATFORM_TAG)/$(TGT)"
	@ mkdir -p $(DIST_DIR)/$(PLATFORM_TAG)
	@ cp -v $(TGT) $(DIST_DIR)/$(PLATFORM_TAG)/$(TGT)





