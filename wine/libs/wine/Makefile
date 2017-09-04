ARCH ?= 64

all : collation.o sortkey.o wctype.o

sortkey.o : sortkey.c
	clang -m$(ARCH) -c -o sortkey.o sortkey.c \
              -I../../../wine/include \
              -D__WINESRC__ \
              -DWINE_UNICODE_API="" -D_REENTRANT -fPIC \
              -Wall -pipe -fno-strict-aliasing \
              -Wdeclaration-after-statement  -Wempty-body \
              -Wignored-qualifiers  -Wstrict-prototypes \
              -Wtype-limits  -Wvla \
              -Wwrite-strings  -Wpointer-arith \
              -gdwarf-2 -gstrict-dwarf \
              -fno-omit-frame-pointer -std=gnu89 \
              -g -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0

collation.o : collation.c
	clang -m$(ARCH) -c -o collation.o collation.c \
              -D__WINESRC__ -DWINE_UNICODE_API="" -D_REENTRANT \
              -fPIC -Wall -pipe -fno-strict-aliasing -Wdeclaration-after-statement -Wempty-body \
              -Wignored-qualifiers -Wstrict-prototypes -Wtype-limits -Wvla -Wwrite-strings -Wpointer-arith \
              -gdwarf-2 -gstrict-dwarf -fno-omit-frame-pointer -std=gnu89 -g -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0


wctype.o : wctype.c
	clang -m$(ARCH) -c -o wctype.o wctype.c \
              -I../../../wine/include \
              -D__WINESRC__ -DWINE_UNICODE_API="" -D_REENTRANT -fPIC \
              -Wall -pipe -fno-strict-aliasing -Wdeclaration-after-statement -Wempty-body -Wignored-qualifiers \
              -Wstrict-prototypes -Wtype-limits -Wvla -Wwrite-strings -Wpointer-arith -gdwarf-2 -gstrict-dwarf \
              -fno-omit-frame-pointer -std=gnu89 -g -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0

.PHONY: clean
clean :
	@ rm -f sortkey.o
	@ rm -f collation.o
	@ rm -f wctype.o
