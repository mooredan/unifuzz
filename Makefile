ARCH=64

all : unifuzz.dylib

unifuzz.dylib : unifuzz.c \
                wine/libs/wine/collation.o \
                wine/libs/wine/wctype.o \
                wine/dlls/kernel32/locale.o \
                wine/libs/wine/sortkey.o
	gcc -m$(ARCH) -g -fPIC -Wall -dynamiclib \
                          -Iwine/include \
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
