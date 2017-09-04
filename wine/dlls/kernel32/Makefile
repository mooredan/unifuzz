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
              -Wall \
              -pipe \
              -fno-strict-aliasing \
              -Wdeclaration-after-statement \
              -Wempty-body -Wignored-qualifiers -Wstrict-prototypes \
              -Wtype-limits -Wvla -Wwrite-strings \
              -Wpointer-arith \
              -gdwarf-2 -gstrict-dwarf \
              -fno-omit-frame-pointer -std=gnu89 \
              -g -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0


.PHONY: clean
clean :
	@ rm -f locale.o
