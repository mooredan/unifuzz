# unifuzz



Original unifuzz.c code came from here:
https://sqlitetoolsforrootsmagic.wikispaces.com/file/view/unifuzz.c/469471116/unifuzz.c

..and the need for a such an extension is explained here: 
https://sqlitetoolsforrootsmagic.wikispaces.com/RMNOCASE+-+faking+it+in+SQLite+Expert%2C+command-line+shell+et+al


The above page provides a link to a WIN32 DLL for use on Windows systems, which cannot be used on macOS or Linux systems.
Therefore the code needs to be compiled to work on a macOS system.
Jun 2025 : Makefile changes to compile on a Linux system made

This page gives some details on how to compile a SQLite loadable extension:
https://sqlite.org/loadext.html


One hurdle in using the unifuzz.c code is that the author(s) used the Windows CompareStringW()
function, and warns us of this.  A replacement for this code is needed for the extension to
link. 

Wine (https://www.winehq.org/) provides code for running Windows programs on Unix-based systems (like macOS).
Rather than install Wine and figuring out how to get the unifuzz extension to use the runtime libraries, it
was decided to just statically compile the needed function into the extension itself.

This required some mining operations to retrieve only the needed code and some minor editing to establish
the minimal set of files needed.  Only two C source files from the base Wine source files were modified --
all other files are used as is.

Compiling
================================
This has only been tested on a macOS system (MacBook Pro (15-inch, Early 2011) running macOS Sierra Version 10.12.6)

Also installed on the system is MacPorts (including Apple's Xcode Developer Tools), sqlite3 3.20.1 (from MacPorts)

Having the source for Wine is not necessary (but may be necessary in the future).

Clone the repository and compile:

```
% git clone https://github.com/mooredan/unifuzz.git
% cd unifuzz
% make
```

That's it. The result should be the SQLite loadable extension "unifuzz.dylib"

Usage
================================
Run sqlite3 referencing a SQLite database,
... Load the extension with .load
... Run queries

The transcript below shows the collation error prior to 
loading the extension and success after doing so.

```
$ sqlite3 ~/Documents/Genealogy/RootsMagic/ZebMoore.rmgc
SQLite version 3.20.1 2017-08-24 16:21:36
Enter ".help" for usage hints.
sqlite> SELECT Surname, Given FROM NameTable WHERE Surname LIKE "Moore" LIMIT 10;
Error: no such collation sequence: RMNOCASE
sqlite> .load unifuzz
sqlite> SELECT Surname, Given FROM NameTable WHERE Surname LIKE "Moore" LIMIT 10;
Moore|
Moore|?
Moore|?
Moore|Abram Schultz
Moore|Ada
Moore|Ada
Moore|Ada G
Moore|Ada Grace
Moore|Ada Mae
Moore|Ada Pearl
sqlite> .quit
```


To Do
==============================================================
* Automatically reference the Wine source files and copy the
  necessary files over and patch the two files that need 
  modification.

* Release compiled extensions for various platforms (being with macOS)

* Resolve the last two compile warnings (initialization of array of struct
  variables)

* More testing? ...seems to be working OK, however it might choke on names
  where non-English characters are used in names


Acknowledgements
=================================
Big thanks to the following folks, without them blazing the trail,
writing and testing the original code, none of this would have been
possible:

* Jean-Christophe Deschamps : last known author of unifuzz.c
* Tom Holden : founder(?) and maintainer of SQLite Tools for RootsMagic
* Richard Otter : https://github.com/RichardOtter/Genealogy-scripts
* Wine authors

