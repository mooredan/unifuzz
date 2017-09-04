
ARCH=32
ARCH=64

all : unifuzz.dylib


unifuzz.o : unifuzz.c
	gcc -I/opt/local/include/wine/windows -c -o unifuzz.o unifuzz.c


unifuzz.dylib : unifuzz.c collation.o wctype.o locale.o sortkey.o my_def.h
	gcc -m$(ARCH) -g -fPIC -Wall -dynamiclib unifuzz.c collation.o wctype.o locale.o sortkey.o -o unifuzz.dylib



sortkey.o : sortkey.c
	clang -m$(ARCH) -c -o sortkey.o sortkey.c \
              -I. \
              -I../../include \
              -I/Users/mooredan/src/wine/include \
              -D__WINESRC__ \
              -DWINE_UNICODE_API="" -D_REENTRANT -fPIC \
              -Wall -pipe -fno-strict-aliasing \
              -Wdeclaration-after-statement \
              -Wempty-body \
              -Wignored-qualifiers \
              -Wstrict-prototypes \
              -Wtype-limits \
              -Wvla \
              -Wwrite-strings \
              -Wpointer-arith \
              -gdwarf-2 \
              -gstrict-dwarf \
              -fno-omit-frame-pointer \
              -std=gnu89 \
              -g \
              -U_FORTIFY_SOURCE \
              -D_FORTIFY_SOURCE=0


locale.o : locale.c
	clang -m$(ARCH) -c -o locale.o locale.c \
	      -I. \
              -I/Users/mooredan/src/wine/dlls/kernel32 \
              -I../../include \
              -I/Users/mooredan/src/wine/include \
              -D__WINESRC__ \
              -D_KERNEL32_ \
              -D_NORMALIZE_ \
              -D_REENTRANT \
              -fPIC \
              -Wall \
              -pipe \
              -fno-strict-aliasing \
              -Wdeclaration-after-statement \
              -Wempty-body \
              -Wignored-qualifiers \
              -Wstrict-prototypes \
              -Wtype-limits \
              -Wvla \
              -Wwrite-strings \
              -Wpointer-arith \
              -gdwarf-2 \
              -gstrict-dwarf \
              -fno-omit-frame-pointer \
              -std=gnu89 \
              -g \
              -U_FORTIFY_SOURCE \
              -D_FORTIFY_SOURCE=0


wctype.o : wctype.c
	clang -m$(ARCH) -c -o wctype.o wctype.c -I. \
              -I/Users/mooredan/src/wine/include \
              -I../../include -D__WINESRC__ -DWINE_UNICODE_API="" -D_REENTRANT -fPIC \
              -Wall -pipe -fno-strict-aliasing -Wdeclaration-after-statement -Wempty-body -Wignored-qualifiers \
              -Wstrict-prototypes -Wtype-limits -Wvla -Wwrite-strings -Wpointer-arith -gdwarf-2 -gstrict-dwarf \
              -fno-omit-frame-pointer -std=gnu89 -g -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0



collation.o : collation.c
	clang -m$(ARCH) -c -o collation.o collation.c -I. \
              -I/Users/mooredan/src/wine/include \
              -I../../include -D__WINESRC__ -DWINE_UNICODE_API="" -D_REENTRANT \
              -fPIC -Wall -pipe -fno-strict-aliasing -Wdeclaration-after-statement -Wempty-body \
              -Wignored-qualifiers -Wstrict-prototypes -Wtype-limits -Wvla -Wwrite-strings -Wpointer-arith \
              -gdwarf-2 -gstrict-dwarf -fno-omit-frame-pointer -std=gnu89 -g -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0



.PHONY: clean
clean :
	@ rm -f collation.o
	@ rm -f locale.o
	@ rm -f sortkey.o
	@ rm -f unifuzz.dylib
	@ rm -rf unifuzz.dylib.dSYM/
	@ rm -f wctype.o
