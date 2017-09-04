/*
**
**  name:                       unifuzz.c
**
**  version:    (see line below)
*/
#define UNIFUZZ_VERSION Unifuzz v0.3.7  -  Unicode v


#ifndef min
#define min(a,b)   (((a) < (b)) ? (a) : (b))
#endif

/*
**
**  authors:    author of part of original code is Loic Dachary <loic@senga.org>
**              reworked by Ioannis (ioannis.mpsounds.net)
**              almost fully rewritten Jean-Christophe Deschamps <jcd@antichoc.net>
**
**  type:       C extension for SQLite3
**
**  implements: fuzzy Unicode string search
**              locale-invariant Unicode support for casing and collation
**              various utility functions
**
**  warning:    uses Windows CompareStringW() comparison function
**
**  notes:      this loadable extension is currently intended to be built as a
**              single Win32 DLL file.  This is a work in progress, bear with it.
**
**  tabs:       4 spaces
**
**  legalese:   Don't look for a licence reference, there is none. Use this freely,
**              correct its bugs (there are certainly several waiting to destroy
**              your data), give it around, mail it to your representative/lover,
**              as you wish.  Credit DrH, Loic Dachary, Ioannis, Unicode groups
**              and any other contributor.
**
**  History:    0.2.4   2009/10     initial beta
**              0.2.5   2009/11/09  clearer explanations, no code change
**              0.2.6   2009/11/13  fix unaccent table for uppercase german eszet
**              0.2.7   2009/11/16  use slash '/' instead of U+2044 (fraction slash)
**                                  in the unaccent tables for decomposing ¼ ½ ...
**                                  into the _much_ more common 1/4 1/2 ...
**              0.2.8   2009/11/17  add the flip() function to reverse strings
**                                  without regards to decomposed chars, graphemes.
**              0.2.9   2009/11/19  add the chrw(), ascw() and hexw() functions
**              0.3.0   2009/11/29  make auto_load feature optional to remove
**                                  dependancy to sqlite.dll.  Read notes at the end
**                                  of file to learn more.
**              0.3.1   2009/12/01  Fix collations comparison length;
**                                  add new NUMERICS collation function;
**                                  document that index using collations should not
**                                  have the UNIQUE constraint.
**              0.3.2   2009/12/02  ASCW now accept index parameter.
**              0.3.3   2009/12/05  fix NUMERICS to sort rest of string as well but
**                                  with collation NOCASE.
**              0.3.4   2009/12/07  allow registration of some functions to return
**                                  SQLITE_BUSY to permit loading from SQL. Requires
**                                  having some UTF-16 functions even in only UTF-8
**                                  is requested.
**              0.3.5   2010/03/18  Add UNACCENTED collation.
**                                  Add strpos() and strdup() functions.
**              0.3.6   2010/05/21  Fix © --> (C) and ® --> (R).
**              0.3.7   2011/06/19  Add strfilter() and strtaboo().
**              0.3.8   2013/06/21  RMNOCASE is an alias for NOCASE; document unifuzz().
**
**
**  General
**  =======
**
**              The goal was to provide a set of decent Unicode functions without
**              too much bloat and reasonably fast.  In contrast to ICU this code
**              tries to work as locale-independant, which means that compromises
**              had to be made.  There are clearly too many rules and exceptions
**              to achieve a perfect result in such a single small source file.
**
**         |    All functions rely on strings in normal form C.  If you know or
**         |    suspect that your text data contains anything else must be warned
**         |    that the results obtained can be completely unexpected for such
**         |    strings.  This is particularly true for indices.  BTW if you don't
**         |    know what normal forms are, then it is probable that your data is
**         |    already under the C normal form.
**
**              All casing and unaccentuation functions use Unicode 5.1 tries and
**              should port to any system (not tested outside XP x86 yet).
**
**              The fuzzy compare is also portable, with the same disclaimer.
**
**         |    Unfortunately and even if I tried hard other approaches, the two
**         |    collation functions rely on MickeySoft XP CompareStringW function,
**         |    which makes them non portable outside Win 32-64 world.
**         |    It should be possible to come up with, say, a Un*x equivalent or
**         |    at least end up with something very close but I can't help for
**         |    that anytime soon.
**         |
**         |    If ever one of those collations is used to build indices, then they
**         |    \\\_MUST_/// be rebuilt if the underlying compare function changes.
**         |
**         |    Observe that since the collations compare modified data (unaccented
**         |    strings for NOCASE[U] and NAMES, numeric part only for NUMERIC), it
**         |    is not safe to put a UNIQUE constraints on indices using these
**         |    collations.  This is nothing new: the same remark applies to the
**         |    built-in NOCASE as well.
**         |
**         |    I believe the Win function used is now reasonably stable accross
**         |    Win** versions and covers many aspects of Unicode v5.1, even with
**         |    the locale-independant parameter used here.  But of course, this
**         |    kind of choice is likely to be useful to many and, at the same
**         |    time, make almost everyone unhappy.  This is because many [dark]
**         |    corners had to be cut to avoid data and code bloat.  Yet I believe
**         |    this offers a decent alternative between SQLite pure-ASCII offer
**         |    and a full blown ICU implementation.
**
**              If you application demands something else, feel free to adapt the
**              code (but please use different names to avoid confusion).
**
**              A small set of UTF-8 and UTF-16 functions is ready for use with no
**              8 <--> 16 overhead.  A preprocessor define can be used to select
**              which UTF-* set of functions to include, or both.
**              Also, the original unaccent tables used "short" but some codepoints
**              require 32-bit.  I choose to expand these tables only to hold 32-
**              bit values so that all Unicode 5.1 uppercase, lowercase and title
**              _simple_ conversions are enforced.  No, not all: I dropped Deseret
**              as I think that my friends around Salt Lake using Deseret already
**              have their own tools.
**
**
**              Some will call this a half-baked cake, because there may be not
**              even one script/language that will be handled _perfectly_.
**              Others will say it's an oven-dessicated chicken, because of too
**              many cycles wasted for a non-portable, non-universal result.
**
**
**  Ligatures
**  =========
**
**              Provision has been made to handle ligatures in the unaccent
**              function. See below for the special status of 'ß'. When they have
**              a defined "simple"" decomposition, then they are decomposed.
**              There is little point using the output of unaccent() per se: it
**              mainly makes sense used inside comparison functions (LIKE, GLOB,
**              TYPOS).  unaccent() can nonetheless be useful for preparing data
**              before indexing by FTSx: this way searches can be successful
**              independantly of diacritics, for scripts where this is a sensible
**              option.
**
**
**  The German ß (sharp s)
**  ======================
**
**              The special case of the German eszet 'ß' has been handled as
**              follows:
**                          LOWER('ß')         = 'ß'
**                          UPPER('ß')         = 'SS'
**                          TITLE('ß')         = 'SS'
**                          PROPER('ß')        = 'ß'        head letter (normaly never occurs)
**                          PROPER('ß')        = 'ß'        subsequent letters
**                          FOLD('ß')          = 'ß'
**                          UNACCENT('ß')      = 'ss'
**
**                          LOWER('\u1E9E')    = 'ß'
**                          UPPER('\u1E9E')    = 0x1E9E
**                          TITLE('\u1E9E')    = 0x1E9E
**                          PROPER('\u1E9E')   = 0x1E9E     head letter
**                          PROPER('\u1E9E')   = 'ß'        subsequent letters
**                          FOLD('\u1E9E')     = 0x1E9E
**                          UNACCENT('\u1E9E') = 'SS'
**
**              This choice may not be the best (it may change string length)
**              but it seems to be a good compromise.  Notably the recently
**              introduced capital sharp s (U+01E9E) is _not_ considered as the
**              uppercase of 'ß', while 'ß' is the lowercase of U+01E9E.
**              It sounds like most available fonts are not (yet) aware of the
**              (U+01E9E) introduction.  In contrast the substitution ß <--> ss
**              is clearly very common.
**
**
**  Unaccenting
**  ===========
**
**              This set of functions makes fair use of unaccent().  It should
**              not be considered a linguistic aid at all, since the notions of
**              script and locale are ignored.  It is instead a "basic match"
**              thing because when functions like fuzzy search are used, there
**              must be some kind of human validation afterwards.  For instance
**              unaccenting sentences like "Æ e i åa å o e i åa o å" which means,
**              in Trondheim dialect, "I am in the river and she is in the river
**              as well" is likely to produce strict nonsense but a human operator
**              can probably recognize this in "ae e i aa a o e i aa o a" if (s)he
**              can read the dialect.
**
**              In view of this, one should never consider that if typos(st1, st2)
**              returns 0, then the strings are exactly equivalent.  All it means
**              is that they _look_ very close if all accents are removed from
**              them.
**
**              The German Umlaut letters ä, Ä, ö, Ö, ü, Ü are simply unaccented in
**              resp. a, A, o, O, u, U without any 'e' appended.  Doing so would
**              be nonsense in non-Germanic scripts.  LIKE('Köln', 'Koeln') yields
**              False but LIKE('Köln', 'koln') yields True.
**
**              Despite being a crude approximation, unaccent() can also be very
**              useful for full text search without dependency on diacritics. You
**              may want to try passing your data as lower(unaccented()) to FTS3.
**              As mentionned above, this will work fine for any scripts where
**              removal of diacritic signs don't completely change the meaning to
**              the point of being unintelligible.  Such searches will run at full
**              speed with non-ICU FTS3.  Beware that then the snippet() function
**              may fail to locate matches!
**
**
**  Casing
**  ======
**
**              Similarily, upper(), lower() and title() are also approximations.
**              There are probably scripts where upper- and lower-case conversions
**              of some codepoints are terribly wrong.  Even if the tables used
**              come from official Unicode specifications, there are enough cases
**              of exception to invalidate every attempt to get it right in the
**              general case (no script/locale selection).  Anyway I believe the
**              result can be useful for many users despite the limitations.
**
**              fold() is _very_ similar to lower(): the two differ only for 16
**              codepoints in the plane 0.  Those short on memory could make a
**              derivative extension omitting the fold tables and using a 16-entry
**              linear table.
**
**
**  Current problems
**  ================
**
**         |    o) the collation lacks portability: one Windows call is used.
**
**              o) the data table for unaccent() had problems in original source:
**                 data is short[] but some values were > 0xFFFF.  I expanded the
**                 table to Unicode characters (u32) to hold those codes.  As a
**                 consequence the current implementation is larger than the
**                 original version.
**
**              o) For similar reason (codepoints > 0xFFFF) the Deseret alphabet
**                 has been excluded from the upper, lower, title, ... tables.
**                 Including Deseret codepoints would have doubled all tables for
**                 dubious benefit.
**
**              o) Some details are not fixed, like handling (i.e. shutting off)
**                 the case-sensitive LIKE pragma.
**
**
**
**  Usage
**  =====
**
**      Loading
**      -------
**
**            From C:
**            ^^^^^^^
**                    use sqlite3_load_extension() to load on a connection basis
**                         and the standard sqlite3_extension_init entry point.
**
**         |          use sqlite3_auto_extension() to perform auto loading with
**         |               the standard sqlite3_extension_init entry point.
**         |               This requires that your application loads unifuzz.dll
**         |               and obtain the actual address of the entry point.
**         |               Be aware that the extension will only be available to
**         |               subsequent connections and that it will be effective
**         |               for _each_ of them.
**
**         |          use sqlite3_auto_extension() to perform auto loading with
**         |               the auto_load entry point.  Version >= 0.3.0 don't
**         |               ship with this feature enabled: now needs recompile
**         |               with AUTOLOAD_KLUDGE defined. This causes a dependancy
**         |               on sqlite3.dll.
**
**            From a third-party DB manager, wrappers or other SQL application:
**            ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
**                    use "select load_extension('unifuzz.dll');"
**                         to load the extension for the current DB connection only.
**
**         |          use "select load_extension('unifuzz.dll', 'auto_load');"
**         |               to load as auto_loaded extension available to each
**         |               subsequently opened DB in the session. Versions >= 0.3.0
**         |               don't ship with this feature enabled and need recompile
**         |               with AUTOLOAD_KLUDGE defined. This causes a dependancy
**         |               on sqlite3.dll.
**
**
**      ~~~~~~~~~~~~~~
**      Function names
**      ~~~~~~~~~~~~~~
**         |
**         |         A couple of #define options let you choose to override or not
**         |     the native scalar functions or collation.  By default, all SQLite
**         |     functions are overriden and the new functions carry the base name
**         |     with NO letter U postfixed, so the description below applies to
**         |     UPPER, LOWER, LIKE, GLOB, and NOCASE.
**         |
**         |     There is no facility in the SQLite library to declare an operator
**         |     as available in the infix notation.  Only the native names can be
**         |     using this feature.  This means that if you choose to have this
**         |     extension preserve the native functions, the only way to invoke
**         |     the LIKEU and GLOBU functions is using the generic form for calls
**         |
**         |         ... LIKEU(pattern, string [, escape_char]) ...
**         |
**         |     as opposed to the infix (more common) notation.
**         |
**         |
**         |     Defining UNIFUZZ_OVERRIDE_SCALARS makes the new scalar functions
**         |     override corresponding native SQLite scalars functions.
**         |
**         |     Defining UNIFUZZ_OVERRIDE_NOCASE makes the new collation override
**         |     the native NOCASE collation.
**
**
**      Scalar functions
**      ----------------
**
**            UPPER(str)
**            UPPERU(str)
**                  returns a Unicode uppercase version of str
**
**            LOWER(str)
**            LOWERU(str)
**                  returns a Unicode lowercase version of str
**
**            UNACCENT(str)
**                  returns a Unicode unaccented version of str
**
**            FOLD(str)
**                  returns a Unicode folded version of str
**
**            str2 LIKE str1 [ESCAPE esc]
**            LIKE(str1, str2 [, esc])
**            LIKEU(str1, str2 [, esc])
**                  behaves the same as the built-in LIKE, but handles Unicode:
**                  same as LIKE(unaccent(fold(str1)), unaccent(fold(str2))[, esc])
**
**            str2 GLOB str1
**            GLOB(str1, str2)
**            GLOBU(str1, str2)
**                  behaves the same as the built-in GLOB, but handles Unicode:
**                  same as GLOB(unaccent(str1), unaccent(str2))
**                  It is a byproduct of LIKE.
**
**            TYPOS(str1, str2)
**                  returns the "Damerau-Levenshtein distance" between fold(str1) and
**                  fold(str2).  This is the number of insertions, omissions, changes
**                  and transpositions (of adjacent letters only). Similarily to
**                  the overriding LIKE above, it works on Unicode strings by
**                  making them unaccented & lowercase.
**
**                  If the reference string is 'abcdef', it will return 1 (one typo) for
**                        'abdef'        missing c
**                        'abcudef'      u inserted
**                        'abzef'        c changed into z
**                        'abdcef'       c & d exchanged
**
**                  Only one level of "typo" is considered, e.g. the function will
**                  consider the following transformations to be 3 typos:
**                        'abcdef'       reference
**                        'abdcef'       c & d exchanged
**                        'abdzcef'      z inserted inside (c & d exchanged)
**                  In this case, it will return 3.  Technically, it does not
**                  always return the minimum edit distance and doesn't satisfy
**                  the "triangle inequality" in all cases.  It is nonetheless
**                  very useful to anyone having to lookup simple entry subject to
**                  user typo (e.g. name or city name) in a SQLite table where data
**                  is likely or know to use non-ASCII characters.
**
**                  It will also accept '_' and a trailing '%' in str2, both acting
**                  as in LIKE.
**
**                  You can use it this way:
**                        select * from t where typos(col, 'leivencht%') <= 2;
**                        select * from t where typos(name, inputname) <= 2;
**                  or this way:
**                        select typos(str1, str2)
**
**                  NOTE: the implementation may seem naive but is open to several
**                        evolutions.  Due to the complexity in O(n*m) you
**                        should reserve its use to _short_ fields only.  There
**                        are much better algorithms for large fields (most of
**                        which are terrible for small strings.)  The choice made
**                        reflects the typical need to match names, surnames,
**                        street addresses, cities or such data prone to typos
**                        in user input.  Flexibility has been choosen over mere
**                        performance, because fuzzy search is _slow_ anyway.
**                        So you better have a 380% slower algo that retrieves
**                        the rows you're after, than a 100% slow algo that misses
**                        them most of the times.
**
**                  LIMIT There is a limit to the character length of arguments
**                        that TYPOS will handle.  The limit is more precisely
**                        on the _product_ of both length.  Anything near the
**                        default limit will take forever, due to the O(n*m)
**                        algorithm.
**
**         |        DO NOT use TYPOS in case LIKE would do!  for instance, if
**         |        you want rows that contain a fixed substring (without typo),
**         |        then use:
**         |            select * from t where cityname LIKE '%angel%';
**         |        It will match 'Los Angeles' without question.  If you try:
**         |            select * from t where typos(cityname, 'angel%') <= 4;
**         |        you will be overhelmed with rows from everywhere, since up
**         |        to 4 typos allows for typically _many_ values (cities, here).
**
**
**            FLIP(str)
**                  returns a flipped (reversed) version of str without regards to
**         |        decomposed chars or graphemes.  May yield nonsense when applied
**         |        on strings containing decomposed characters, for scripts using
**         |        unbreakable multi-characters graphemes or multi-directional strings.
**
**            STRPOS(str1, str2)
**            STRPOS(str1, str2, nbr)
**                  returns the position of the <nbr>th occurence of str2 within str1.
**                  By default, or when nbr = 0, nbr = 1.  Negative values of nbr count
**                  occurences from the end of str1.  Returned position is 1-based
**                  (first character of str1 is at 1) and is 0 if str2 is not found.
**
**            STRDUP(str, count)
**                  returns concatenation of count times the string str.  count must
**                  be a non-negative integer.  If count is zero, an empty string is
**                  returned; if count is negative, null is returned.
**
**            STRFILTER(str1, str2)
**                  returns str1 with characters NOT in str2 removed.
**
**            STRTABOO(str1, str2)
**                  returns str1 with characters IN str2 removed.
**
**
**
**
**      Collation functions
**      -------------------
**
**         |        These collations are just wrappers around a Windows call!
**         |        The current implementation should work consistently on all
**         |        Windows 32-64 versions, as long as MS doesn't change the
**         |        behavior of this function (which is rather unlikely).
**         |
**         |        Any index built on one of these collations will have to be
**         |        rebuilt if ever the Windows function used changes!
**         |
**         |        Observe that since the collations compare modified data
**         |        (unaccented strings for NOCASE[U] and NAMES, numeric part
**         |        only for NUMERICS), it's unsafe to put a UNIQUE constraint
**         |        on indices using these collations.  This is nothing new: the
**         |        same remark applies to the built-in NOCASE as well.
**
**
**            NOCASE
**            NOCASEU
**            RMNOCASE
**                  this basic function overrides the built-in function and
**                  handles Unicode strings using the CompareStringW Win32
**                  call with "locale-invariant" and IGNORECASE parameters.
**                  If you need locale-specific collation with the highly specific
**                  behavior for a locale, then you should probably use ICU instead.
**                  But then be aware that the locale used for indexing is a metadata
**                  of the table as well as the ICU and Unicode versions!
**
**            UNACCENTED
**                  handles Unicode strings using the CompareStringW Win32
**                  call with "locale-independant" parameter similarly to NOCASE
**                  but also ignores diacritics.
**
**            NAMES
**                  this collation uses the "word order" option explained on the
**                  MSDN page for CompareStringW(): hyphens and single quotes are
**                  ignored, as well as punctuation and symbols.
**                  Should be tested for validity with uncommon scripts.
**                  See http://msdn.microsoft.com/en-us/library/ms647476(VS.85).aspx
**
**            NUMERICS
**                  this collation performs a numerical sort on text values similar
**                  to what _atoi64 would give, but converts all forms of Unicode
**                  decimal digits codepoints to their decimal equivalent.  This
**                  conversion accepts leading whitespaces (all sorts of Unicode
**                  whitespaces are OK), one optional sign (all sorts of + and -
**                  are OK) then optional numeric part (all sorts of Unicode digits
**                  are OK) but it doesn't check for signed 64-bit integer overflow.
**                  Conversion stops at the first non-digit.  Nulls yield zero.
**                  In cases where numeric prefixes yield the same value, then the
**                  non-numeric part of the strings, if any, are then compared
**                  with NOCASE collation.
**
**
**      Convenience functions
**      ---------------------
**
**            HEXW(number)
**            HEXW('number')
**                  Returns the hexadecimal representation of <number>.
**
**
**            ASCW(string [, index])
**                  Returns the numeric value of the Unicode codepoint of the
**                  character of <string> at position <index>. First character
**                  is at index 1, last character at -1. Yields null for invalid
**                  index values. Default index is 1.
**
**
**            CHRW(decimal_number)
**            CHRW('decimal_number')
**            CHRW(x'hexadecimal_number')
**                  Returns a string containing the Unicode character whose code
**                  point numeric value was passed as argument.  Non-characters
**                  codepoints are rendered as U+00FFFD (invalid character).
**
**
**            PRINTF(formatstr, p1, p2, ..., pn)
**                  This is a [dirty] wrapper around sqlite3_mprintf().  Use it in
**                  your SQL just like any scalar function returning a string.
**
**                  SELECT printf('format %lli %g %s %s', 123, 4.56, 'abc', null);
**
**                  will return:     'format 123 4.56 abc <NULL>'
**
**                  Valid format specifications include:
**
**                     %ll[diouxX]  64-bit integer    (don't omit the ll modifier!)
**                     %[eEfgG]     double
**                     %s           string
**                     %s           SQL null          will print as '<NULL>'
**                     %q %Q %w     escaping specs    see sqlite3 docs
**                     %%           percent
**
**          |       NEVER use %p, %c, or variations: this will crash your application.
**
**                  Refer to the SQlite documentation or source code for width and
**                  precision specifications.
**
**
**            UNIFUZZ()
**					Returns a string containing the Unicode tries and the unifuzz extension
**					version numbers.
**
**
**  Compilation
**  ===========
**      This code compiles fine with MinGW gcc (preferred), VC++ Express 2008 and tcc.
**
**      gcc -O2 -lm -shared unifuzz.c -o unifuzz.dll
**      tcc -shared unifuzz.c -ounifuzz.dll
**
**
**    I hope someone will find it useful.  It would be nice to have reports from users
**    in various countries or using various scripts.  Even if you choose to not use it
**    your feedback is valuable.
**
**    Please send any feedback at:
**                        "Jean-Christophe Deschamps" <jcd@antichoc.net>
**
*/

#ifdef __cplusplus
extern "C" {
#endif


// #include "../sqlite3ext.h"
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1

#include <assert.h>
#ifndef WIN32
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winnls.h"
#endif

#ifndef NO_WINDOWS_COLLATION
#ifdef WIN32
#include <windows.h>
#endif
#endif

#ifndef SQLITE_PRIVATE
# define SQLITE_PRIVATE static
#endif


#define DLL_EXPORT __declspec(dllexport)
#define DLL_IMPORT __declspec(dllimport)

#define UNUSED_PARAMETER(x) (void)(x)



// Select preferred encoding:
//
// Choose a set of scalar functions for the preferred UTF encoding, or both.
//
// Whatever your choice is, you can still use this library with any base since
// SQLite will perform the necessary conversions for you transparently.
//
// The whole purpose of offering both UTF-8 and UTF-16 versions is to avoid these
// conversions.  When, for instance, you invoke a function with a base encoded in
// UTF-16 then SQLite will select the version of this function said to be optimized
// for handling / returning UTF-16 parameters without extra conversion.
//
// Of course only one of the two set of function is required but if you happen to
// have both UTF-8 and UTF-16 bases, then you can find it more efficient to have
// both sets of functions handy.
#if !defined(UNIFUZZ_UTF8) && !defined(UNIFUZZ_UTF16) && !defined(UNIFUZZ_UTF_BOTH)
# define UNIFUZZ_UTF_BOTH
#endif


// By default, this library does overrides the native scalar functions
// UPPER, LOWER, LIKE and GLOB.  The following define lets you choose to
// leave the native functions available and have the new ones suffixed by U:
// UPPERU, LOWERU, LIKEU and GLOBU.
//
// You can choose to override them by defining UNIFUZZ_OVERRIDE_SCALARS
//
// Similarily, the NOCASE collation sequence is overriden by default.
//
// You can choose to override it by defining UNIFUZZ_OVERRIDE_NOCASE
// and the new collation calls NOCASEU.
//
// You can of course choose to define those symbols at the compiler command line
// instead of having them hardcoded here.

#define UNIFUZZ_OVERRIDE_SCALARS

#define UNIFUZZ_OVERRIDE_NOCASE




/*
** Integers of known sizes.  These typedefs might change for architectures
** where the sizes vary.  Preprocessor macros are available so that the
** types can be conveniently redefined at compile-type.  Like this:
**
**         cc '-DUINTPTR_TYPE=long long int' ...
*/
#ifndef UINT32_TYPE
# ifdef HAVE_UINT32_T
#  define UINT32_TYPE uint32_t
# else
#  define UINT32_TYPE unsigned int
# endif
#endif
#ifndef INT32_TYPE
# ifdef HAVE_INT32_T
#  define INT32_TYPE int32_t
# else
#  define INT32_TYPE int
# endif
#endif
#ifndef UINT16_TYPE
# ifdef HAVE_UINT16_T
#  define UINT16_TYPE uint16_t
# else
#  define UINT16_TYPE unsigned short int
# endif
#endif
#ifndef INT16_TYPE
# ifdef HAVE_INT16_T
#  define INT16_TYPE int16_t
# else
#  define INT16_TYPE short int
# endif
#endif
#ifndef UINT8_TYPE
# ifdef HAVE_UINT8_T
#  define UINT8_TYPE uint8_t
# else
#  define UINT8_TYPE unsigned char
# endif
#endif
#ifndef INT8_TYPE
# ifdef HAVE_INT8_T
#  define INT8_TYPE int8_t
# else
#  define INT8_TYPE signed char
# endif
#endif
#ifndef LONGDOUBLE_TYPE
# define LONGDOUBLE_TYPE long double
#endif

typedef sqlite_int64  i64;         /* 8-byte   signed integer */
typedef sqlite_uint64 u64;         /* 8-byte unsigned integer */
typedef  INT32_TYPE   i32;         /* 4-byte   signed integer */
typedef UINT32_TYPE   u32;         /* 4-byte unsigned integer */
typedef  INT16_TYPE   i16;         /* 2-byte   signed integer */
typedef UINT16_TYPE   u16;         /* 2-byte unsigned integer */
typedef  INT8_TYPE     i8;         /* 1-byte   signed integer */
typedef UINT8_TYPE     u8;         /* 1-byte unsigned integer */


/*
** This library sometimes has to deal with string expansion, e.g.
** apply a function to an input string which gives a longer output
** string in terms of number of Unicode characters.  This is only
** discovered on the fly.
** To avoid having to realloc the output string every times this
** occurs for _one_ extra output position, the library uses a
** "chunked" allocation strategy.
**
** A "chunk" is a predefined small number of output positions which
** determines the increment by which output string grow when needed
** during some operations.
** First, the output string is allocated one extra chunk.  When this
** number of extra positions have been used the string is realloc'ed
** with one more chunk, until operation ends.
**
** The output of scalar functions are used and freed quickly, so it
** shouldn't be a problem if we allocate a block of memory slightly
** larger than absolutely necessary, even on embedded systems.
**
** You can increase the size from its default value of 16 to a larger
** value if you handle strings containing more than a few characters
** subject to expansion.
**
** DO NOT set UNIFUZZ_CHUNK below 8.
**
** Most applications should use the default value of 16.
**
*/
#ifndef UNIFUZZ_CHUNK
# define UNIFUZZ_CHUNK 16
#endif


/*
** This is the maximum square size of the virtual 2D array on which
** TYPOS is allowed to work.  Anything anywhere near this size is
** unreasonable and should definitely be handled otherwise.
**
** This is computed as the product of the maximum character length of
** the two arguments.  Reminder: each character is one UTF-32.  TYPOS
** only allocates three rows, but the algorithm complexity is directly
** proportional to the product of the length of both arguments.  Don't
** forget it will run for every _row_ in the rowset.  Experiment first!
*/
#ifndef   UNIFUZZ_TYPOS_LIMIT
# define  UNIFUZZ_TYPOS_LIMIT   (1024 * 1024)
#endif



/* Generated by builder. Do not modify. Start version_defines */
#define UNICODE_VERSION_MAJOR        5
#define UNICODE_VERSION_MINOR        1
#define UNICODE_VERSION_MICRO        0
#define UNICODE_VERSION_BUILD        10

#define __UNICODE_VERSION_STRING(a,b,c,d) #a "." #b "." #c "." #d
#define _UNICODE_VERSION_STRING(a,b,c,d)  __UNICODE_VERSION_STRING(a,b,c,d)
#define UNICODE_VERSION_STRING            _UNICODE_VERSION_STRING(UNICODE_VERSION_MAJOR,UNICODE_VERSION_MINOR,UNICODE_VERSION_MICRO,UNICODE_VERSION_BUILD)


#define __VERSION_STRINGS(a, b)     #a b
#define _VERSION_STRINGS(a, b)      __VERSION_STRINGS(a, b)
#define VERSION_STRINGS             _VERSION_STRINGS(UNIFUZZ_VERSION, UNICODE_VERSION_STRING)



/* Generated by builder. Do not modify. Start fold_defines */
#define FOLD_BLOCK_SHIFT 5
#define FOLD_BLOCK_MASK ((1 << FOLD_BLOCK_SHIFT) - 1)
#define FOLD_BLOCK_SIZE (1 << FOLD_BLOCK_SHIFT)
#define FOLD_BLOCK_COUNT 69
#define FOLD_INDEXES_SIZE (0x10000 >> FOLD_BLOCK_SHIFT)
/* Generated by builder. Do not modify. End fold_defines */


/* Generated by builder. Do not modify. Start fold_tables */
static const u16 fold_indexes[FOLD_INDEXES_SIZE] = {
   0,   0,   1,   0,   0,   2,   3,   0,   4,   5,   6,   7,   8,   9,  10,
  11,  12,  13,  14,   0,   0,   0,   0,   0,   0,   0,  15,  16,  17,  18,
  19,  20,  21,  22,   0,  23,  24,  25,  26,  27,  28,  29,  30,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  31,  32,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,
  48,   0,   0,   0,   0,   0,   0,   0,   0,   0,  49,   0,  50,  51,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,  52,  53,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,  54,  55,   0,  56,  57,  58,  59,  60,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  61,  62,  63,   0,   0,
   0,   0,  64,  65,  66,  67,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,  68,   0,   0,   0,   0,   0,   0
};

static const u16 fold_data0[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data1[]  = { 0x0000, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data2[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x03BC, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data3[]  = { 0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF, 0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x0000, 0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x0000 };
static const u16 fold_data4[]  = { 0x0101, 0x0000, 0x0103, 0x0000, 0x0105, 0x0000, 0x0107, 0x0000, 0x0109, 0x0000, 0x010B, 0x0000, 0x010D, 0x0000, 0x010F, 0x0000, 0x0111, 0x0000, 0x0113, 0x0000, 0x0115, 0x0000, 0x0117, 0x0000, 0x0119, 0x0000, 0x011B, 0x0000, 0x011D, 0x0000, 0x011F, 0x0000 };
static const u16 fold_data5[]  = { 0x0121, 0x0000, 0x0123, 0x0000, 0x0125, 0x0000, 0x0127, 0x0000, 0x0129, 0x0000, 0x012B, 0x0000, 0x012D, 0x0000, 0x012F, 0x0000, 0x0000, 0x0000, 0x0133, 0x0000, 0x0135, 0x0000, 0x0137, 0x0000, 0x0000, 0x013A, 0x0000, 0x013C, 0x0000, 0x013E, 0x0000, 0x0140 };
static const u16 fold_data6[]  = { 0x0000, 0x0142, 0x0000, 0x0144, 0x0000, 0x0146, 0x0000, 0x0148, 0x0000, 0x0000, 0x014B, 0x0000, 0x014D, 0x0000, 0x014F, 0x0000, 0x0151, 0x0000, 0x0153, 0x0000, 0x0155, 0x0000, 0x0157, 0x0000, 0x0159, 0x0000, 0x015B, 0x0000, 0x015D, 0x0000, 0x015F, 0x0000 };
static const u16 fold_data7[]  = { 0x0161, 0x0000, 0x0163, 0x0000, 0x0165, 0x0000, 0x0167, 0x0000, 0x0169, 0x0000, 0x016B, 0x0000, 0x016D, 0x0000, 0x016F, 0x0000, 0x0171, 0x0000, 0x0173, 0x0000, 0x0175, 0x0000, 0x0177, 0x0000, 0x00FF, 0x017A, 0x0000, 0x017C, 0x0000, 0x017E, 0x0000, 0x0073 };
static const u16 fold_data8[]  = { 0x0000, 0x0253, 0x0183, 0x0000, 0x0185, 0x0000, 0x0254, 0x0188, 0x0000, 0x0256, 0x0257, 0x018C, 0x0000, 0x0000, 0x01DD, 0x0259, 0x025B, 0x0192, 0x0000, 0x0260, 0x0263, 0x0000, 0x0269, 0x0268, 0x0199, 0x0000, 0x0000, 0x0000, 0x026F, 0x0272, 0x0000, 0x0275 };
static const u16 fold_data9[]  = { 0x01A1, 0x0000, 0x01A3, 0x0000, 0x01A5, 0x0000, 0x0280, 0x01A8, 0x0000, 0x0283, 0x0000, 0x0000, 0x01AD, 0x0000, 0x0288, 0x01B0, 0x0000, 0x028A, 0x028B, 0x01B4, 0x0000, 0x01B6, 0x0000, 0x0292, 0x01B9, 0x0000, 0x0000, 0x0000, 0x01BD, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data10[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x01C6, 0x01C6, 0x0000, 0x01C9, 0x01C9, 0x0000, 0x01CC, 0x01CC, 0x0000, 0x01CE, 0x0000, 0x01D0, 0x0000, 0x01D2, 0x0000, 0x01D4, 0x0000, 0x01D6, 0x0000, 0x01D8, 0x0000, 0x01DA, 0x0000, 0x01DC, 0x0000, 0x0000, 0x01DF, 0x0000 };
static const u16 fold_data11[] = { 0x01E1, 0x0000, 0x01E3, 0x0000, 0x01E5, 0x0000, 0x01E7, 0x0000, 0x01E9, 0x0000, 0x01EB, 0x0000, 0x01ED, 0x0000, 0x01EF, 0x0000, 0x0000, 0x01F3, 0x01F3, 0x0000, 0x01F5, 0x0000, 0x0195, 0x01BF, 0x01F9, 0x0000, 0x01FB, 0x0000, 0x01FD, 0x0000, 0x01FF, 0x0000 };
static const u16 fold_data12[] = { 0x0201, 0x0000, 0x0203, 0x0000, 0x0205, 0x0000, 0x0207, 0x0000, 0x0209, 0x0000, 0x020B, 0x0000, 0x020D, 0x0000, 0x020F, 0x0000, 0x0211, 0x0000, 0x0213, 0x0000, 0x0215, 0x0000, 0x0217, 0x0000, 0x0219, 0x0000, 0x021B, 0x0000, 0x021D, 0x0000, 0x021F, 0x0000 };
static const u16 fold_data13[] = { 0x019E, 0x0000, 0x0223, 0x0000, 0x0225, 0x0000, 0x0227, 0x0000, 0x0229, 0x0000, 0x022B, 0x0000, 0x022D, 0x0000, 0x022F, 0x0000, 0x0231, 0x0000, 0x0233, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2C65, 0x023C, 0x0000, 0x019A, 0x2C66, 0x0000 };
static const u16 fold_data14[] = { 0x0000, 0x0242, 0x0000, 0x0180, 0x0289, 0x028C, 0x0247, 0x0000, 0x0249, 0x0000, 0x024B, 0x0000, 0x024D, 0x0000, 0x024F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data15[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x03B9, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data16[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0371, 0x0000, 0x0373, 0x0000, 0x0000, 0x0000, 0x0377, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data17[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x03AC, 0x0000, 0x03AD, 0x03AE, 0x03AF, 0x0000, 0x03CC, 0x0000, 0x03CD, 0x03CE, 0x0000, 0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7, 0x03B8, 0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF };
static const u16 fold_data18[] = { 0x03C0, 0x03C1, 0x0000, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7, 0x03C8, 0x03C9, 0x03CA, 0x03CB, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data19[] = { 0x0000, 0x0000, 0x03C3, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x03D7, 0x03B2, 0x03B8, 0x0000, 0x0000, 0x0000, 0x03C6, 0x03C0, 0x0000, 0x03D9, 0x0000, 0x03DB, 0x0000, 0x03DD, 0x0000, 0x03DF, 0x0000 };
static const u16 fold_data20[] = { 0x03E1, 0x0000, 0x03E3, 0x0000, 0x03E5, 0x0000, 0x03E7, 0x0000, 0x03E9, 0x0000, 0x03EB, 0x0000, 0x03ED, 0x0000, 0x03EF, 0x0000, 0x03BA, 0x03C1, 0x0000, 0x0000, 0x03B8, 0x03B5, 0x0000, 0x03F8, 0x0000, 0x03F2, 0x03FB, 0x0000, 0x0000, 0x037B, 0x037C, 0x037D };
static const u16 fold_data21[] = { 0x0450, 0x0451, 0x0452, 0x0453, 0x0454, 0x0455, 0x0456, 0x0457, 0x0458, 0x0459, 0x045A, 0x045B, 0x045C, 0x045D, 0x045E, 0x045F, 0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F };
static const u16 fold_data22[] = { 0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data23[] = { 0x0461, 0x0000, 0x0463, 0x0000, 0x0465, 0x0000, 0x0467, 0x0000, 0x0469, 0x0000, 0x046B, 0x0000, 0x046D, 0x0000, 0x046F, 0x0000, 0x0471, 0x0000, 0x0473, 0x0000, 0x0475, 0x0000, 0x0477, 0x0000, 0x0479, 0x0000, 0x047B, 0x0000, 0x047D, 0x0000, 0x047F, 0x0000 };
static const u16 fold_data24[] = { 0x0481, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x048B, 0x0000, 0x048D, 0x0000, 0x048F, 0x0000, 0x0491, 0x0000, 0x0493, 0x0000, 0x0495, 0x0000, 0x0497, 0x0000, 0x0499, 0x0000, 0x049B, 0x0000, 0x049D, 0x0000, 0x049F, 0x0000 };
static const u16 fold_data25[] = { 0x04A1, 0x0000, 0x04A3, 0x0000, 0x04A5, 0x0000, 0x04A7, 0x0000, 0x04A9, 0x0000, 0x04AB, 0x0000, 0x04AD, 0x0000, 0x04AF, 0x0000, 0x04B1, 0x0000, 0x04B3, 0x0000, 0x04B5, 0x0000, 0x04B7, 0x0000, 0x04B9, 0x0000, 0x04BB, 0x0000, 0x04BD, 0x0000, 0x04BF, 0x0000 };
static const u16 fold_data26[] = { 0x04CF, 0x04C2, 0x0000, 0x04C4, 0x0000, 0x04C6, 0x0000, 0x04C8, 0x0000, 0x04CA, 0x0000, 0x04CC, 0x0000, 0x04CE, 0x0000, 0x0000, 0x04D1, 0x0000, 0x04D3, 0x0000, 0x04D5, 0x0000, 0x04D7, 0x0000, 0x04D9, 0x0000, 0x04DB, 0x0000, 0x04DD, 0x0000, 0x04DF, 0x0000 };
static const u16 fold_data27[] = { 0x04E1, 0x0000, 0x04E3, 0x0000, 0x04E5, 0x0000, 0x04E7, 0x0000, 0x04E9, 0x0000, 0x04EB, 0x0000, 0x04ED, 0x0000, 0x04EF, 0x0000, 0x04F1, 0x0000, 0x04F3, 0x0000, 0x04F5, 0x0000, 0x04F7, 0x0000, 0x04F9, 0x0000, 0x04FB, 0x0000, 0x04FD, 0x0000, 0x04FF, 0x0000 };
static const u16 fold_data28[] = { 0x0501, 0x0000, 0x0503, 0x0000, 0x0505, 0x0000, 0x0507, 0x0000, 0x0509, 0x0000, 0x050B, 0x0000, 0x050D, 0x0000, 0x050F, 0x0000, 0x0511, 0x0000, 0x0513, 0x0000, 0x0515, 0x0000, 0x0517, 0x0000, 0x0519, 0x0000, 0x051B, 0x0000, 0x051D, 0x0000, 0x051F, 0x0000 };
static const u16 fold_data29[] = { 0x0521, 0x0000, 0x0523, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0561, 0x0562, 0x0563, 0x0564, 0x0565, 0x0566, 0x0567, 0x0568, 0x0569, 0x056A, 0x056B, 0x056C, 0x056D, 0x056E, 0x056F };
static const u16 fold_data30[] = { 0x0570, 0x0571, 0x0572, 0x0573, 0x0574, 0x0575, 0x0576, 0x0577, 0x0578, 0x0579, 0x057A, 0x057B, 0x057C, 0x057D, 0x057E, 0x057F, 0x0580, 0x0581, 0x0582, 0x0583, 0x0584, 0x0585, 0x0586, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data31[] = { 0x2D00, 0x2D01, 0x2D02, 0x2D03, 0x2D04, 0x2D05, 0x2D06, 0x2D07, 0x2D08, 0x2D09, 0x2D0A, 0x2D0B, 0x2D0C, 0x2D0D, 0x2D0E, 0x2D0F, 0x2D10, 0x2D11, 0x2D12, 0x2D13, 0x2D14, 0x2D15, 0x2D16, 0x2D17, 0x2D18, 0x2D19, 0x2D1A, 0x2D1B, 0x2D1C, 0x2D1D, 0x2D1E, 0x2D1F };
static const u16 fold_data32[] = { 0x2D20, 0x2D21, 0x2D22, 0x2D23, 0x2D24, 0x2D25, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data33[] = { 0x1E01, 0x0000, 0x1E03, 0x0000, 0x1E05, 0x0000, 0x1E07, 0x0000, 0x1E09, 0x0000, 0x1E0B, 0x0000, 0x1E0D, 0x0000, 0x1E0F, 0x0000, 0x1E11, 0x0000, 0x1E13, 0x0000, 0x1E15, 0x0000, 0x1E17, 0x0000, 0x1E19, 0x0000, 0x1E1B, 0x0000, 0x1E1D, 0x0000, 0x1E1F, 0x0000 };
static const u16 fold_data34[] = { 0x1E21, 0x0000, 0x1E23, 0x0000, 0x1E25, 0x0000, 0x1E27, 0x0000, 0x1E29, 0x0000, 0x1E2B, 0x0000, 0x1E2D, 0x0000, 0x1E2F, 0x0000, 0x1E31, 0x0000, 0x1E33, 0x0000, 0x1E35, 0x0000, 0x1E37, 0x0000, 0x1E39, 0x0000, 0x1E3B, 0x0000, 0x1E3D, 0x0000, 0x1E3F, 0x0000 };
static const u16 fold_data35[] = { 0x1E41, 0x0000, 0x1E43, 0x0000, 0x1E45, 0x0000, 0x1E47, 0x0000, 0x1E49, 0x0000, 0x1E4B, 0x0000, 0x1E4D, 0x0000, 0x1E4F, 0x0000, 0x1E51, 0x0000, 0x1E53, 0x0000, 0x1E55, 0x0000, 0x1E57, 0x0000, 0x1E59, 0x0000, 0x1E5B, 0x0000, 0x1E5D, 0x0000, 0x1E5F, 0x0000 };
static const u16 fold_data36[] = { 0x1E61, 0x0000, 0x1E63, 0x0000, 0x1E65, 0x0000, 0x1E67, 0x0000, 0x1E69, 0x0000, 0x1E6B, 0x0000, 0x1E6D, 0x0000, 0x1E6F, 0x0000, 0x1E71, 0x0000, 0x1E73, 0x0000, 0x1E75, 0x0000, 0x1E77, 0x0000, 0x1E79, 0x0000, 0x1E7B, 0x0000, 0x1E7D, 0x0000, 0x1E7F, 0x0000 };
static const u16 fold_data37[] = { 0x1E81, 0x0000, 0x1E83, 0x0000, 0x1E85, 0x0000, 0x1E87, 0x0000, 0x1E89, 0x0000, 0x1E8B, 0x0000, 0x1E8D, 0x0000, 0x1E8F, 0x0000, 0x1E91, 0x0000, 0x1E93, 0x0000, 0x1E95, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1E61, 0x0000, 0x0000, 0x00DF, 0x0000 };
static const u16 fold_data38[] = { 0x1EA1, 0x0000, 0x1EA3, 0x0000, 0x1EA5, 0x0000, 0x1EA7, 0x0000, 0x1EA9, 0x0000, 0x1EAB, 0x0000, 0x1EAD, 0x0000, 0x1EAF, 0x0000, 0x1EB1, 0x0000, 0x1EB3, 0x0000, 0x1EB5, 0x0000, 0x1EB7, 0x0000, 0x1EB9, 0x0000, 0x1EBB, 0x0000, 0x1EBD, 0x0000, 0x1EBF, 0x0000 };
static const u16 fold_data39[] = { 0x1EC1, 0x0000, 0x1EC3, 0x0000, 0x1EC5, 0x0000, 0x1EC7, 0x0000, 0x1EC9, 0x0000, 0x1ECB, 0x0000, 0x1ECD, 0x0000, 0x1ECF, 0x0000, 0x1ED1, 0x0000, 0x1ED3, 0x0000, 0x1ED5, 0x0000, 0x1ED7, 0x0000, 0x1ED9, 0x0000, 0x1EDB, 0x0000, 0x1EDD, 0x0000, 0x1EDF, 0x0000 };
static const u16 fold_data40[] = { 0x1EE1, 0x0000, 0x1EE3, 0x0000, 0x1EE5, 0x0000, 0x1EE7, 0x0000, 0x1EE9, 0x0000, 0x1EEB, 0x0000, 0x1EED, 0x0000, 0x1EEF, 0x0000, 0x1EF1, 0x0000, 0x1EF3, 0x0000, 0x1EF5, 0x0000, 0x1EF7, 0x0000, 0x1EF9, 0x0000, 0x1EFB, 0x0000, 0x1EFD, 0x0000, 0x1EFF, 0x0000 };
static const u16 fold_data41[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F00, 0x1F01, 0x1F02, 0x1F03, 0x1F04, 0x1F05, 0x1F06, 0x1F07, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F10, 0x1F11, 0x1F12, 0x1F13, 0x1F14, 0x1F15, 0x0000, 0x0000 };
static const u16 fold_data42[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F20, 0x1F21, 0x1F22, 0x1F23, 0x1F24, 0x1F25, 0x1F26, 0x1F27, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F30, 0x1F31, 0x1F32, 0x1F33, 0x1F34, 0x1F35, 0x1F36, 0x1F37 };
static const u16 fold_data43[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F40, 0x1F41, 0x1F42, 0x1F43, 0x1F44, 0x1F45, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F51, 0x0000, 0x1F53, 0x0000, 0x1F55, 0x0000, 0x1F57 };
static const u16 fold_data44[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F60, 0x1F61, 0x1F62, 0x1F63, 0x1F64, 0x1F65, 0x1F66, 0x1F67, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data45[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F80, 0x1F81, 0x1F82, 0x1F83, 0x1F84, 0x1F85, 0x1F86, 0x1F87, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F90, 0x1F91, 0x1F92, 0x1F93, 0x1F94, 0x1F95, 0x1F96, 0x1F97 };
static const u16 fold_data46[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1FA0, 0x1FA1, 0x1FA2, 0x1FA3, 0x1FA4, 0x1FA5, 0x1FA6, 0x1FA7, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1FB0, 0x1FB1, 0x1F70, 0x1F71, 0x1FB3, 0x0000, 0x03B9, 0x0000 };
static const u16 fold_data47[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F72, 0x1F73, 0x1F74, 0x1F75, 0x1FC3, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1FD0, 0x1FD1, 0x1F76, 0x1F77, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data48[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1FE0, 0x1FE1, 0x1F7A, 0x1F7B, 0x1FE5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F78, 0x1F79, 0x1F7C, 0x1F7D, 0x1FF3, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data49[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x03C9, 0x0000, 0x0000, 0x0000, 0x006B, 0x00E5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x214E, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data50[] = { 0x2170, 0x2171, 0x2172, 0x2173, 0x2174, 0x2175, 0x2176, 0x2177, 0x2178, 0x2179, 0x217A, 0x217B, 0x217C, 0x217D, 0x217E, 0x217F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data51[] = { 0x0000, 0x0000, 0x0000, 0x2184, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data52[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x24D0, 0x24D1, 0x24D2, 0x24D3, 0x24D4, 0x24D5, 0x24D6, 0x24D7, 0x24D8, 0x24D9 };
static const u16 fold_data53[] = { 0x24DA, 0x24DB, 0x24DC, 0x24DD, 0x24DE, 0x24DF, 0x24E0, 0x24E1, 0x24E2, 0x24E3, 0x24E4, 0x24E5, 0x24E6, 0x24E7, 0x24E8, 0x24E9, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data54[] = { 0x2C30, 0x2C31, 0x2C32, 0x2C33, 0x2C34, 0x2C35, 0x2C36, 0x2C37, 0x2C38, 0x2C39, 0x2C3A, 0x2C3B, 0x2C3C, 0x2C3D, 0x2C3E, 0x2C3F, 0x2C40, 0x2C41, 0x2C42, 0x2C43, 0x2C44, 0x2C45, 0x2C46, 0x2C47, 0x2C48, 0x2C49, 0x2C4A, 0x2C4B, 0x2C4C, 0x2C4D, 0x2C4E, 0x2C4F };
static const u16 fold_data55[] = { 0x2C50, 0x2C51, 0x2C52, 0x2C53, 0x2C54, 0x2C55, 0x2C56, 0x2C57, 0x2C58, 0x2C59, 0x2C5A, 0x2C5B, 0x2C5C, 0x2C5D, 0x2C5E, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data56[] = { 0x2C61, 0x0000, 0x026B, 0x1D7D, 0x027D, 0x0000, 0x0000, 0x2C68, 0x0000, 0x2C6A, 0x0000, 0x2C6C, 0x0000, 0x0251, 0x0271, 0x0250, 0x0000, 0x0000, 0x2C73, 0x0000, 0x0000, 0x2C76, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data57[] = { 0x2C81, 0x0000, 0x2C83, 0x0000, 0x2C85, 0x0000, 0x2C87, 0x0000, 0x2C89, 0x0000, 0x2C8B, 0x0000, 0x2C8D, 0x0000, 0x2C8F, 0x0000, 0x2C91, 0x0000, 0x2C93, 0x0000, 0x2C95, 0x0000, 0x2C97, 0x0000, 0x2C99, 0x0000, 0x2C9B, 0x0000, 0x2C9D, 0x0000, 0x2C9F, 0x0000 };
static const u16 fold_data58[] = { 0x2CA1, 0x0000, 0x2CA3, 0x0000, 0x2CA5, 0x0000, 0x2CA7, 0x0000, 0x2CA9, 0x0000, 0x2CAB, 0x0000, 0x2CAD, 0x0000, 0x2CAF, 0x0000, 0x2CB1, 0x0000, 0x2CB3, 0x0000, 0x2CB5, 0x0000, 0x2CB7, 0x0000, 0x2CB9, 0x0000, 0x2CBB, 0x0000, 0x2CBD, 0x0000, 0x2CBF, 0x0000 };
static const u16 fold_data59[] = { 0x2CC1, 0x0000, 0x2CC3, 0x0000, 0x2CC5, 0x0000, 0x2CC7, 0x0000, 0x2CC9, 0x0000, 0x2CCB, 0x0000, 0x2CCD, 0x0000, 0x2CCF, 0x0000, 0x2CD1, 0x0000, 0x2CD3, 0x0000, 0x2CD5, 0x0000, 0x2CD7, 0x0000, 0x2CD9, 0x0000, 0x2CDB, 0x0000, 0x2CDD, 0x0000, 0x2CDF, 0x0000 };
static const u16 fold_data60[] = { 0x2CE1, 0x0000, 0x2CE3, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data61[] = { 0xA641, 0x0000, 0xA643, 0x0000, 0xA645, 0x0000, 0xA647, 0x0000, 0xA649, 0x0000, 0xA64B, 0x0000, 0xA64D, 0x0000, 0xA64F, 0x0000, 0xA651, 0x0000, 0xA653, 0x0000, 0xA655, 0x0000, 0xA657, 0x0000, 0xA659, 0x0000, 0xA65B, 0x0000, 0xA65D, 0x0000, 0xA65F, 0x0000 };
static const u16 fold_data62[] = { 0x0000, 0x0000, 0xA663, 0x0000, 0xA665, 0x0000, 0xA667, 0x0000, 0xA669, 0x0000, 0xA66B, 0x0000, 0xA66D, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data63[] = { 0xA681, 0x0000, 0xA683, 0x0000, 0xA685, 0x0000, 0xA687, 0x0000, 0xA689, 0x0000, 0xA68B, 0x0000, 0xA68D, 0x0000, 0xA68F, 0x0000, 0xA691, 0x0000, 0xA693, 0x0000, 0xA695, 0x0000, 0xA697, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data64[] = { 0x0000, 0x0000, 0xA723, 0x0000, 0xA725, 0x0000, 0xA727, 0x0000, 0xA729, 0x0000, 0xA72B, 0x0000, 0xA72D, 0x0000, 0xA72F, 0x0000, 0x0000, 0x0000, 0xA733, 0x0000, 0xA735, 0x0000, 0xA737, 0x0000, 0xA739, 0x0000, 0xA73B, 0x0000, 0xA73D, 0x0000, 0xA73F, 0x0000 };
static const u16 fold_data65[] = { 0xA741, 0x0000, 0xA743, 0x0000, 0xA745, 0x0000, 0xA747, 0x0000, 0xA749, 0x0000, 0xA74B, 0x0000, 0xA74D, 0x0000, 0xA74F, 0x0000, 0xA751, 0x0000, 0xA753, 0x0000, 0xA755, 0x0000, 0xA757, 0x0000, 0xA759, 0x0000, 0xA75B, 0x0000, 0xA75D, 0x0000, 0xA75F, 0x0000 };
static const u16 fold_data66[] = { 0xA761, 0x0000, 0xA763, 0x0000, 0xA765, 0x0000, 0xA767, 0x0000, 0xA769, 0x0000, 0xA76B, 0x0000, 0xA76D, 0x0000, 0xA76F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xA77A, 0x0000, 0xA77C, 0x0000, 0x1D79, 0xA77F, 0x0000 };
static const u16 fold_data67[] = { 0xA781, 0x0000, 0xA783, 0x0000, 0xA785, 0x0000, 0xA787, 0x0000, 0x0000, 0x0000, 0x0000, 0xA78C, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 fold_data68[] = { 0x0000, 0xFF41, 0xFF42, 0xFF43, 0xFF44, 0xFF45, 0xFF46, 0xFF47, 0xFF48, 0xFF49, 0xFF4A, 0xFF4B, 0xFF4C, 0xFF4D, 0xFF4E, 0xFF4F, 0xFF50, 0xFF51, 0xFF52, 0xFF53, 0xFF54, 0xFF55, 0xFF56, 0xFF57, 0xFF58, 0xFF59, 0xFF5A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };

static const u16 *fold_data_table[FOLD_BLOCK_COUNT] = {
fold_data0,fold_data1,fold_data2,fold_data3,fold_data4,fold_data5,fold_data6,fold_data7,fold_data8,fold_data9,
fold_data10,fold_data11,fold_data12,fold_data13,fold_data14,fold_data15,fold_data16,fold_data17,fold_data18,fold_data19,
fold_data20,fold_data21,fold_data22,fold_data23,fold_data24,fold_data25,fold_data26,fold_data27,fold_data28,fold_data29,
fold_data30,fold_data31,fold_data32,fold_data33,fold_data34,fold_data35,fold_data36,fold_data37,fold_data38,fold_data39,
fold_data40,fold_data41,fold_data42,fold_data43,fold_data44,fold_data45,fold_data46,fold_data47,fold_data48,fold_data49,
fold_data50,fold_data51,fold_data52,fold_data53,fold_data54,fold_data55,fold_data56,fold_data57,fold_data58,fold_data59,
fold_data60,fold_data61,fold_data62,fold_data63,fold_data64,fold_data65,fold_data66,fold_data67,fold_data68
};
/* Generated by builder. Do not modify. End fold_tables */

SQLITE_PRIVATE u32 unifuzz_fold(
    u32 c
){
    u32 p;
    if (c >= 0x10000) return c;
    p = (fold_data_table[fold_indexes[(c) >> FOLD_BLOCK_SHIFT]][(c) & FOLD_BLOCK_MASK]);
    return (p == 0) ? c : p;
}

/* Generated by builder. Do not modify. Start unacc_defines */
#define UNACC_BLOCK_SHIFT 5
#define UNACC_BLOCK_MASK ((1 << UNACC_BLOCK_SHIFT) - 1)
#define UNACC_BLOCK_SIZE (1 << UNACC_BLOCK_SHIFT)
#define UNACC_BLOCK_COUNT 239
#define UNACC_INDEXES_SIZE (0x10000 >> UNACC_BLOCK_SHIFT)
/* Generated by builder. Do not modify. End unacc_defines */

/* Generated by builder. Do not modify. Start unacc_tables */
static const u16 unacc_indexes[UNACC_INDEXES_SIZE] = {
   0,   0,   0,   0,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,
  11,  12,  13,  14,  15,  16,  17,  18,  19,   0,   0,   0,  20,  21,  22,
  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,   0,   0,  35,
   0,   0,   0,   0,  36,   0,  37,  38,  39,  40,  41,   0,   0,  42,  43,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  44,  45,
   0,   0,   0,  46,  47,   0,  48,  49,   0,   0,   0,   0,   0,   0,   0,
  50,   0,  51,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,  52,   0,   0,   0,  53,  54,   0,
  55,   0,  56,  57,   0,   0,   0,   0,   0,  58,   0,   0,   0,   0,   0,
  59,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,  60,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,  61,  62,  63,  64,  65,  66,   0,   0,
  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,
  82,  83,  84,  85,  86,  87,  88,   0,   0,  89,  90,  91,  92,  93,  94,
  95,  96,  97,  98,  99, 100, 101, 102,   0, 103,   0, 104,   0, 105, 106,
   0, 107, 108,   0,   0,   0, 109, 110, 111, 112, 113,   0,   0,   0,   0,
   0, 114,   0, 115, 116,   0,   0,   0, 117,   0,   0,   0,   0,   0,   0,
   0,   0,   0, 118, 119,   0,   0,   0,   0,   0,   0,   0,   0, 120, 121,
 122,   0, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135,
 136,   0,   0,   0,   0,   0,   0,   0, 137, 138, 139,   0,   0,   0,   0,
   0,   0,   0, 140,   0,   0,   0,   0, 141,   0,   0,   0, 142,   0,   0,
 143, 144, 145, 146, 147, 148, 149, 150,   0, 151, 152, 153, 154, 155, 156,
 157, 158,   0, 159, 160, 161, 162,   0,   0,   0, 163, 164, 165, 166, 167,
 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 179,   0, 180,   0,   0,
   0,   0, 181, 182, 183,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 184, 185, 186,
 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198,   0, 199, 200,
 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215,
 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230,
 231, 232, 233, 234, 235, 236, 237, 238
};

// This table has been modified to take care of German Eszet (lower- and upper-case)
static const u16 unacc_positions[UNACC_BLOCK_COUNT][UNACC_BLOCK_SIZE + 1] = {
/*  0 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/*  1 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 14, 15, 16, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 35, 38, 41, 42 },
/*  2 */ { 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 34 },
/*  3 */ { 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33 },
/*  4 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/*  5 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 35 },
/*  6 */ { 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36 },
/*  7 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/*  8 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/*  9 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 10 */ { 0, 1, 2, 3, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41 },
/* 11 */ { 0, 1, 2, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 23, 25, 26, 27, 28, 29, 30, 31, 32, 33, 35, 37, 38, 39 },
/* 12 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 13 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 14 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 15 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 16 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 17 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 18 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 19 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 20 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 21 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 22 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 23 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 24 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 25 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 26 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 27 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 28 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 29 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 30 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 31 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 32 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 33 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 34 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 35 */ { 0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33 },
/* 36 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 37 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 23, 25, 27, 29, 30, 31, 32, 33, 34, 35, 36 },
/* 38 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 39 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 40 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 41 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 42 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 43 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 44 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 45 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 46 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 47 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 48 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 49 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 50 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 51 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 52 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 53 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 54 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 30, 32, 33, 34 },
/* 55 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 56 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 57 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 58 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 59 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 60 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 61 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 62 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33 },
/* 63 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 64 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 65 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 66 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 67 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 68 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 69 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 70 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 71 */ //{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 28, 29, 30, 31, 32, 33 },
/* 71 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 28, 29, 30, 31, 33, 34 },
/* 72 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 73 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 74 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 75 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 76 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 77 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 78 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 79 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 80 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 81 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 82 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 83 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 84 */ { 0, 1, 2, 3, 4, 5, 7, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 24, 27, 28, 30, 33, 34, 35, 36, 37, 39, 40, 41, 42 },
/* 85 */ { 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 30, 31, 32, 33, 34, 35, 36, 37, 38 },
/* 86 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 87 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 88 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33 },
/* 89 */ { 0, 3, 6, 7, 9, 10, 13, 16, 17, 18, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43 },
/* 90 */ { 0, 2, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 34, 35, 36, 37, 38 },
/* 91 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46, 49, 52, 55, 57 },
/* 92 */ { 0, 1, 3, 6, 8, 9, 11, 14, 18, 20, 21, 23, 26, 27, 28, 29, 30, 31, 33, 36, 38, 39, 41, 44, 48, 50, 51, 53, 56, 57, 58, 59, 60 },
/* 93 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 94 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 95 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 96 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 97 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 98 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 17, 18, 20, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38 },
/* 99 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 100 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 101 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 102 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 103 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 104 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 105 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 106 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 107 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 108 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 109 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 34, 37, 40, 43, 46, 49, 52, 55, 58, 62, 66, 70 },
/* 110 */ { 0, 4, 8, 12, 16, 20, 24, 28, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 53, 56, 59, 62, 65, 68, 71, 74, 77, 80, 83, 86, 89, 92, 95 },
/* 111 */ { 0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76 },
/* 112 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 113 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 114 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 115 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 116 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 117 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 118 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 119 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 120 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 121 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 122 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 123 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 124 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 125 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 126 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 127 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35 },
/* 128 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 129 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 130 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 23, 25, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37 },
/* 131 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 132 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 133 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 134 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 135 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 136 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 137 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 138 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 139 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 140 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 141 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 142 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 143 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 144 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 145 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 146 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 147 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 148 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 149 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 150 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 151 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 152 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 153 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 154 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 155 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 33 },
/* 156 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 157 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 158 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 33 },
/* 159 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 160 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 161 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 162 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 163 */ { 0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 46, 50, 54, 58, 62, 66, 70, 74, 78, 82, 86, 90, 94, 98, 102, 109, 115, 116 },
/* 164 */ { 0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81, 84, 87, 90, 93, 96 },
/* 165 */ { 0, 3, 6, 9, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57 },
/* 166 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 47, 51, 53, 54 },
/* 167 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 168 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47 },
/* 169 */ { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 21, 24, 27, 29, 32, 34, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53 },
/* 170 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 171 */ { 0, 4, 8, 12, 15, 19, 22, 25, 30, 34, 37, 40, 43, 47, 51, 54, 57, 59, 62, 66, 70, 72, 77, 83, 88, 91, 96, 101, 105, 108, 111, 114, 118 },
/* 172 */ { 0, 5, 9, 12, 15, 18, 20, 22, 24, 26, 29, 32, 37, 40, 44, 49, 52, 54, 56, 61, 65, 70, 73, 78, 80, 83, 86, 89, 92, 95, 99, 102, 104 },
/* 173 */ { 0, 3, 6, 9, 13, 16, 19, 22, 27, 31, 33, 38, 40, 44, 48, 51, 54, 57, 61, 63, 66, 70, 72, 77, 80, 82, 84, 86, 88, 90, 92, 94, 96 },
/* 174 */ { 0, 2, 4, 7, 10, 13, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46, 49, 52, 54, 56, 59, 61, 63, 65, 68, 71, 73, 75, 77, 79, 81, 85 },
/* 175 */ { 0, 2, 4, 6, 8, 10, 12, 14, 16, 19, 23, 25, 27, 29, 31, 33, 35, 37, 40, 43, 46, 49, 51, 53, 55, 57, 59, 61, 63, 65, 67, 69, 72 },
/* 176 */ { 0, 3, 5, 8, 11, 14, 16, 19, 22, 26, 28, 31, 34, 37, 40, 45, 51, 53, 55, 57, 59, 61, 63, 65, 67, 69, 71, 73, 75, 77, 79, 81, 83 },
/* 177 */ { 0, 2, 4, 8, 10, 12, 14, 18, 21, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 44, 46, 48, 51, 54, 56, 60, 63, 65, 67, 69, 71, 74, 77 },
/* 178 */ { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81, 84, 87 },
/* 179 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 180 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 181 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 182 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 183 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 184 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 185 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 186 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 187 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 188 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 189 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 190 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 191 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 192 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 193 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 194 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 195 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 196 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 197 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 198 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 199 */ { 0, 2, 4, 6, 9, 12, 14, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 30, 32, 34, 36, 38, 39, 40, 41, 42, 43, 44, 45, 46 },
/* 200 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 201 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33 },
/* 202 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 203 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 204 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 205 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 31, 32, 33 },
/* 206 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 47, 48, 49, 50 },
/* 207 */ { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64 },
/* 208 */ { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64 },
/* 209 */ { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 55, 56, 57, 58, 59 },
/* 210 */ { 0, 1, 2, 3, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60 },
/* 211 */ { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63 },
/* 212 */ { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64 },
/* 213 */ { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 51, 53, 55, 57, 59, 61, 63 },
/* 214 */ { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 37, 38, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61 },
/* 215 */ { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64 },
/* 216 */ { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 57, 58, 59, 60 },
/* 217 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46, 49, 52, 55, 58, 61, 64 },
/* 218 */ { 0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81, 84, 87, 90, 93, 96 },
/* 219 */ { 0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48, 49, 50, 53, 56, 59, 62, 65, 68, 71, 74, 77, 80, 83, 86, 89, 92 },
/* 220 */ { 0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81, 84, 87, 90, 93, 96 },
/* 221 */ { 0, 3, 6, 9, 12, 15, 18, 21, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48 },
/* 222 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 19, 22, 26, 30, 34, 38, 42, 46, 50, 53, 71, 79, 83, 84, 85, 86 },
/* 223 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 28, 29, 30, 31, 32, 33, 34 },
/* 224 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33 },
/* 225 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 226 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 227 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 228 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 229 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 230 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 23, 25, 27, 29, 31, 33, 35, 37, 38, 39, 40 },
/* 231 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 232 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 233 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 234 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 235 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 236 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 237 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
/* 238 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 }
};

// This table has been modified to take care of full width characters
// This table has been modified to take care of German Eszet (lower- and upper-case)
// This table has been modified to decompose vulgar fraction characters like ¼ ½ ... into the common sequences 1/2 1/4 ... using a common slash '/' (U+002F) instead of the [rare] fraction slash (U+2044) that Unicode mistakenly recommends
static u32 unacc_data0  [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data1  [] = { 0x0020, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0028, 0x0043, 0x0029, 0x0061, 0x0000, 0x0000, 0x0000, 0x0028, 0x0052, 0x0029, 0x0020, 0x0000, 0x0000, 0x0032, 0x0033, 0x0020, 0x03BC, 0x0000, 0x0000, 0x0020, 0x0031, 0x006F, 0x0000,
                               0x0031, 0x002F, 0x0034, 0x0031, 0x002F, 0x0032, 0x0033, 0x002F, 0x0034, 0x0000 };
static u32 unacc_data2  [] = { 0x0041, 0x0041, 0x0041, 0x0041, 0x0041, 0x0041, 0x0041, 0x0045, 0x0043, 0x0045, 0x0045, 0x0045, 0x0045, 0x0049, 0x0049, 0x0049, 0x0049, 0x0000, 0x004E, 0x004F, 0x004F, 0x004F, 0x004F, 0x004F, 0x0000, 0x004F, 0x0055, 0x0055, 0x0055, 0x0055, 0x0059, 0x0000,
                               0x0073, 0x0073 };
static u32 unacc_data3  [] = { 0x0061, 0x0061, 0x0061, 0x0061, 0x0061, 0x0061, 0x0061, 0x0065, 0x0063, 0x0065, 0x0065, 0x0065, 0x0065, 0x0069, 0x0069, 0x0069, 0x0069, 0x0000, 0x006E, 0x006F, 0x006F, 0x006F, 0x006F, 0x006F, 0x0000, 0x006F, 0x0075, 0x0075, 0x0075, 0x0075, 0x0079, 0x0000, 0x0079 };
static u32 unacc_data4  [] = { 0x0041, 0x0061, 0x0041, 0x0061, 0x0041, 0x0061, 0x0043, 0x0063, 0x0043, 0x0063, 0x0043, 0x0063, 0x0043, 0x0063, 0x0044, 0x0064, 0x0044, 0x0064, 0x0045, 0x0065, 0x0045, 0x0065, 0x0045, 0x0065, 0x0045, 0x0065, 0x0045, 0x0065, 0x0047, 0x0067, 0x0047, 0x0067 };
static u32 unacc_data5  [] = { 0x0047, 0x0067, 0x0047, 0x0067, 0x0048, 0x0068, 0x0048, 0x0068, 0x0049, 0x0069, 0x0049, 0x0069, 0x0049, 0x0069, 0x0049, 0x0069, 0x0049, 0x0000, 0x0049, 0x004A, 0x0069, 0x006A, 0x004A, 0x006A, 0x004B, 0x006B, 0x0000, 0x004C, 0x006C, 0x004C, 0x006C, 0x004C,
                               0x006C, 0x004C, 0x00B7 };
static u32 unacc_data6  [] = { 0x006C, 0x00B7, 0x004C, 0x006C, 0x004E, 0x006E, 0x004E, 0x006E, 0x004E, 0x006E, 0x02BC, 0x006E, 0x0000, 0x0000, 0x004F, 0x006F, 0x004F, 0x006F, 0x004F, 0x006F, 0x004F, 0x0045, 0x006F, 0x0065, 0x0052, 0x0072, 0x0052, 0x0072, 0x0052, 0x0072, 0x0053, 0x0073,
                               0x0053, 0x0073, 0x0053, 0x0073 };
static u32 unacc_data7  [] = { 0x0053, 0x0073, 0x0054, 0x0074, 0x0054, 0x0074, 0x0054, 0x0074, 0x0055, 0x0075, 0x0055, 0x0075, 0x0055, 0x0075, 0x0055, 0x0075, 0x0055, 0x0075, 0x0055, 0x0075, 0x0057, 0x0077, 0x0059, 0x0079, 0x0059, 0x005A, 0x007A, 0x005A, 0x007A, 0x005A, 0x007A, 0x0073 };
static u32 unacc_data8  [] = { 0x0062, 0x0042, 0x0042, 0x0062, 0x0000, 0x0000, 0x0000, 0x0043, 0x0063, 0x0000, 0x0044, 0x0044, 0x0064, 0x0000, 0x0000, 0x0000, 0x0000, 0x0046, 0x0066, 0x0047, 0x0000, 0x0000, 0x0000, 0x0049, 0x004B, 0x006B, 0x006C, 0x0000, 0x0000, 0x004E, 0x006E, 0x004F };
static u32 unacc_data9  [] = { 0x004F, 0x006F, 0x0000, 0x0000, 0x0050, 0x0070, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0074, 0x0054, 0x0074, 0x0054, 0x0055, 0x0075, 0x0000, 0x0056, 0x0059, 0x0079, 0x005A, 0x007A, 0x0000, 0x0000, 0x0000, 0x0292, 0x0000, 0x0000, 0x0000, 0x0296, 0x0000 };
static u32 unacc_data10 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0044, 0x005A, 0x0044, 0x007A, 0x0064, 0x007A, 0x004C, 0x004A, 0x004C, 0x006A, 0x006C, 0x006A, 0x004E, 0x004A, 0x004E, 0x006A, 0x006E, 0x006A, 0x0041, 0x0061, 0x0049, 0x0069, 0x004F, 0x006F, 0x0055, 0x0075, 0x0055, 0x0075,
                               0x0055, 0x0075, 0x0055, 0x0075, 0x0055, 0x0075, 0x0000, 0x0041, 0x0061 };
static u32 unacc_data11 [] = { 0x0041, 0x0061, 0x0041, 0x0045, 0x0061, 0x0065, 0x0047, 0x0067, 0x0047, 0x0067, 0x004B, 0x006B, 0x004F, 0x006F, 0x004F, 0x006F, 0x01B7, 0x0292, 0x006A, 0x0044, 0x005A, 0x0044, 0x007A, 0x0064, 0x007A, 0x0047, 0x0067, 0x0000, 0x0000, 0x004E, 0x006E, 0x0041,
                               0x0061, 0x0041, 0x0045, 0x0061, 0x0065, 0x004F, 0x006F };
static u32 unacc_data12 [] = { 0x0041, 0x0061, 0x0041, 0x0061, 0x0045, 0x0065, 0x0045, 0x0065, 0x0049, 0x0069, 0x0049, 0x0069, 0x004F, 0x006F, 0x004F, 0x006F, 0x0052, 0x0072, 0x0052, 0x0072, 0x0055, 0x0075, 0x0055, 0x0075, 0x0053, 0x0073, 0x0054, 0x0074, 0x0000, 0x0000, 0x0048, 0x0068 };
static u32 unacc_data13 [] = { 0x004E, 0x0064, 0x0000, 0x0000, 0x005A, 0x007A, 0x0041, 0x0061, 0x0045, 0x0065, 0x004F, 0x006F, 0x004F, 0x006F, 0x004F, 0x006F, 0x004F, 0x006F, 0x0059, 0x0079, 0x006C, 0x006E, 0x0074, 0x0000, 0x0000, 0x0000, 0x0041, 0x0043, 0x0063, 0x004C, 0x0054, 0x0073 };
static u32 unacc_data14 [] = { 0x007A, 0x0000, 0x0000, 0x0042, 0x0000, 0x0000, 0x0045, 0x0065, 0x004A, 0x006A, 0x0000, 0x0071, 0x0052, 0x0072, 0x0059, 0x0079, 0x0000, 0x0000, 0x0000, 0x0062, 0x0000, 0x0063, 0x0064, 0x0064, 0x0000, 0x0000, 0x0259, 0x0000, 0x0000, 0x025C, 0x0000, 0x0237 };
static u32 unacc_data15 [] = { 0x0067, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0068, 0xA727, 0x0069, 0x0000, 0x0000, 0x006C, 0x006C, 0x006C, 0x0000, 0x0000, 0x026F, 0x006D, 0x006E, 0x006E, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0279, 0x0279, 0x0072, 0x0072, 0x0072, 0x0000 };
static u32 unacc_data16 [] = { 0x0000, 0x0000, 0x0073, 0x0000, 0x0237, 0x0000, 0x0283, 0x0000, 0x0074, 0x0000, 0x0000, 0x0076, 0x0000, 0x0000, 0x0000, 0x0000, 0x007A, 0x007A, 0x0000, 0x0292, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0262, 0x0000, 0x006A, 0x0000, 0x0000 };
static u32 unacc_data17 [] = { 0x0071, 0x0294, 0x0000, 0x0000, 0x0000, 0x02A3, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0265, 0x0265, 0x0068, 0x0068, 0x006A, 0x0072, 0x0279, 0x0279, 0x0281, 0x0077, 0x0079, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data18 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0000, 0x0000 };
static u32 unacc_data19 [] = { 0x0263, 0x006C, 0x0073, 0x0078, 0x0295, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data20 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x02B9, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0000, 0x0000, 0x0000, 0x003B, 0x0000 };
static u32 unacc_data21 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0020, 0x0391, 0x00B7, 0x0395, 0x0397, 0x0399, 0x0000, 0x039F, 0x0000, 0x03A5, 0x03A9, 0x03B9, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data22 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0399, 0x03A5, 0x03B1, 0x03B5, 0x03B7, 0x03B9, 0x03C5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data23 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x03B9, 0x03C5, 0x03BF, 0x03C5, 0x03C9, 0x0000, 0x03B2, 0x03B8, 0x03A5, 0x03A5, 0x03A5, 0x03C6, 0x03C0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data24 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x03BA, 0x03C1, 0x03C2, 0x0000, 0x0398, 0x03B5, 0x0000, 0x0000, 0x0000, 0x03A3, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data25 [] = { 0x0415, 0x0415, 0x0000, 0x0413, 0x0000, 0x0000, 0x0000, 0x0406, 0x0000, 0x0000, 0x0000, 0x0000, 0x041A, 0x0418, 0x0423, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0418, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data26 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0438, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data27 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0435, 0x0435, 0x0000, 0x0433, 0x0000, 0x0000, 0x0000, 0x0456, 0x0000, 0x0000, 0x0000, 0x0000, 0x043A, 0x0438, 0x0443, 0x0000 };
static u32 unacc_data28 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0474, 0x0475, 0x0000, 0x0000, 0x0000, 0x0000, 0x0460, 0x0461, 0x0000, 0x0000 };
static u32 unacc_data29 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0418, 0x0438, 0x0000, 0x0000, 0x0420, 0x0440, 0x0413, 0x0433, 0x0413, 0x0433, 0x0413, 0x0433, 0x0416, 0x0436, 0x0417, 0x0437, 0x041A, 0x043A, 0x041A, 0x043A, 0x041A, 0x043A };
static u32 unacc_data30 [] = { 0x0000, 0x0000, 0x041D, 0x043D, 0x0000, 0x0000, 0x041F, 0x043F, 0x0000, 0x0000, 0x0421, 0x0441, 0x0422, 0x0442, 0x0000, 0x0000, 0x04AE, 0x04AF, 0x0425, 0x0445, 0x0000, 0x0000, 0x0427, 0x0447, 0x0427, 0x0447, 0x0000, 0x0000, 0x0000, 0x0000, 0x04BC, 0x04BD };
static u32 unacc_data31 [] = { 0x0000, 0x0416, 0x0436, 0x041A, 0x043A, 0x041B, 0x043B, 0x041D, 0x043D, 0x041D, 0x043D, 0x0000, 0x0000, 0x041C, 0x043C, 0x0000, 0x0410, 0x0430, 0x0410, 0x0430, 0x0000, 0x0000, 0x0415, 0x0435, 0x0000, 0x0000, 0x04D8, 0x04D9, 0x0416, 0x0436, 0x0417, 0x0437 };
static u32 unacc_data32 [] = { 0x0000, 0x0000, 0x0418, 0x0438, 0x0418, 0x0438, 0x041E, 0x043E, 0x0000, 0x0000, 0x04E8, 0x04E9, 0x042D, 0x044D, 0x0423, 0x0443, 0x0423, 0x0443, 0x0423, 0x0443, 0x0427, 0x0447, 0x0413, 0x0433, 0x042B, 0x044B, 0x0413, 0x0433, 0x0425, 0x0445, 0x0425, 0x0445 };
static u32 unacc_data33 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x041B, 0x043B, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data34 [] = { 0x041B, 0x043B, 0x041D, 0x043D, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data35 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0565, 0x0582, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                               0x0000 };
static u32 unacc_data36 [] = { 0x0000, 0x0000, 0x0627, 0x0627, 0x0648, 0x0627, 0x064A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x06A9, 0x06A9, 0x06CC, 0x06CC, 0x06CC };
static u32 unacc_data37 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0627, 0x0627, 0x0000, 0x0627, 0x0674, 0x0648, 0x0674, 0x06C7, 0x0674, 0x064A, 0x0674, 0x0000, 0x0000, 0x0000,
                               0x062A, 0x062A, 0x0000, 0x0000 };
static u32 unacc_data38 [] = { 0x0000, 0x062D, 0x062D, 0x0000, 0x0000, 0x062D, 0x0000, 0x0000, 0x0000, 0x062F, 0x062F, 0x062F, 0x0000, 0x0000, 0x0000, 0x062F, 0x062F, 0x0000, 0x0631, 0x0631, 0x0631, 0x0631, 0x0631, 0x0631, 0x0000, 0x0631, 0x0633, 0x0633, 0x0633, 0x0635, 0x0635, 0x0637 };
static u32 unacc_data39 [] = { 0x0639, 0x0000, 0x0641, 0x0641, 0x0000, 0x0641, 0x0000, 0x0642, 0x0642, 0x0000, 0x0000, 0x0643, 0x0643, 0x0000, 0x0643, 0x0000, 0x06AF, 0x0000, 0x06AF, 0x0000, 0x06AF, 0x0644, 0x0644, 0x0644, 0x0644, 0x0646, 0x0000, 0x0000, 0x0646, 0x0646, 0x0000, 0x0686 };
static u32 unacc_data40 [] = { 0x06D5, 0x0000, 0x06C1, 0x0000, 0x0648, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0648, 0x0000, 0x0000, 0x064A, 0x064A, 0x0648, 0x0000, 0x064A, 0x0000, 0x06D2, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data41 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x062F, 0x0631, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0634, 0x0636, 0x063A, 0x0000, 0x0000, 0x0647 };
static u32 unacc_data42 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0628, 0x0628, 0x0628, 0x0628, 0x0628, 0x0628, 0x0628, 0x062D, 0x062D, 0x062F, 0x062F, 0x0631, 0x0633, 0x0639, 0x0639, 0x0639 };
static u32 unacc_data43 [] = { 0x0641, 0x0641, 0x06A9, 0x06A9, 0x06A9, 0x0645, 0x0645, 0x0646, 0x0646, 0x0646, 0x0644, 0x0631, 0x0631, 0x0633, 0x062D, 0x062D, 0x0633, 0x0631, 0x062D, 0x0627, 0x0627, 0x06CC, 0x06CC, 0x06CC, 0x0648, 0x0648, 0x06D2, 0x06D2, 0x062D, 0x0633, 0x0633, 0x0643 };
static u32 unacc_data44 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0928, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0930, 0x0000, 0x0000, 0x0933, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data45 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0915, 0x0916, 0x0917, 0x091C, 0x0921, 0x0922, 0x092B, 0x092F };
static u32 unacc_data46 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x09A1, 0x09A2, 0x0000, 0x09AF };
static u32 unacc_data47 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x09B0, 0x09B0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data48 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0A32, 0x0000, 0x0000, 0x0A38, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data49 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0A16, 0x0A17, 0x0A1C, 0x0000, 0x0000, 0x0A2B, 0x0000 };
static u32 unacc_data50 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0B21, 0x0B22, 0x0000, 0x0000 };
static u32 unacc_data51 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0B92, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data52 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0E32, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data53 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0EB2, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data54 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0EAB, 0x0E99, 0x0EAB, 0x0EA1,
                               0x0000, 0x0000 };
static u32 unacc_data55 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0F0B, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data56 [] = { 0x0000, 0x0000, 0x0000, 0x0F42, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0F4C, 0x0000, 0x0000, 0x0000, 0x0000, 0x0F51, 0x0000, 0x0000, 0x0000, 0x0000, 0x0F56, 0x0000, 0x0000, 0x0000, 0x0000, 0x0F5B, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data57 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0F40, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data58 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1025, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data59 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x10DC, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data60 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1B05, 0x0000, 0x1B07, 0x0000, 0x1B09, 0x0000, 0x1B0B, 0x0000, 0x1B0D, 0x0000, 0x0000, 0x0000, 0x1B11, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data61 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x029F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1D11, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data62 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0041, 0x0041, 0x0045, 0x0042, 0x0000, 0x0044, 0x0045, 0x018E, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x0000, 0x004F, 0x0222, 0x0050,
                               0x0052 };
static u32 unacc_data63 [] = { 0x0054, 0x0055, 0x0057, 0x0061, 0x0250, 0x0251, 0x1D02, 0x0062, 0x0064, 0x0065, 0x0259, 0x025B, 0x025C, 0x0067, 0x0000, 0x006B, 0x006D, 0x014B, 0x006F, 0x0254, 0x1D16, 0x1D17, 0x0070, 0x0074, 0x0075, 0x1D1D, 0x026F, 0x0076, 0x1D25, 0x03B2, 0x03B3, 0x03B4 };
static u32 unacc_data64 [] = { 0x03C6, 0x03C7, 0x0069, 0x0072, 0x0075, 0x0076, 0x03B2, 0x03B3, 0x03C1, 0x03C6, 0x03C7, 0x0000, 0x0062, 0x0064, 0x0066, 0x006D, 0x006E, 0x0070, 0x0072, 0x0072, 0x0073, 0x0074, 0x007A, 0x0000, 0x043D, 0x0000, 0x0000, 0x0000, 0x0269, 0x0070, 0x0000, 0x028A };
static u32 unacc_data65 [] = { 0x0062, 0x0064, 0x0066, 0x0067, 0x006B, 0x006C, 0x006D, 0x006E, 0x0070, 0x0072, 0x0073, 0x0283, 0x0076, 0x0078, 0x007A, 0x0061, 0x0251, 0x0064, 0x0065, 0x025B, 0x025C, 0x0259, 0x0069, 0x0254, 0x0283, 0x0075, 0x0292, 0x0252, 0x0063, 0x0063, 0x00F0, 0x025C };
static u32 unacc_data66 [] = { 0x0066, 0x0237, 0x0261, 0x0265, 0x0069, 0x0269, 0x026A, 0x1D7B, 0x006A, 0x006C, 0x006C, 0x029F, 0x006D, 0x026F, 0x006E, 0x006E, 0x0274, 0x0275, 0x0278, 0x0073, 0x0283, 0x0074, 0x0289, 0x028A, 0x1D1C, 0x0076, 0x028C, 0x007A, 0x007A, 0x007A, 0x0292, 0x03B8 };
static u32 unacc_data67 [] = { 0x0041, 0x0061, 0x0042, 0x0062, 0x0042, 0x0062, 0x0042, 0x0062, 0x0043, 0x0063, 0x0044, 0x0064, 0x0044, 0x0064, 0x0044, 0x0064, 0x0044, 0x0064, 0x0044, 0x0064, 0x0045, 0x0065, 0x0045, 0x0065, 0x0045, 0x0065, 0x0045, 0x0065, 0x0045, 0x0065, 0x0046, 0x0066 };
static u32 unacc_data68 [] = { 0x0047, 0x0067, 0x0048, 0x0068, 0x0048, 0x0068, 0x0048, 0x0068, 0x0048, 0x0068, 0x0048, 0x0068, 0x0049, 0x0069, 0x0049, 0x0069, 0x004B, 0x006B, 0x004B, 0x006B, 0x004B, 0x006B, 0x004C, 0x006C, 0x004C, 0x006C, 0x004C, 0x006C, 0x004C, 0x006C, 0x004D, 0x006D };
static u32 unacc_data69 [] = { 0x004D, 0x006D, 0x004D, 0x006D, 0x004E, 0x006E, 0x004E, 0x006E, 0x004E, 0x006E, 0x004E, 0x006E, 0x004F, 0x006F, 0x004F, 0x006F, 0x004F, 0x006F, 0x004F, 0x006F, 0x0050, 0x0070, 0x0050, 0x0070, 0x0052, 0x0072, 0x0052, 0x0072, 0x0052, 0x0072, 0x0052, 0x0072 };
static u32 unacc_data70 [] = { 0x0053, 0x0073, 0x0053, 0x0073, 0x0053, 0x0073, 0x0053, 0x0073, 0x0053, 0x0073, 0x0054, 0x0074, 0x0054, 0x0074, 0x0054, 0x0074, 0x0054, 0x0074, 0x0055, 0x0075, 0x0055, 0x0075, 0x0055, 0x0075, 0x0055, 0x0075, 0x0055, 0x0075, 0x0056, 0x0076, 0x0056, 0x0076 };
static u32 unacc_data71 [] = { 0x0057, 0x0077, 0x0057, 0x0077, 0x0057, 0x0077, 0x0057, 0x0077, 0x0057, 0x0077, 0x0058, 0x0078, 0x0058, 0x0078, 0x0059, 0x0079, 0x005A, 0x007A, 0x005A, 0x007A, 0x005A, 0x007A, 0x0068, 0x0074, 0x0077, 0x0079, 0x0061, 0x02BE, 0x0073, 0x0073, 0x0073, 0x0053,
                               0x0053, 0x0000 };
static u32 unacc_data72 [] = { 0x0041, 0x0061, 0x0041, 0x0061, 0x0041, 0x0061, 0x0041, 0x0061, 0x0041, 0x0061, 0x0041, 0x0061, 0x0041, 0x0061, 0x0041, 0x0061, 0x0041, 0x0061, 0x0041, 0x0061, 0x0041, 0x0061, 0x0041, 0x0061, 0x0045, 0x0065, 0x0045, 0x0065, 0x0045, 0x0065, 0x0045, 0x0065 };
static u32 unacc_data73 [] = { 0x0045, 0x0065, 0x0045, 0x0065, 0x0045, 0x0065, 0x0045, 0x0065, 0x0049, 0x0069, 0x0049, 0x0069, 0x004F, 0x006F, 0x004F, 0x006F, 0x004F, 0x006F, 0x004F, 0x006F, 0x004F, 0x006F, 0x004F, 0x006F, 0x004F, 0x006F, 0x004F, 0x006F, 0x004F, 0x006F, 0x004F, 0x006F };
static u32 unacc_data74 [] = { 0x004F, 0x006F, 0x004F, 0x006F, 0x0055, 0x0075, 0x0055, 0x0075, 0x0055, 0x0075, 0x0055, 0x0075, 0x0055, 0x0075, 0x0055, 0x0075, 0x0055, 0x0075, 0x0059, 0x0079, 0x0059, 0x0079, 0x0059, 0x0079, 0x0059, 0x0079, 0x0000, 0x0000, 0x0000, 0x0000, 0x0059, 0x0079 };
static u32 unacc_data75 [] = { 0x03B1, 0x03B1, 0x03B1, 0x03B1, 0x03B1, 0x03B1, 0x03B1, 0x03B1, 0x0391, 0x0391, 0x0391, 0x0391, 0x0391, 0x0391, 0x0391, 0x0391, 0x03B5, 0x03B5, 0x03B5, 0x03B5, 0x03B5, 0x03B5, 0x0000, 0x0000, 0x0395, 0x0395, 0x0395, 0x0395, 0x0395, 0x0395, 0x0000, 0x0000 };
static u32 unacc_data76 [] = { 0x03B7, 0x03B7, 0x03B7, 0x03B7, 0x03B7, 0x03B7, 0x03B7, 0x03B7, 0x0397, 0x0397, 0x0397, 0x0397, 0x0397, 0x0397, 0x0397, 0x0397, 0x03B9, 0x03B9, 0x03B9, 0x03B9, 0x03B9, 0x03B9, 0x03B9, 0x03B9, 0x0399, 0x0399, 0x0399, 0x0399, 0x0399, 0x0399, 0x0399, 0x0399 };
static u32 unacc_data77 [] = { 0x03BF, 0x03BF, 0x03BF, 0x03BF, 0x03BF, 0x03BF, 0x0000, 0x0000, 0x039F, 0x039F, 0x039F, 0x039F, 0x039F, 0x039F, 0x0000, 0x0000, 0x03C5, 0x03C5, 0x03C5, 0x03C5, 0x03C5, 0x03C5, 0x03C5, 0x03C5, 0x0000, 0x03A5, 0x0000, 0x03A5, 0x0000, 0x03A5, 0x0000, 0x03A5 };
static u32 unacc_data78 [] = { 0x03C9, 0x03C9, 0x03C9, 0x03C9, 0x03C9, 0x03C9, 0x03C9, 0x03C9, 0x03A9, 0x03A9, 0x03A9, 0x03A9, 0x03A9, 0x03A9, 0x03A9, 0x03A9, 0x03B1, 0x03B1, 0x03B5, 0x03B5, 0x03B7, 0x03B7, 0x03B9, 0x03B9, 0x03BF, 0x03BF, 0x03C5, 0x03C5, 0x03C9, 0x03C9, 0x0000, 0x0000 };
static u32 unacc_data79 [] = { 0x03B1, 0x03B1, 0x03B1, 0x03B1, 0x03B1, 0x03B1, 0x03B1, 0x03B1, 0x0391, 0x0391, 0x0391, 0x0391, 0x0391, 0x0391, 0x0391, 0x0391, 0x03B7, 0x03B7, 0x03B7, 0x03B7, 0x03B7, 0x03B7, 0x03B7, 0x03B7, 0x0397, 0x0397, 0x0397, 0x0397, 0x0397, 0x0397, 0x0397, 0x0397 };
static u32 unacc_data80 [] = { 0x03C9, 0x03C9, 0x03C9, 0x03C9, 0x03C9, 0x03C9, 0x03C9, 0x03C9, 0x03A9, 0x03A9, 0x03A9, 0x03A9, 0x03A9, 0x03A9, 0x03A9, 0x03A9, 0x03B1, 0x03B1, 0x03B1, 0x03B1, 0x03B1, 0x0000, 0x03B1, 0x03B1, 0x0391, 0x0391, 0x0391, 0x0391, 0x0391, 0x0020, 0x03B9, 0x0020 };
static u32 unacc_data81 [] = { 0x0020, 0x0020, 0x03B7, 0x03B7, 0x03B7, 0x0000, 0x03B7, 0x03B7, 0x0395, 0x0395, 0x0397, 0x0397, 0x0397, 0x0020, 0x0020, 0x0020, 0x03B9, 0x03B9, 0x03B9, 0x03B9, 0x0000, 0x0000, 0x03B9, 0x03B9, 0x0399, 0x0399, 0x0399, 0x0399, 0x0000, 0x0020, 0x0020, 0x0020 };
static u32 unacc_data82 [] = { 0x03C5, 0x03C5, 0x03C5, 0x03C5, 0x03C1, 0x03C1, 0x03C5, 0x03C5, 0x03A5, 0x03A5, 0x03A5, 0x03A5, 0x03A1, 0x0020, 0x0020, 0x0060, 0x0000, 0x0000, 0x03C9, 0x03C9, 0x03C9, 0x0000, 0x03C9, 0x03C9, 0x039F, 0x039F, 0x03A9, 0x03A9, 0x03A9, 0x0020, 0x0020, 0x0000 };
static u32 unacc_data83 [] = { 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2010, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data84 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x002E, 0x002E, 0x002E, 0x002E, 0x002E, 0x002E, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0000, 0x0000, 0x0000, 0x2032, 0x2032, 0x2032, 0x2032, 0x2032, 0x0000, 0x2035, 0x2035, 0x2035, 0x2035,
                               0x2035, 0x0000, 0x0000, 0x0000, 0x0000, 0x0021, 0x0021, 0x0000, 0x0020, 0x0000 };
static u32 unacc_data85 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x005B, 0x005D, 0x003F, 0x003F, 0x003F, 0x0021, 0x0021, 0x003F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2032, 0x2032, 0x2032, 0x2032, 0x0000, 0x0000,
                               0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020 };
static u32 unacc_data86 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0030, 0x0069, 0x0000, 0x0000, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x002B, 0x2212, 0x003D, 0x0028, 0x0029, 0x006E };
static u32 unacc_data87 [] = { 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x002B, 0x2212, 0x003D, 0x0028, 0x0029, 0x0000, 0x0061, 0x0065, 0x006F, 0x0078, 0x0259, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data88 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0052, 0x0073, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data89 [] = { 0x0061, 0x002F, 0x0063, 0x0061, 0x002F, 0x0073, 0x0043, 0x00B0, 0x0043, 0x0000, 0x0063, 0x002F, 0x006F, 0x0063, 0x002F, 0x0075, 0x0190, 0x0000, 0x00B0, 0x0046, 0x0067, 0x0048, 0x0048, 0x0048, 0x0068, 0x0068, 0x0049, 0x0049, 0x004C, 0x006C, 0x0000, 0x004E,
                               0x004E, 0x006F, 0x0000, 0x0000, 0x0050, 0x0051, 0x0052, 0x0052, 0x0052, 0x0000, 0x0000 };
static u32 unacc_data90 [] = { 0x0053, 0x004D, 0x0054, 0x0045, 0x004C, 0x0054, 0x004D, 0x0000, 0x005A, 0x0000, 0x03A9, 0x0000, 0x005A, 0x0000, 0x004B, 0x0041, 0x0042, 0x0043, 0x0000, 0x0065, 0x0045, 0x0046, 0x0000, 0x004D, 0x006F, 0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x0069, 0x0000, 0x0046,
                               0x0041, 0x0058, 0x03C0, 0x03B3, 0x0393, 0x03A0 };
static u32 unacc_data91 [] = { 0x2211, 0x0000, 0x0000, 0x0000, 0x0000, 0x0044, 0x0064, 0x0065, 0x0069, 0x006A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0031, 0x002F, 0x0033, 0x0032, 0x002F, 0x0033, 0x0031, 0x002F, 0x0035, 0x0032, 0x002F, 0x0035, 0x0033,
                               0x002F, 0x0035, 0x0034, 0x002F, 0x0035, 0x0031, 0x002F, 0x0036, 0x0035, 0x002F, 0x0036, 0x0031, 0x002F, 0x0038, 0x0033, 0x002F, 0x0038, 0x0035, 0x002F, 0x0038, 0x0037, 0x002F, 0x0038, 0x0031, 0x002F };
static u32 unacc_data92 [] = { 0x0049, 0x0049, 0x0049, 0x0049, 0x0049, 0x0049, 0x0049, 0x0056, 0x0056, 0x0056, 0x0049, 0x0056, 0x0049, 0x0049, 0x0056, 0x0049, 0x0049, 0x0049, 0x0049, 0x0058, 0x0058, 0x0058, 0x0049, 0x0058, 0x0049, 0x0049, 0x004C, 0x0043, 0x0044, 0x004D, 0x0069, 0x0069,
                               0x0069, 0x0069, 0x0069, 0x0069, 0x0069, 0x0076, 0x0076, 0x0076, 0x0069, 0x0076, 0x0069, 0x0069, 0x0076, 0x0069, 0x0069, 0x0069, 0x0069, 0x0078, 0x0078, 0x0078, 0x0069, 0x0078, 0x0069, 0x0069, 0x006C, 0x0063, 0x0064, 0x006D };
static u32 unacc_data93 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2190, 0x2192, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data94 [] = { 0x0000, 0x0000, 0x2190, 0x2192, 0x0000, 0x0000, 0x0000, 0x0000, 0x2195, 0x2190, 0x2192, 0x2190, 0x2192, 0x0000, 0x2194, 0x0000, 0x2191, 0x2191, 0x2193, 0x2193, 0x2192, 0x2193, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data95 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x21D0, 0x21D4, 0x21D2, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2191, 0x2193 };
static u32 unacc_data96 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x21EB, 0x21EB, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2190, 0x2192, 0x2194, 0x2190, 0x2192, 0x2194, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data97 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x2203, 0x0000, 0x0000, 0x0000, 0x0000, 0x2208, 0x0000, 0x0000, 0x220B, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data98 [] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x2223, 0x0000, 0x2225, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x222B, 0x222B, 0x222B, 0x222B, 0x222B, 0x0000, 0x222E, 0x222E, 0x222E, 0x222E, 0x222E, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                               0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data99 [] = { 0x0000, 0x223C, 0x0000, 0x0000, 0x2243, 0x0000, 0x0000, 0x2245, 0x0000, 0x2248, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data100[] = { 0x003D, 0x0000, 0x2261, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x224D, 0x003C, 0x003E, 0x2264, 0x2265, 0x0000, 0x0000, 0x2272, 0x2273, 0x0000, 0x0000, 0x2276, 0x2277, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data101[] = { 0x227A, 0x227B, 0x0000, 0x0000, 0x2282, 0x2283, 0x0000, 0x0000, 0x2286, 0x2287, 0x2282, 0x2283, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data102[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x22A2, 0x22A8, 0x22A9, 0x22AB, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x221F, 0x0000 };
static u32 unacc_data103[] = { 0x227C, 0x227D, 0x2291, 0x2292, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x22B2, 0x22B3, 0x22B4, 0x22B5, 0x0000, 0x0000, 0x0000, 0x0000, 0x2208, 0x2208, 0x220A, 0x2208, 0x2208, 0x220A, 0x2208, 0x2208, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data104[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x3008, 0x3009, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data105[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x221F, 0x0000, 0x0000, 0x007C };
static u32 unacc_data106[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x25A1, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data107[] = { 0x0000, 0x23C9, 0x23CA, 0x0000, 0x23C9, 0x23CA, 0x0000, 0x23C9, 0x23CA, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data108[] = { 0x0000, 0x0000, 0x0000, 0x232C, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data109[] = { 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x0031, 0x0030, 0x0031, 0x0031, 0x0031, 0x0032, 0x0031, 0x0033, 0x0031, 0x0034, 0x0031, 0x0035, 0x0031, 0x0036, 0x0031, 0x0037, 0x0031, 0x0038, 0x0031, 0x0039, 0x0032, 0x0030, 0x0028,
                               0x0031, 0x0029, 0x0028, 0x0032, 0x0029, 0x0028, 0x0033, 0x0029, 0x0028, 0x0034, 0x0029, 0x0028, 0x0035, 0x0029, 0x0028, 0x0036, 0x0029, 0x0028, 0x0037, 0x0029, 0x0028, 0x0038, 0x0029, 0x0028, 0x0039, 0x0029, 0x0028, 0x0031, 0x0030, 0x0029, 0x0028, 0x0031,
                               0x0031, 0x0029, 0x0028, 0x0031, 0x0032, 0x0029 };
static u32 unacc_data110[] = { 0x0028, 0x0031, 0x0033, 0x0029, 0x0028, 0x0031, 0x0034, 0x0029, 0x0028, 0x0031, 0x0035, 0x0029, 0x0028, 0x0031, 0x0036, 0x0029, 0x0028, 0x0031, 0x0037, 0x0029, 0x0028, 0x0031, 0x0038, 0x0029, 0x0028, 0x0031, 0x0039, 0x0029, 0x0028, 0x0032, 0x0030, 0x0029,
                               0x0031, 0x002E, 0x0032, 0x002E, 0x0033, 0x002E, 0x0034, 0x002E, 0x0035, 0x002E, 0x0036, 0x002E, 0x0037, 0x002E, 0x0038, 0x002E, 0x0039, 0x002E, 0x0031, 0x0030, 0x002E, 0x0031, 0x0031, 0x002E, 0x0031, 0x0032, 0x002E, 0x0031, 0x0033, 0x002E, 0x0031, 0x0034,
                               0x002E, 0x0031, 0x0035, 0x002E, 0x0031, 0x0036, 0x002E, 0x0031, 0x0037, 0x002E, 0x0031, 0x0038, 0x002E, 0x0031, 0x0039, 0x002E, 0x0032, 0x0030, 0x002E, 0x0028, 0x0061, 0x0029, 0x0028, 0x0062, 0x0029, 0x0028, 0x0063, 0x0029, 0x0028, 0x0064, 0x0029 };
static u32 unacc_data111[] = { 0x0028, 0x0065, 0x0029, 0x0028, 0x0066, 0x0029, 0x0028, 0x0067, 0x0029, 0x0028, 0x0068, 0x0029, 0x0028, 0x0069, 0x0029, 0x0028, 0x006A, 0x0029, 0x0028, 0x006B, 0x0029, 0x0028, 0x006C, 0x0029, 0x0028, 0x006D, 0x0029, 0x0028, 0x006E, 0x0029, 0x0028, 0x006F,
                               0x0029, 0x0028, 0x0070, 0x0029, 0x0028, 0x0071, 0x0029, 0x0028, 0x0072, 0x0029, 0x0028, 0x0073, 0x0029, 0x0028, 0x0074, 0x0029, 0x0028, 0x0075, 0x0029, 0x0028, 0x0076, 0x0029, 0x0028, 0x0077, 0x0029, 0x0028, 0x0078, 0x0029, 0x0028, 0x0079, 0x0029, 0x0028,
                               0x007A, 0x0029, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A };
static u32 unacc_data112[] = { 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F, 0x0070 };
static u32 unacc_data113[] = { 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x0030, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data114[] = { 0x0000, 0x0000, 0x25A1, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data115[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x25A1, 0x25B3, 0x0000, 0x0000, 0x0000, 0x25A1, 0x25A1, 0x25A1, 0x25A1, 0x25CB, 0x25CB, 0x25CB, 0x25CB, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data116[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2610, 0x2610, 0x0000, 0x2602, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data117[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x25CB, 0x25CB, 0x25CF, 0x25CF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data118[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x25C7, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x22A5 };
static u32 unacc_data119[] = { 0x0000, 0x0000, 0x27E1, 0x27E1, 0x25A1, 0x25A1, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data120[] = { 0x0000, 0x0000, 0x21D0, 0x21D2, 0x21D4, 0x0000, 0x0000, 0x0000, 0x2193, 0x2191, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2192, 0x0000, 0x0000, 0x2192, 0x2192, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data121[] = { 0x0000, 0x0000, 0x0000, 0x2196, 0x2197, 0x2198, 0x2199, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x293A, 0x0000, 0x0000 };
static u32 unacc_data122[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2192, 0x2190, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data123[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x005B, 0x005D, 0x005B, 0x005D, 0x005B, 0x005D, 0x3008, 0x3009, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2220, 0x0000 };
static u32 unacc_data124[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x2220, 0x29A3, 0x0000, 0x0000, 0x2221, 0x2221, 0x2221, 0x2221, 0x2221, 0x2221, 0x2221, 0x2221, 0x0000, 0x2205, 0x2205, 0x2205, 0x2205, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data125[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x22C8, 0x22C8, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data126[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x29E3, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x25C6, 0x0000, 0x25CB, 0x25CF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x002F, 0x005C, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data127[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x222B, 0x222B, 0x222B, 0x222B, 0x0000, 0x222B, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x222B, 0x222B, 0x222B, 0x222B, 0x222B, 0x222B,
                               0x0000, 0x0000, 0x0000 };
static u32 unacc_data128[] = { 0x0000, 0x0000, 0x002B, 0x002B, 0x002B, 0x002B, 0x002B, 0x002B, 0x002B, 0x2212, 0x2212, 0x2212, 0x2212, 0x0000, 0x0000, 0x0000, 0x00D7, 0x00D7, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data129[] = { 0x2229, 0x222A, 0x222A, 0x2229, 0x2229, 0x222A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2227, 0x2228, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2227, 0x2228, 0x2227, 0x2228, 0x2227, 0x2227 };
static u32 unacc_data130[] = { 0x2227, 0x0000, 0x2228, 0x2228, 0x0000, 0x0000, 0x003D, 0x0000, 0x0000, 0x0000, 0x223C, 0x223C, 0x0000, 0x0000, 0x0000, 0x2248, 0x0000, 0x0000, 0x0000, 0x0000, 0x003A, 0x003A, 0x003D, 0x003D, 0x003D, 0x003D, 0x003D, 0x003D, 0x003D, 0x0000, 0x0000, 0x0000,
                               0x0000, 0x0000, 0x0000, 0x0000, 0x2A7D };
static u32 unacc_data131[] = { 0x2A7E, 0x2A7D, 0x2A7E, 0x2A7D, 0x2A7E, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2A95, 0x2A96, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data132[] = { 0x0000, 0x0000, 0x0000, 0x2AA1, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x003D, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data133[] = { 0x0000, 0x0000, 0x0000, 0x2286, 0x2287, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x22D4, 0x0000, 0x2ADD, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data134[] = { 0x0000, 0x27C2, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2ADF, 0x2AE0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2223, 0x007C, 0x007C, 0x22A4, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data135[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2192, 0x2192, 0x2190, 0x2190, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data136[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2190, 0x2190, 0x2190, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data137[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2C24, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data138[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2C54, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data139[] = { 0x004C, 0x006C, 0x004C, 0x0050, 0x0052, 0x0061, 0x0074, 0x0048, 0x0068, 0x004B, 0x006B, 0x005A, 0x007A, 0x0000, 0x004D, 0x0000, 0x0000, 0x0076, 0x0057, 0x0077, 0x0076, 0x0000, 0x0000, 0x0000, 0x0065, 0x0279, 0x006F, 0x0000, 0x006A, 0x0056, 0x0000, 0x0000 };
static u32 unacc_data140[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2D61, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data141[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2010, 0x007E, 0x0000, 0x0000, 0x007E, 0x007E };
static u32 unacc_data142[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x6BCD };
static u32 unacc_data143[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x9F9F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data144[] = { 0x4E00, 0x4E28, 0x4E36, 0x4E3F, 0x4E59, 0x4E85, 0x4E8C, 0x4EA0, 0x4EBA, 0x513F, 0x5165, 0x516B, 0x5182, 0x5196, 0x51AB, 0x51E0, 0x51F5, 0x5200, 0x529B, 0x52F9, 0x5315, 0x531A, 0x5338, 0x5341, 0x535C, 0x5369, 0x5382, 0x53B6, 0x53C8, 0x53E3, 0x56D7, 0x571F };
static u32 unacc_data145[] = { 0x58EB, 0x5902, 0x590A, 0x5915, 0x5927, 0x5973, 0x5B50, 0x5B80, 0x5BF8, 0x5C0F, 0x5C22, 0x5C38, 0x5C6E, 0x5C71, 0x5DDB, 0x5DE5, 0x5DF1, 0x5DFE, 0x5E72, 0x5E7A, 0x5E7F, 0x5EF4, 0x5EFE, 0x5F0B, 0x5F13, 0x5F50, 0x5F61, 0x5F73, 0x5FC3, 0x6208, 0x6236, 0x624B };
static u32 unacc_data146[] = { 0x652F, 0x6534, 0x6587, 0x6597, 0x65A4, 0x65B9, 0x65E0, 0x65E5, 0x66F0, 0x6708, 0x6728, 0x6B20, 0x6B62, 0x6B79, 0x6BB3, 0x6BCB, 0x6BD4, 0x6BDB, 0x6C0F, 0x6C14, 0x6C34, 0x706B, 0x722A, 0x7236, 0x723B, 0x723F, 0x7247, 0x7259, 0x725B, 0x72AC, 0x7384, 0x7389 };
static u32 unacc_data147[] = { 0x74DC, 0x74E6, 0x7518, 0x751F, 0x7528, 0x7530, 0x758B, 0x7592, 0x7676, 0x767D, 0x76AE, 0x76BF, 0x76EE, 0x77DB, 0x77E2, 0x77F3, 0x793A, 0x79B8, 0x79BE, 0x7A74, 0x7ACB, 0x7AF9, 0x7C73, 0x7CF8, 0x7F36, 0x7F51, 0x7F8A, 0x7FBD, 0x8001, 0x800C, 0x8012, 0x8033 };
static u32 unacc_data148[] = { 0x807F, 0x8089, 0x81E3, 0x81EA, 0x81F3, 0x81FC, 0x820C, 0x821B, 0x821F, 0x826E, 0x8272, 0x8278, 0x864D, 0x866B, 0x8840, 0x884C, 0x8863, 0x897E, 0x898B, 0x89D2, 0x8A00, 0x8C37, 0x8C46, 0x8C55, 0x8C78, 0x8C9D, 0x8D64, 0x8D70, 0x8DB3, 0x8EAB, 0x8ECA, 0x8F9B };
static u32 unacc_data149[] = { 0x8FB0, 0x8FB5, 0x9091, 0x9149, 0x91C6, 0x91CC, 0x91D1, 0x9577, 0x9580, 0x961C, 0x96B6, 0x96B9, 0x96E8, 0x9751, 0x975E, 0x9762, 0x9769, 0x97CB, 0x97ED, 0x97F3, 0x9801, 0x98A8, 0x98DB, 0x98DF, 0x9996, 0x9999, 0x99AC, 0x9AA8, 0x9AD8, 0x9ADF, 0x9B25, 0x9B2F };
static u32 unacc_data150[] = { 0x9B32, 0x9B3C, 0x9B5A, 0x9CE5, 0x9E75, 0x9E7F, 0x9EA5, 0x9EBB, 0x9EC3, 0x9ECD, 0x9ED1, 0x9EF9, 0x9EFD, 0x9F0E, 0x9F13, 0x9F20, 0x9F3B, 0x9F4A, 0x9F52, 0x9F8D, 0x9F9C, 0x9FA0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data151[] = { 0x0020, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data152[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x3012, 0x0000, 0x5341, 0x5344, 0x5345, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data153[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x304B, 0x0000, 0x304D, 0x0000, 0x304F, 0x0000, 0x3051, 0x0000, 0x3053, 0x0000, 0x3055, 0x0000, 0x3057, 0x0000, 0x3059, 0x0000, 0x305B, 0x0000, 0x305D, 0x0000 };
static u32 unacc_data154[] = { 0x305F, 0x0000, 0x3061, 0x0000, 0x0000, 0x3064, 0x0000, 0x3066, 0x0000, 0x3068, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x306F, 0x306F, 0x0000, 0x3072, 0x3072, 0x0000, 0x3075, 0x3075, 0x0000, 0x3078, 0x3078, 0x0000, 0x307B, 0x307B, 0x0000, 0x0000 };
static u32 unacc_data155[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x3046, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0020, 0x0000, 0x309D, 0x3088,
                               0x308A };
static u32 unacc_data156[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x30AB, 0x0000, 0x30AD, 0x0000, 0x30AF, 0x0000, 0x30B1, 0x0000, 0x30B3, 0x0000, 0x30B5, 0x0000, 0x30B7, 0x0000, 0x30B9, 0x0000, 0x30BB, 0x0000, 0x30BD, 0x0000 };
static u32 unacc_data157[] = { 0x30BF, 0x0000, 0x30C1, 0x0000, 0x0000, 0x30C4, 0x0000, 0x30C6, 0x0000, 0x30C8, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x30CF, 0x30CF, 0x0000, 0x30D2, 0x30D2, 0x0000, 0x30D5, 0x30D5, 0x0000, 0x30D8, 0x30D8, 0x0000, 0x30DB, 0x30DB, 0x0000, 0x0000 };
static u32 unacc_data158[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x30A6, 0x0000, 0x0000, 0x30EF, 0x30F0, 0x30F1, 0x30F2, 0x0000, 0x0000, 0x0000, 0x30FD, 0x30B3,
                               0x30C8 };
static u32 unacc_data159[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1100, 0x1101, 0x11AA, 0x1102, 0x11AC, 0x11AD, 0x1103, 0x1104, 0x1105, 0x11B0, 0x11B1, 0x11B2, 0x11B3, 0x11B4, 0x11B5 };
static u32 unacc_data160[] = { 0x111A, 0x1106, 0x1107, 0x1108, 0x1121, 0x1109, 0x110A, 0x110B, 0x110C, 0x110D, 0x110E, 0x110F, 0x1110, 0x1111, 0x1112, 0x1161, 0x1162, 0x1163, 0x1164, 0x1165, 0x1166, 0x1167, 0x1168, 0x1169, 0x116A, 0x116B, 0x116C, 0x116D, 0x116E, 0x116F, 0x1170, 0x1171 };
static u32 unacc_data161[] = { 0x1172, 0x1173, 0x1174, 0x1175, 0x1160, 0x1114, 0x1115, 0x11C7, 0x11C8, 0x11CC, 0x11CE, 0x11D3, 0x11D7, 0x11D9, 0x111C, 0x11DD, 0x11DF, 0x111D, 0x111E, 0x1120, 0x1122, 0x1123, 0x1127, 0x1129, 0x112B, 0x112C, 0x112D, 0x112E, 0x112F, 0x1132, 0x1136, 0x1140 };
static u32 unacc_data162[] = { 0x1147, 0x114C, 0x11F1, 0x11F2, 0x1157, 0x1158, 0x1159, 0x1184, 0x1185, 0x1188, 0x1191, 0x1192, 0x1194, 0x119E, 0x11A1, 0x0000, 0x0000, 0x0000, 0x4E00, 0x4E8C, 0x4E09, 0x56DB, 0x4E0A, 0x4E2D, 0x4E0B, 0x7532, 0x4E59, 0x4E19, 0x4E01, 0x5929, 0x5730, 0x4EBA };
static u32 unacc_data163[] = { 0x0028, 0x1100, 0x0029, 0x0028, 0x1102, 0x0029, 0x0028, 0x1103, 0x0029, 0x0028, 0x1105, 0x0029, 0x0028, 0x1106, 0x0029, 0x0028, 0x1107, 0x0029, 0x0028, 0x1109, 0x0029, 0x0028, 0x110B, 0x0029, 0x0028, 0x110C, 0x0029, 0x0028, 0x110E, 0x0029, 0x0028, 0x110F,
                               0x0029, 0x0028, 0x1110, 0x0029, 0x0028, 0x1111, 0x0029, 0x0028, 0x1112, 0x0029, 0x0028, 0x1100, 0x1161, 0x0029, 0x0028, 0x1102, 0x1161, 0x0029, 0x0028, 0x1103, 0x1161, 0x0029, 0x0028, 0x1105, 0x1161, 0x0029, 0x0028, 0x1106, 0x1161, 0x0029, 0x0028, 0x1107,
                               0x1161, 0x0029, 0x0028, 0x1109, 0x1161, 0x0029, 0x0028, 0x110B, 0x1161, 0x0029, 0x0028, 0x110C, 0x1161, 0x0029, 0x0028, 0x110E, 0x1161, 0x0029, 0x0028, 0x110F, 0x1161, 0x0029, 0x0028, 0x1110, 0x1161, 0x0029, 0x0028, 0x1111, 0x1161, 0x0029, 0x0028, 0x1112,
                               0x1161, 0x0029, 0x0028, 0x110C, 0x116E, 0x0029, 0x0028, 0x110B, 0x1169, 0x110C, 0x1165, 0x11AB, 0x0029, 0x0028, 0x110B, 0x1169, 0x1112, 0x116E, 0x0029, 0x0000 };
static u32 unacc_data164[] = { 0x0028, 0x4E00, 0x0029, 0x0028, 0x4E8C, 0x0029, 0x0028, 0x4E09, 0x0029, 0x0028, 0x56DB, 0x0029, 0x0028, 0x4E94, 0x0029, 0x0028, 0x516D, 0x0029, 0x0028, 0x4E03, 0x0029, 0x0028, 0x516B, 0x0029, 0x0028, 0x4E5D, 0x0029, 0x0028, 0x5341, 0x0029, 0x0028, 0x6708,
                               0x0029, 0x0028, 0x706B, 0x0029, 0x0028, 0x6C34, 0x0029, 0x0028, 0x6728, 0x0029, 0x0028, 0x91D1, 0x0029, 0x0028, 0x571F, 0x0029, 0x0028, 0x65E5, 0x0029, 0x0028, 0x682A, 0x0029, 0x0028, 0x6709, 0x0029, 0x0028, 0x793E, 0x0029, 0x0028, 0x540D, 0x0029, 0x0028,
                               0x7279, 0x0029, 0x0028, 0x8CA1, 0x0029, 0x0028, 0x795D, 0x0029, 0x0028, 0x52B4, 0x0029, 0x0028, 0x4EE3, 0x0029, 0x0028, 0x547C, 0x0029, 0x0028, 0x5B66, 0x0029, 0x0028, 0x76E3, 0x0029, 0x0028, 0x4F01, 0x0029, 0x0028, 0x8CC7, 0x0029, 0x0028, 0x5354, 0x0029 };
static u32 unacc_data165[] = { 0x0028, 0x796D, 0x0029, 0x0028, 0x4F11, 0x0029, 0x0028, 0x81EA, 0x0029, 0x0028, 0x81F3, 0x0029, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0050, 0x0054, 0x0045, 0x0032, 0x0031, 0x0032, 0x0032, 0x0032,
                               0x0033, 0x0032, 0x0034, 0x0032, 0x0035, 0x0032, 0x0036, 0x0032, 0x0037, 0x0032, 0x0038, 0x0032, 0x0039, 0x0033, 0x0030, 0x0033, 0x0031, 0x0033, 0x0032, 0x0033, 0x0033, 0x0033, 0x0034, 0x0033, 0x0035 };
static u32 unacc_data166[] = { 0x1100, 0x1102, 0x1103, 0x1105, 0x1106, 0x1107, 0x1109, 0x110B, 0x110C, 0x110E, 0x110F, 0x1110, 0x1111, 0x1112, 0x1100, 0x1161, 0x1102, 0x1161, 0x1103, 0x1161, 0x1105, 0x1161, 0x1106, 0x1161, 0x1107, 0x1161, 0x1109, 0x1161, 0x110B, 0x1161, 0x110C, 0x1161,
                               0x110E, 0x1161, 0x110F, 0x1161, 0x1110, 0x1161, 0x1111, 0x1161, 0x1112, 0x1161, 0x110E, 0x1161, 0x11B7, 0x1100, 0x1169, 0x110C, 0x116E, 0x110B, 0x1174, 0x110B, 0x116E, 0x0000 };
static u32 unacc_data167[] = { 0x4E00, 0x4E8C, 0x4E09, 0x56DB, 0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D, 0x5341, 0x6708, 0x706B, 0x6C34, 0x6728, 0x91D1, 0x571F, 0x65E5, 0x682A, 0x6709, 0x793E, 0x540D, 0x7279, 0x8CA1, 0x795D, 0x52B4, 0x79D8, 0x7537, 0x5973, 0x9069, 0x512A, 0x5370, 0x6CE8 };
static u32 unacc_data168[] = { 0x9805, 0x4F11, 0x5199, 0x6B63, 0x4E0A, 0x4E2D, 0x4E0B, 0x5DE6, 0x53F3, 0x533B, 0x5B97, 0x5B66, 0x76E3, 0x4F01, 0x8CC7, 0x5354, 0x591C, 0x0033, 0x0036, 0x0033, 0x0037, 0x0033, 0x0038, 0x0033, 0x0039, 0x0034, 0x0030, 0x0034, 0x0031, 0x0034, 0x0032, 0x0034,
                               0x0033, 0x0034, 0x0034, 0x0034, 0x0035, 0x0034, 0x0036, 0x0034, 0x0037, 0x0034, 0x0038, 0x0034, 0x0039, 0x0035, 0x0030 };
static u32 unacc_data169[] = { 0x0031, 0x6708, 0x0032, 0x6708, 0x0033, 0x6708, 0x0034, 0x6708, 0x0035, 0x6708, 0x0036, 0x6708, 0x0037, 0x6708, 0x0038, 0x6708, 0x0039, 0x6708, 0x0031, 0x0030, 0x6708, 0x0031, 0x0031, 0x6708, 0x0031, 0x0032, 0x6708, 0x0048, 0x0067, 0x0065, 0x0072, 0x0067,
                               0x0065, 0x0056, 0x004C, 0x0054, 0x0044, 0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF, 0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF };
static u32 unacc_data170[] = { 0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD, 0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF, 0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA, 0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F0, 0x30F1, 0x30F2, 0x0000 };
static u32 unacc_data171[] = { 0x30A2, 0x30FC, 0x30C8, 0x30CF, 0x30A2, 0x30EB, 0x30D5, 0x30A1, 0x30A2, 0x30F3, 0x30A2, 0x30D8, 0x30A2, 0x30FC, 0x30EB, 0x30A4, 0x30CB, 0x30F3, 0x30AF, 0x30A4, 0x30F3, 0x30C1, 0x30A6, 0x30A9, 0x30F3, 0x30A8, 0x30B9, 0x30AF, 0x30FC, 0x30C8, 0x30A8, 0x30FC,
                               0x30AB, 0x30FC, 0x30AA, 0x30F3, 0x30B9, 0x30AA, 0x30FC, 0x30E0, 0x30AB, 0x30A4, 0x30EA, 0x30AB, 0x30E9, 0x30C3, 0x30C8, 0x30AB, 0x30ED, 0x30EA, 0x30FC, 0x30ED, 0x30F3, 0x30AB, 0x30F3, 0x30DE, 0x30AB, 0x30AD, 0x30AB, 0x30CB, 0x30FC, 0x30AD, 0x30AD, 0x30E5,
                               0x30EA, 0x30FC, 0x30EB, 0x30FC, 0x30AD, 0x30BF, 0x30AD, 0x30ED, 0x30AD, 0x30ED, 0x30E9, 0x30E0, 0x30AF, 0x30AD, 0x30ED, 0x30E1, 0x30FC, 0x30C8, 0x30EB, 0x30AD, 0x30ED, 0x30EF, 0x30C3, 0x30C8, 0x30E9, 0x30E0, 0x30AF, 0x30E9, 0x30E0, 0x30C8, 0x30F3, 0x30AF,
                               0x30AF, 0x30EB, 0x30A4, 0x30ED, 0x30BB, 0x30AF, 0x30ED, 0x30FC, 0x30CD, 0x30B1, 0x30FC, 0x30B9, 0x30B3, 0x30EB, 0x30CA, 0x30B3, 0x30FC, 0x30DB, 0x30B5, 0x30A4, 0x30AF, 0x30EB };
static u32 unacc_data172[] = { 0x30B5, 0x30F3, 0x30C1, 0x30FC, 0x30E0, 0x30B7, 0x30EA, 0x30F3, 0x30AF, 0x30BB, 0x30F3, 0x30C1, 0x30BB, 0x30F3, 0x30C8, 0x30FC, 0x30B9, 0x30BF, 0x30B7, 0x30C6, 0x30EB, 0x30C8, 0x30C8, 0x30F3, 0x30CA, 0x30CE, 0x30CE, 0x30C3, 0x30C8, 0x30CF, 0x30A4, 0x30C4,
                               0x30FC, 0x30BB, 0x30F3, 0x30C8, 0x30CF, 0x30FC, 0x30C4, 0x30CF, 0x30FC, 0x30EC, 0x30EB, 0x30CF, 0x30A2, 0x30B9, 0x30C8, 0x30EB, 0x30D2, 0x30AF, 0x30EB, 0x30D2, 0x30B3, 0x30D2, 0x30EB, 0x30D2, 0x30D5, 0x30A1, 0x30E9, 0x30C3, 0x30C8, 0x30D5, 0x30A3, 0x30FC,
                               0x30C8, 0x30C3, 0x30B7, 0x30A7, 0x30EB, 0x30D5, 0x30D5, 0x30E9, 0x30F3, 0x30D8, 0x30AF, 0x30BF, 0x30FC, 0x30EB, 0x30BD, 0x30D8, 0x30CB, 0x30D2, 0x30D8, 0x30D8, 0x30EB, 0x30C4, 0x30F3, 0x30B9, 0x30D8, 0x30FC, 0x30D8, 0x30B7, 0x30FC, 0x30BF, 0x30D8, 0x30A4,
                               0x30F3, 0x30C8, 0x30DB, 0x30EB, 0x30C8, 0x30DB, 0x30DB, 0x30F3 };
static u32 unacc_data173[] = { 0x30F3, 0x30DB, 0x30C8, 0x30DB, 0x30FC, 0x30EB, 0x30DB, 0x30FC, 0x30F3, 0x30DE, 0x30A4, 0x30AF, 0x30ED, 0x30DE, 0x30A4, 0x30EB, 0x30DE, 0x30C3, 0x30CF, 0x30DE, 0x30EB, 0x30AF, 0x30DE, 0x30F3, 0x30B7, 0x30E7, 0x30F3, 0x30DF, 0x30AF, 0x30ED, 0x30F3, 0x30DF,
                               0x30EA, 0x30DF, 0x30EA, 0x30FC, 0x30EB, 0x30CF, 0x30E1, 0x30AB, 0x30E1, 0x30C8, 0x30F3, 0x30AB, 0x30E1, 0x30FC, 0x30C8, 0x30EB, 0x30E4, 0x30FC, 0x30C8, 0x30E4, 0x30FC, 0x30EB, 0x30E6, 0x30A2, 0x30F3, 0x30EA, 0x30C3, 0x30C8, 0x30EB, 0x30EA, 0x30E9, 0x30EB,
                               0x30FC, 0x30D2, 0x30EB, 0x30FC, 0x30EB, 0x30D5, 0x30EC, 0x30E0, 0x30EC, 0x30F3, 0x30C8, 0x30F3, 0x30B1, 0x30EF, 0x30C3, 0x30C8, 0x0030, 0x70B9, 0x0031, 0x70B9, 0x0032, 0x70B9, 0x0033, 0x70B9, 0x0034, 0x70B9, 0x0035, 0x70B9, 0x0036, 0x70B9, 0x0037, 0x70B9 };
static u32 unacc_data174[] = { 0x0038, 0x70B9, 0x0039, 0x70B9, 0x0031, 0x0030, 0x70B9, 0x0031, 0x0031, 0x70B9, 0x0031, 0x0032, 0x70B9, 0x0031, 0x0033, 0x70B9, 0x0031, 0x0034, 0x70B9, 0x0031, 0x0035, 0x70B9, 0x0031, 0x0036, 0x70B9, 0x0031, 0x0037, 0x70B9, 0x0031, 0x0038, 0x70B9, 0x0031,
                               0x0039, 0x70B9, 0x0032, 0x0030, 0x70B9, 0x0032, 0x0031, 0x70B9, 0x0032, 0x0032, 0x70B9, 0x0032, 0x0033, 0x70B9, 0x0032, 0x0034, 0x70B9, 0x0068, 0x0050, 0x0061, 0x0064, 0x0061, 0x0041, 0x0055, 0x0062, 0x0061, 0x0072, 0x006F, 0x0056, 0x0070, 0x0063, 0x0064,
                               0x006D, 0x0064, 0x006D, 0x0032, 0x0064, 0x006D, 0x0033, 0x0049, 0x0055, 0x5E73, 0x6210, 0x662D, 0x548C, 0x5927, 0x6B63, 0x660E, 0x6CBB, 0x682A, 0x5F0F, 0x4F1A, 0x793E };
static u32 unacc_data175[] = { 0x0070, 0x0041, 0x006E, 0x0041, 0x03BC, 0x0041, 0x006D, 0x0041, 0x006B, 0x0041, 0x004B, 0x0042, 0x004D, 0x0042, 0x0047, 0x0042, 0x0063, 0x0061, 0x006C, 0x006B, 0x0063, 0x0061, 0x006C, 0x0070, 0x0046, 0x006E, 0x0046, 0x03BC, 0x0046, 0x03BC, 0x0067, 0x006D,
                               0x0067, 0x006B, 0x0067, 0x0048, 0x007A, 0x006B, 0x0048, 0x007A, 0x004D, 0x0048, 0x007A, 0x0047, 0x0048, 0x007A, 0x0054, 0x0048, 0x007A, 0x03BC, 0x006C, 0x006D, 0x006C, 0x0064, 0x006C, 0x006B, 0x006C, 0x0066, 0x006D, 0x006E, 0x006D, 0x03BC, 0x006D, 0x006D,
                               0x006D, 0x0063, 0x006D, 0x006B, 0x006D, 0x006D, 0x006D, 0x0032 };
static u32 unacc_data176[] = { 0x0063, 0x006D, 0x0032, 0x006D, 0x0032, 0x006B, 0x006D, 0x0032, 0x006D, 0x006D, 0x0033, 0x0063, 0x006D, 0x0033, 0x006D, 0x0033, 0x006B, 0x006D, 0x0033, 0x006D, 0x2215, 0x0073, 0x006D, 0x2215, 0x0073, 0x0032, 0x0050, 0x0061, 0x006B, 0x0050, 0x0061, 0x004D,
                               0x0050, 0x0061, 0x0047, 0x0050, 0x0061, 0x0072, 0x0061, 0x0064, 0x0072, 0x0061, 0x0064, 0x2215, 0x0073, 0x0072, 0x0061, 0x0064, 0x2215, 0x0073, 0x0032, 0x0070, 0x0073, 0x006E, 0x0073, 0x03BC, 0x0073, 0x006D, 0x0073, 0x0070, 0x0056, 0x006E, 0x0056, 0x03BC,
                               0x0056, 0x006D, 0x0056, 0x006B, 0x0056, 0x004D, 0x0056, 0x0070, 0x0057, 0x006E, 0x0057, 0x03BC, 0x0057, 0x006D, 0x0057, 0x006B, 0x0057, 0x004D, 0x0057 };
static u32 unacc_data177[] = { 0x006B, 0x03A9, 0x004D, 0x03A9, 0x0061, 0x002E, 0x006D, 0x002E, 0x0042, 0x0071, 0x0063, 0x0063, 0x0063, 0x0064, 0x0043, 0x2215, 0x006B, 0x0067, 0x0043, 0x006F, 0x002E, 0x0064, 0x0042, 0x0047, 0x0079, 0x0068, 0x0061, 0x0048, 0x0050, 0x0069, 0x006E, 0x004B,
                               0x004B, 0x004B, 0x004D, 0x006B, 0x0074, 0x006C, 0x006D, 0x006C, 0x006E, 0x006C, 0x006F, 0x0067, 0x006C, 0x0078, 0x006D, 0x0062, 0x006D, 0x0069, 0x006C, 0x006D, 0x006F, 0x006C, 0x0050, 0x0048, 0x0070, 0x002E, 0x006D, 0x002E, 0x0050, 0x0050, 0x004D, 0x0050,
                               0x0052, 0x0073, 0x0072, 0x0053, 0x0076, 0x0057, 0x0062, 0x0056, 0x2215, 0x006D, 0x0041, 0x2215, 0x006D };
static u32 unacc_data178[] = { 0x0031, 0x65E5, 0x0032, 0x65E5, 0x0033, 0x65E5, 0x0034, 0x65E5, 0x0035, 0x65E5, 0x0036, 0x65E5, 0x0037, 0x65E5, 0x0038, 0x65E5, 0x0039, 0x65E5, 0x0031, 0x0030, 0x65E5, 0x0031, 0x0031, 0x65E5, 0x0031, 0x0032, 0x65E5, 0x0031, 0x0033, 0x65E5, 0x0031, 0x0034,
                               0x65E5, 0x0031, 0x0035, 0x65E5, 0x0031, 0x0036, 0x65E5, 0x0031, 0x0037, 0x65E5, 0x0031, 0x0038, 0x65E5, 0x0031, 0x0039, 0x65E5, 0x0032, 0x0030, 0x65E5, 0x0032, 0x0031, 0x65E5, 0x0032, 0x0032, 0x65E5, 0x0032, 0x0033, 0x65E5, 0x0032, 0x0034, 0x65E5, 0x0032,
                               0x0035, 0x65E5, 0x0032, 0x0036, 0x65E5, 0x0032, 0x0037, 0x65E5, 0x0032, 0x0038, 0x65E5, 0x0032, 0x0039, 0x65E5, 0x0033, 0x0030, 0x65E5, 0x0033, 0x0031, 0x65E5, 0x0067, 0x0061, 0x006C };
static u32 unacc_data179[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x042B, 0x044B, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data180[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0422, 0x0442, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data181[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xA72C, 0xA72D, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xA738, 0xA739, 0x0000, 0x0000, 0x0000, 0x2184 };
static u32 unacc_data182[] = { 0x004B, 0x006B, 0x004B, 0x006B, 0x004B, 0x006B, 0x0000, 0x0000, 0x004C, 0x006C, 0x004F, 0x006F, 0x004F, 0x006F, 0x0000, 0x0000, 0x0050, 0x0070, 0x0050, 0x0070, 0x0050, 0x0070, 0x0051, 0x0071, 0x0051, 0x0071, 0x0000, 0x0000, 0x0000, 0x0000, 0x0056, 0x0076 };
static u32 unacc_data183[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x00DE, 0x00FE, 0x00DE, 0x00FE, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xA76F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data184[] = { 0x8C48, 0x66F4, 0x8ECA, 0x8CC8, 0x6ED1, 0x4E32, 0x53E5, 0x9F9C, 0x9F9C, 0x5951, 0x91D1, 0x5587, 0x5948, 0x61F6, 0x7669, 0x7F85, 0x863F, 0x87BA, 0x88F8, 0x908F, 0x6A02, 0x6D1B, 0x70D9, 0x73DE, 0x843D, 0x916A, 0x99F1, 0x4E82, 0x5375, 0x6B04, 0x721B, 0x862D };
static u32 unacc_data185[] = { 0x9E1E, 0x5D50, 0x6FEB, 0x85CD, 0x8964, 0x62C9, 0x81D8, 0x881F, 0x5ECA, 0x6717, 0x6D6A, 0x72FC, 0x90CE, 0x4F86, 0x51B7, 0x52DE, 0x64C4, 0x6AD3, 0x7210, 0x76E7, 0x8001, 0x8606, 0x865C, 0x8DEF, 0x9732, 0x9B6F, 0x9DFA, 0x788C, 0x797F, 0x7DA0, 0x83C9, 0x9304 };
static u32 unacc_data186[] = { 0x9E7F, 0x8AD6, 0x58DF, 0x5F04, 0x7C60, 0x807E, 0x7262, 0x78CA, 0x8CC2, 0x96F7, 0x58D8, 0x5C62, 0x6A13, 0x6DDA, 0x6F0F, 0x7D2F, 0x7E37, 0x964B, 0x52D2, 0x808B, 0x51DC, 0x51CC, 0x7A1C, 0x7DBE, 0x83F1, 0x9675, 0x8B80, 0x62CF, 0x6A02, 0x8AFE, 0x4E39, 0x5BE7 };
static u32 unacc_data187[] = { 0x6012, 0x7387, 0x7570, 0x5317, 0x78FB, 0x4FBF, 0x5FA9, 0x4E0D, 0x6CCC, 0x6578, 0x7D22, 0x53C3, 0x585E, 0x7701, 0x8449, 0x8AAA, 0x6BBA, 0x8FB0, 0x6C88, 0x62FE, 0x82E5, 0x63A0, 0x7565, 0x4EAE, 0x5169, 0x51C9, 0x6881, 0x7CE7, 0x826F, 0x8AD2, 0x91CF, 0x52F5 };
static u32 unacc_data188[] = { 0x5442, 0x5973, 0x5EEC, 0x65C5, 0x6FFE, 0x792A, 0x95AD, 0x9A6A, 0x9E97, 0x9ECE, 0x529B, 0x66C6, 0x6B77, 0x8F62, 0x5E74, 0x6190, 0x6200, 0x649A, 0x6F23, 0x7149, 0x7489, 0x79CA, 0x7DF4, 0x806F, 0x8F26, 0x84EE, 0x9023, 0x934A, 0x5217, 0x52A3, 0x54BD, 0x70C8 };
static u32 unacc_data189[] = { 0x88C2, 0x8AAA, 0x5EC9, 0x5FF5, 0x637B, 0x6BAE, 0x7C3E, 0x7375, 0x4EE4, 0x56F9, 0x5BE7, 0x5DBA, 0x601C, 0x73B2, 0x7469, 0x7F9A, 0x8046, 0x9234, 0x96F6, 0x9748, 0x9818, 0x4F8B, 0x79AE, 0x91B4, 0x96B8, 0x60E1, 0x4E86, 0x50DA, 0x5BEE, 0x5C3F, 0x6599, 0x6A02 };
static u32 unacc_data190[] = { 0x71CE, 0x7642, 0x84FC, 0x907C, 0x9F8D, 0x6688, 0x962E, 0x5289, 0x677B, 0x67F3, 0x6D41, 0x6E9C, 0x7409, 0x7559, 0x786B, 0x7D10, 0x985E, 0x516D, 0x622E, 0x9678, 0x502B, 0x5D19, 0x6DEA, 0x8F2A, 0x5F8B, 0x6144, 0x6817, 0x7387, 0x9686, 0x5229, 0x540F, 0x5C65 };
static u32 unacc_data191[] = { 0x6613, 0x674E, 0x68A8, 0x6CE5, 0x7406, 0x75E2, 0x7F79, 0x88CF, 0x88E1, 0x91CC, 0x96E2, 0x533F, 0x6EBA, 0x541D, 0x71D0, 0x7498, 0x85FA, 0x96A3, 0x9C57, 0x9E9F, 0x6797, 0x6DCB, 0x81E8, 0x7ACB, 0x7B20, 0x7C92, 0x72C0, 0x7099, 0x8B58, 0x4EC0, 0x8336, 0x523A };
static u32 unacc_data192[] = { 0x5207, 0x5EA6, 0x62D3, 0x7CD6, 0x5B85, 0x6D1E, 0x66B4, 0x8F3B, 0x884C, 0x964D, 0x898B, 0x5ED3, 0x5140, 0x55C0, 0x0000, 0x0000, 0x585A, 0x0000, 0x6674, 0x0000, 0x0000, 0x51DE, 0x732A, 0x76CA, 0x793C, 0x795E, 0x7965, 0x798F, 0x9756, 0x7CBE, 0x7FBD, 0x0000 };
static u32 unacc_data193[] = { 0x8612, 0x0000, 0x8AF8, 0x0000, 0x0000, 0x9038, 0x90FD, 0x0000, 0x0000, 0x0000, 0x98EF, 0x98FC, 0x9928, 0x9DB4, 0x0000, 0x0000, 0x4FAE, 0x50E7, 0x514D, 0x52C9, 0x52E4, 0x5351, 0x559D, 0x5606, 0x5668, 0x5840, 0x58A8, 0x5C64, 0x5C6E, 0x6094, 0x6168, 0x618E };
static u32 unacc_data194[] = { 0x61F2, 0x654F, 0x65E2, 0x6691, 0x6885, 0x6D77, 0x6E1A, 0x6F22, 0x716E, 0x722B, 0x7422, 0x7891, 0x793E, 0x7949, 0x7948, 0x7950, 0x7956, 0x795D, 0x798D, 0x798E, 0x7A40, 0x7A81, 0x7BC0, 0x7DF4, 0x7E09, 0x7E41, 0x7F72, 0x8005, 0x81ED, 0x8279, 0x8279, 0x8457 };
static u32 unacc_data195[] = { 0x8910, 0x8996, 0x8B01, 0x8B39, 0x8CD3, 0x8D08, 0x8FB6, 0x9038, 0x96E3, 0x97FF, 0x983B, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x4E26, 0x51B5, 0x5168, 0x4F80, 0x5145, 0x5180, 0x52C7, 0x52FA, 0x559D, 0x5555, 0x5599, 0x55E2, 0x585A, 0x58B3, 0x5944, 0x5954 };
static u32 unacc_data196[] = { 0x5A62, 0x5B28, 0x5ED2, 0x5ED9, 0x5F69, 0x5FAD, 0x60D8, 0x614E, 0x6108, 0x618E, 0x6160, 0x61F2, 0x6234, 0x63C4, 0x641C, 0x6452, 0x6556, 0x6674, 0x6717, 0x671B, 0x6756, 0x6B79, 0x6BBA, 0x6D41, 0x6EDB, 0x6ECB, 0x6F22, 0x701E, 0x716E, 0x77A7, 0x7235, 0x72AF };
static u32 unacc_data197[] = { 0x732A, 0x7471, 0x7506, 0x753B, 0x761D, 0x761F, 0x76CA, 0x76DB, 0x76F4, 0x774A, 0x7740, 0x78CC, 0x7AB1, 0x7BC0, 0x7C7B, 0x7D5B, 0x7DF4, 0x7F3E, 0x8005, 0x8352, 0x83EF, 0x8779, 0x8941, 0x8986, 0x8996, 0x8ABF, 0x8AF8, 0x8ACB, 0x8B01, 0x8AFE, 0x8AED, 0x8B39 };
// Some data don't fit into a u16 (unsigned short); I switched the table to hold u32 values, doubling its size.
// An alternative would be to switch back the table to u16 values and change the large values in the next row to 0x0000, as commented below
static u32 unacc_data198[] = { 0x8B8A, 0x8D08, 0x8F38, 0x9072, 0x9199, 0x9276, 0x967C, 0x96E3, 0x9756, 0x97DB, 0x97FF, 0x980B, 0x983B, 0x9B12, 0x9F9C,0x2284A,0x22844,0x233D5, 0x3B9D, 0x4018, 0x4039,0x25249,0x25CD0,0x27ED3, 0x9F43, 0x9F8E, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
// static const u16 unacc_data198[] = { 0x8B8A, 0x8D08, 0x8F38, 0x9072, 0x9199, 0x9276, 0x967C, 0x96E3, 0x9756, 0x97DB, 0x97FF, 0x980B, 0x983B, 0x9B12, 0x9F9C, 0x0000, 0x0000, 0x0000, 0x3B9D, 0x4018, 0x4039, 0x0000, 0x0000, 0x0000, 0x9F43, 0x9F8E, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data199[] = { 0x0066, 0x0066, 0x0066, 0x0069, 0x0066, 0x006C, 0x0066, 0x0066, 0x0069, 0x0066, 0x0066, 0x006C, 0x0074, 0x0073, 0x0073, 0x0074, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0574, 0x0576, 0x0574, 0x0565,
                               0x0574, 0x056B, 0x057E, 0x0576, 0x0574, 0x056D, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x05D9, 0x0000, 0x05F2 };
static u32 unacc_data200[] = { 0x05E2, 0x05D0, 0x05D3, 0x05D4, 0x05DB, 0x05DC, 0x05DD, 0x05E8, 0x05EA, 0x002B, 0x05E9, 0x05E9, 0x05E9, 0x05E9, 0x05D0, 0x05D0, 0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4, 0x05D5, 0x05D6, 0x0000, 0x05D8, 0x05D9, 0x05DA, 0x05DB, 0x05DC, 0x0000, 0x05DE, 0x0000 };
static u32 unacc_data201[] = { 0x05E0, 0x05E1, 0x0000, 0x05E3, 0x05E4, 0x0000, 0x05E6, 0x05E7, 0x05E8, 0x05E9, 0x05EA, 0x05D5, 0x05D1, 0x05DB, 0x05E4, 0x05D0, 0x05DC, 0x0671, 0x0671, 0x067B, 0x067B, 0x067B, 0x067B, 0x067E, 0x067E, 0x067E, 0x067E, 0x0680, 0x0680, 0x0680, 0x0680, 0x067A,
                               0x067A };
static u32 unacc_data202[] = { 0x067A, 0x067A, 0x067F, 0x067F, 0x067F, 0x067F, 0x0679, 0x0679, 0x0679, 0x0679, 0x06A4, 0x06A4, 0x06A4, 0x06A4, 0x06A6, 0x06A6, 0x06A6, 0x06A6, 0x0684, 0x0684, 0x0684, 0x0684, 0x0683, 0x0683, 0x0683, 0x0683, 0x0686, 0x0686, 0x0686, 0x0686, 0x0687, 0x0687 };
static u32 unacc_data203[] = { 0x0687, 0x0687, 0x068D, 0x068D, 0x068C, 0x068C, 0x068E, 0x068E, 0x0688, 0x0688, 0x0698, 0x0698, 0x0691, 0x0691, 0x06A9, 0x06A9, 0x06A9, 0x06A9, 0x06AF, 0x06AF, 0x06AF, 0x06AF, 0x06B3, 0x06B3, 0x06B3, 0x06B3, 0x06B1, 0x06B1, 0x06B1, 0x06B1, 0x06BA, 0x06BA };
static u32 unacc_data204[] = { 0x06BB, 0x06BB, 0x06BB, 0x06BB, 0x06D5, 0x06D5, 0x06C1, 0x06C1, 0x06C1, 0x06C1, 0x06BE, 0x06BE, 0x06BE, 0x06BE, 0x06D2, 0x06D2, 0x06D2, 0x06D2, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data205[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x06AD, 0x06AD, 0x06AD, 0x06AD, 0x06C7, 0x06C7, 0x06C6, 0x06C6, 0x06C8, 0x06C8, 0x06C7, 0x0674, 0x06CB,
                               0x06CB };
static u32 unacc_data206[] = { 0x06C5, 0x06C5, 0x06C9, 0x06C9, 0x06D0, 0x06D0, 0x06D0, 0x06D0, 0x0649, 0x0649, 0x0627, 0x064A, 0x0627, 0x064A, 0x06D5, 0x064A, 0x06D5, 0x064A, 0x0648, 0x064A, 0x0648, 0x064A, 0x06C7, 0x064A, 0x06C7, 0x064A, 0x06C6, 0x064A, 0x06C6, 0x064A, 0x06C8, 0x064A,
                               0x06C8, 0x064A, 0x06D0, 0x064A, 0x06D0, 0x064A, 0x06D0, 0x064A, 0x0649, 0x064A, 0x0649, 0x064A, 0x0649, 0x064A, 0x06CC, 0x06CC, 0x06CC, 0x06CC };
static u32 unacc_data207[] = { 0x062C, 0x064A, 0x062D, 0x064A, 0x0645, 0x064A, 0x0649, 0x064A, 0x064A, 0x064A, 0x0628, 0x062C, 0x0628, 0x062D, 0x0628, 0x062E, 0x0628, 0x0645, 0x0628, 0x0649, 0x0628, 0x064A, 0x062A, 0x062C, 0x062A, 0x062D, 0x062A, 0x062E, 0x062A, 0x0645, 0x062A, 0x0649,
                               0x062A, 0x064A, 0x062B, 0x062C, 0x062B, 0x0645, 0x062B, 0x0649, 0x062B, 0x064A, 0x062C, 0x062D, 0x062C, 0x0645, 0x062D, 0x062C, 0x062D, 0x0645, 0x062E, 0x062C, 0x062E, 0x062D, 0x062E, 0x0645, 0x0633, 0x062C, 0x0633, 0x062D, 0x0633, 0x062E, 0x0633, 0x0645 };
static u32 unacc_data208[] = { 0x0635, 0x062D, 0x0635, 0x0645, 0x0636, 0x062C, 0x0636, 0x062D, 0x0636, 0x062E, 0x0636, 0x0645, 0x0637, 0x062D, 0x0637, 0x0645, 0x0638, 0x0645, 0x0639, 0x062C, 0x0639, 0x0645, 0x063A, 0x062C, 0x063A, 0x0645, 0x0641, 0x062C, 0x0641, 0x062D, 0x0641, 0x062E,
                               0x0641, 0x0645, 0x0641, 0x0649, 0x0641, 0x064A, 0x0642, 0x062D, 0x0642, 0x0645, 0x0642, 0x0649, 0x0642, 0x064A, 0x0643, 0x0627, 0x0643, 0x062C, 0x0643, 0x062D, 0x0643, 0x062E, 0x0643, 0x0644, 0x0643, 0x0645, 0x0643, 0x0649, 0x0643, 0x064A, 0x0644, 0x062C };
static u32 unacc_data209[] = { 0x0644, 0x062D, 0x0644, 0x062E, 0x0644, 0x0645, 0x0644, 0x0649, 0x0644, 0x064A, 0x0645, 0x062C, 0x0645, 0x062D, 0x0645, 0x062E, 0x0645, 0x0645, 0x0645, 0x0649, 0x0645, 0x064A, 0x0646, 0x062C, 0x0646, 0x062D, 0x0646, 0x062E, 0x0646, 0x0645, 0x0646, 0x0649,
                               0x0646, 0x064A, 0x0647, 0x062C, 0x0647, 0x0645, 0x0647, 0x0649, 0x0647, 0x064A, 0x064A, 0x062C, 0x064A, 0x062D, 0x064A, 0x062E, 0x064A, 0x0645, 0x064A, 0x0649, 0x064A, 0x064A, 0x0630, 0x0631, 0x0649, 0x0020, 0x0020 };
static u32 unacc_data210[] = { 0x0020, 0x0020, 0x0020, 0x0020, 0x0631, 0x064A, 0x0632, 0x064A, 0x0645, 0x064A, 0x0646, 0x064A, 0x0649, 0x064A, 0x064A, 0x064A, 0x0628, 0x0631, 0x0628, 0x0632, 0x0628, 0x0645, 0x0628, 0x0646, 0x0628, 0x0649, 0x0628, 0x064A, 0x062A, 0x0631, 0x062A, 0x0632,
                               0x062A, 0x0645, 0x062A, 0x0646, 0x062A, 0x0649, 0x062A, 0x064A, 0x062B, 0x0631, 0x062B, 0x0632, 0x062B, 0x0645, 0x062B, 0x0646, 0x062B, 0x0649, 0x062B, 0x064A, 0x0641, 0x0649, 0x0641, 0x064A, 0x0642, 0x0649, 0x0642, 0x064A };
static u32 unacc_data211[] = { 0x0643, 0x0627, 0x0643, 0x0644, 0x0643, 0x0645, 0x0643, 0x0649, 0x0643, 0x064A, 0x0644, 0x0645, 0x0644, 0x0649, 0x0644, 0x064A, 0x0645, 0x0627, 0x0645, 0x0645, 0x0646, 0x0631, 0x0646, 0x0632, 0x0646, 0x0645, 0x0646, 0x0646, 0x0646, 0x0649, 0x0646, 0x064A,
                               0x0649, 0x064A, 0x0631, 0x064A, 0x0632, 0x064A, 0x0645, 0x064A, 0x0646, 0x064A, 0x0649, 0x064A, 0x064A, 0x062C, 0x064A, 0x062D, 0x064A, 0x062E, 0x064A, 0x0645, 0x064A, 0x0647, 0x064A, 0x0628, 0x062C, 0x0628, 0x062D, 0x0628, 0x062E, 0x0628, 0x0645 };
static u32 unacc_data212[] = { 0x0628, 0x0647, 0x062A, 0x062C, 0x062A, 0x062D, 0x062A, 0x062E, 0x062A, 0x0645, 0x062A, 0x0647, 0x062B, 0x0645, 0x062C, 0x062D, 0x062C, 0x0645, 0x062D, 0x062C, 0x062D, 0x0645, 0x062E, 0x062C, 0x062E, 0x0645, 0x0633, 0x062C, 0x0633, 0x062D, 0x0633, 0x062E,
                               0x0633, 0x0645, 0x0635, 0x062D, 0x0635, 0x062E, 0x0635, 0x0645, 0x0636, 0x062C, 0x0636, 0x062D, 0x0636, 0x062E, 0x0636, 0x0645, 0x0637, 0x062D, 0x0638, 0x0645, 0x0639, 0x062C, 0x0639, 0x0645, 0x063A, 0x062C, 0x063A, 0x0645, 0x0641, 0x062C, 0x0641, 0x062D };
static u32 unacc_data213[] = { 0x0641, 0x062E, 0x0641, 0x0645, 0x0642, 0x062D, 0x0642, 0x0645, 0x0643, 0x062C, 0x0643, 0x062D, 0x0643, 0x062E, 0x0643, 0x0644, 0x0643, 0x0645, 0x0644, 0x062C, 0x0644, 0x062D, 0x0644, 0x062E, 0x0644, 0x0645, 0x0644, 0x0647, 0x0645, 0x062C, 0x0645, 0x062D,
                               0x0645, 0x062E, 0x0645, 0x0645, 0x0646, 0x062C, 0x0646, 0x062D, 0x0646, 0x062E, 0x0646, 0x0645, 0x0646, 0x0647, 0x0647, 0x062C, 0x0647, 0x0645, 0x0647, 0x064A, 0x062C, 0x064A, 0x062D, 0x064A, 0x062E, 0x064A, 0x0645, 0x064A, 0x0647, 0x0645, 0x064A };
static u32 unacc_data214[] = { 0x0647, 0x064A, 0x0628, 0x0645, 0x0628, 0x0647, 0x062A, 0x0645, 0x062A, 0x0647, 0x062B, 0x0645, 0x062B, 0x0647, 0x0633, 0x0645, 0x0633, 0x0647, 0x0634, 0x0645, 0x0634, 0x0647, 0x0643, 0x0644, 0x0643, 0x0645, 0x0644, 0x0645, 0x0646, 0x0645, 0x0646, 0x0647,
                               0x064A, 0x0645, 0x064A, 0x0647, 0x0640, 0x0640, 0x0640, 0x0637, 0x0649, 0x0637, 0x064A, 0x0639, 0x0649, 0x0639, 0x064A, 0x063A, 0x0649, 0x063A, 0x064A, 0x0633, 0x0649, 0x0633, 0x064A, 0x0634, 0x0649, 0x0634, 0x064A, 0x062D, 0x0649 };
static u32 unacc_data215[] = { 0x062D, 0x064A, 0x062C, 0x0649, 0x062C, 0x064A, 0x062E, 0x0649, 0x062E, 0x064A, 0x0635, 0x0649, 0x0635, 0x064A, 0x0636, 0x0649, 0x0636, 0x064A, 0x0634, 0x062C, 0x0634, 0x062D, 0x0634, 0x062E, 0x0634, 0x0645, 0x0634, 0x0631, 0x0633, 0x0631, 0x0635, 0x0631,
                               0x0636, 0x0631, 0x0637, 0x0649, 0x0637, 0x064A, 0x0639, 0x0649, 0x0639, 0x064A, 0x063A, 0x0649, 0x063A, 0x064A, 0x0633, 0x0649, 0x0633, 0x064A, 0x0634, 0x0649, 0x0634, 0x064A, 0x062D, 0x0649, 0x062D, 0x064A, 0x062C, 0x0649, 0x062C, 0x064A, 0x062E, 0x0649 };
static u32 unacc_data216[] = { 0x062E, 0x064A, 0x0635, 0x0649, 0x0635, 0x064A, 0x0636, 0x0649, 0x0636, 0x064A, 0x0634, 0x062C, 0x0634, 0x062D, 0x0634, 0x062E, 0x0634, 0x0645, 0x0634, 0x0631, 0x0633, 0x0631, 0x0635, 0x0631, 0x0636, 0x0631, 0x0634, 0x062C, 0x0634, 0x062D, 0x0634, 0x062E,
                               0x0634, 0x0645, 0x0633, 0x0647, 0x0634, 0x0647, 0x0637, 0x0645, 0x0633, 0x062C, 0x0633, 0x062D, 0x0633, 0x062E, 0x0634, 0x062C, 0x0634, 0x062D, 0x0634, 0x062E, 0x0637, 0x0645, 0x0638, 0x0645, 0x0627, 0x0627, 0x0000, 0x0000 };
static u32 unacc_data217[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x062A, 0x062C, 0x0645, 0x062A, 0x062D, 0x062C, 0x062A, 0x062D, 0x062C, 0x062A, 0x062D, 0x0645, 0x062A, 0x062E, 0x0645, 0x062A,
                               0x0645, 0x062C, 0x062A, 0x0645, 0x062D, 0x062A, 0x0645, 0x062E, 0x062C, 0x0645, 0x062D, 0x062C, 0x0645, 0x062D, 0x062D, 0x0645, 0x064A, 0x062D, 0x0645, 0x0649, 0x0633, 0x062D, 0x062C, 0x0633, 0x062C, 0x062D, 0x0633, 0x062C, 0x0649, 0x0633, 0x0645, 0x062D };
static u32 unacc_data218[] = { 0x0633, 0x0645, 0x062D, 0x0633, 0x0645, 0x062C, 0x0633, 0x0645, 0x0645, 0x0633, 0x0645, 0x0645, 0x0635, 0x062D, 0x062D, 0x0635, 0x062D, 0x062D, 0x0635, 0x0645, 0x0645, 0x0634, 0x062D, 0x0645, 0x0634, 0x062D, 0x0645, 0x0634, 0x062C, 0x064A, 0x0634, 0x0645,
                               0x062E, 0x0634, 0x0645, 0x062E, 0x0634, 0x0645, 0x0645, 0x0634, 0x0645, 0x0645, 0x0636, 0x062D, 0x0649, 0x0636, 0x062E, 0x0645, 0x0636, 0x062E, 0x0645, 0x0637, 0x0645, 0x062D, 0x0637, 0x0645, 0x062D, 0x0637, 0x0645, 0x0645, 0x0637, 0x0645, 0x064A, 0x0639,
                               0x062C, 0x0645, 0x0639, 0x0645, 0x0645, 0x0639, 0x0645, 0x0645, 0x0639, 0x0645, 0x0649, 0x063A, 0x0645, 0x0645, 0x063A, 0x0645, 0x064A, 0x063A, 0x0645, 0x0649, 0x0641, 0x062E, 0x0645, 0x0641, 0x062E, 0x0645, 0x0642, 0x0645, 0x062D, 0x0642, 0x0645, 0x0645 };
static u32 unacc_data219[] = { 0x0644, 0x062D, 0x0645, 0x0644, 0x062D, 0x064A, 0x0644, 0x062D, 0x0649, 0x0644, 0x062C, 0x062C, 0x0644, 0x062C, 0x062C, 0x0644, 0x062E, 0x0645, 0x0644, 0x062E, 0x0645, 0x0644, 0x0645, 0x062D, 0x0644, 0x0645, 0x062D, 0x0645, 0x062D, 0x062C, 0x0645, 0x062D,
                               0x0645, 0x0645, 0x062D, 0x064A, 0x0645, 0x062C, 0x062D, 0x0645, 0x062C, 0x0645, 0x0645, 0x062E, 0x062C, 0x0645, 0x062E, 0x0645, 0x0000, 0x0000, 0x0645, 0x062C, 0x062E, 0x0647, 0x0645, 0x062C, 0x0647, 0x0645, 0x0645, 0x0646, 0x062D, 0x0645, 0x0646, 0x062D,
                               0x0649, 0x0646, 0x062C, 0x0645, 0x0646, 0x062C, 0x0645, 0x0646, 0x062C, 0x0649, 0x0646, 0x0645, 0x064A, 0x0646, 0x0645, 0x0649, 0x064A, 0x0645, 0x0645, 0x064A, 0x0645, 0x0645, 0x0628, 0x062E, 0x064A, 0x062A, 0x062C, 0x064A };
static u32 unacc_data220[] = { 0x062A, 0x062C, 0x0649, 0x062A, 0x062E, 0x064A, 0x062A, 0x062E, 0x0649, 0x062A, 0x0645, 0x064A, 0x062A, 0x0645, 0x0649, 0x062C, 0x0645, 0x064A, 0x062C, 0x062D, 0x0649, 0x062C, 0x0645, 0x0649, 0x0633, 0x062E, 0x0649, 0x0635, 0x062D, 0x064A, 0x0634, 0x062D,
                               0x064A, 0x0636, 0x062D, 0x064A, 0x0644, 0x062C, 0x064A, 0x0644, 0x0645, 0x064A, 0x064A, 0x062D, 0x064A, 0x064A, 0x062C, 0x064A, 0x064A, 0x0645, 0x064A, 0x0645, 0x0645, 0x064A, 0x0642, 0x0645, 0x064A, 0x0646, 0x062D, 0x064A, 0x0642, 0x0645, 0x062D, 0x0644,
                               0x062D, 0x0645, 0x0639, 0x0645, 0x064A, 0x0643, 0x0645, 0x064A, 0x0646, 0x062C, 0x062D, 0x0645, 0x062E, 0x064A, 0x0644, 0x062C, 0x0645, 0x0643, 0x0645, 0x0645, 0x0644, 0x062C, 0x0645, 0x0646, 0x062C, 0x062D, 0x062C, 0x062D, 0x064A, 0x062D, 0x062C, 0x064A };
static u32 unacc_data221[] = { 0x0645, 0x062C, 0x064A, 0x0641, 0x0645, 0x064A, 0x0628, 0x062D, 0x064A, 0x0643, 0x0645, 0x0645, 0x0639, 0x062C, 0x0645, 0x0635, 0x0645, 0x0645, 0x0633, 0x062E, 0x064A, 0x0646, 0x062C, 0x064A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                               0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data222[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0635, 0x0644, 0x06D2, 0x0642, 0x0644, 0x06D2, 0x0627, 0x0644, 0x0644, 0x0647, 0x0627, 0x0643, 0x0628, 0x0631, 0x0645, 0x062D,
                               0x0645, 0x062F, 0x0635, 0x0644, 0x0639, 0x0645, 0x0631, 0x0633, 0x0648, 0x0644, 0x0639, 0x0644, 0x064A, 0x0647, 0x0648, 0x0633, 0x0644, 0x0645, 0x0635, 0x0644, 0x0649, 0x0635, 0x0644, 0x0649, 0x0020, 0x0627, 0x0644, 0x0644, 0x0647, 0x0020, 0x0639, 0x0644,
                               0x064A, 0x0647, 0x0020, 0x0648, 0x0633, 0x0644, 0x0645, 0x062C, 0x0644, 0x0020, 0x062C, 0x0644, 0x0627, 0x0644, 0x0647, 0x0631, 0x06CC, 0x0627, 0x0644, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data223[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x002C, 0x3001, 0x3002, 0x003A, 0x003B, 0x0021, 0x003F, 0x3016, 0x3017, 0x002E, 0x002E, 0x002E, 0x0000, 0x0000, 0x0000, 0x0000,
                               0x0000, 0x0000 };
static u32 unacc_data224[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x002E, 0x002E, 0x2014, 0x2013, 0x005F, 0x005F, 0x0028, 0x0029, 0x007B, 0x007D, 0x3014, 0x3015, 0x3010, 0x3011, 0x300A, 0x300B,
                               0x3008 };
static u32 unacc_data225[] = { 0x3009, 0x300C, 0x300D, 0x300E, 0x300F, 0x0000, 0x0000, 0x005B, 0x005D, 0x0020, 0x0020, 0x0020, 0x0020, 0x005F, 0x005F, 0x005F, 0x002C, 0x3001, 0x002E, 0x0000, 0x003B, 0x003A, 0x003F, 0x0021, 0x2014, 0x0028, 0x0029, 0x007B, 0x007D, 0x3014, 0x3015, 0x0023 };
static u32 unacc_data226[] = { 0x0026, 0x002A, 0x002B, 0x002D, 0x003C, 0x003E, 0x003D, 0x0000, 0x005C, 0x0024, 0x0025, 0x0040, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0640, 0x0020, 0x0000, 0x0020, 0x0000, 0x0020, 0x0640, 0x0020, 0x0640, 0x0020, 0x0640, 0x0020, 0x0640, 0x0020, 0x0640 };
static u32 unacc_data227[] = { 0x0621, 0x0627, 0x0627, 0x0627, 0x0627, 0x0648, 0x0648, 0x0627, 0x0627, 0x064A, 0x064A, 0x064A, 0x064A, 0x0627, 0x0627, 0x0628, 0x0628, 0x0628, 0x0628, 0x0629, 0x0629, 0x062A, 0x062A, 0x062A, 0x062A, 0x062B, 0x062B, 0x062B, 0x062B, 0x062C, 0x062C, 0x062C };
static u32 unacc_data228[] = { 0x062C, 0x062D, 0x062D, 0x062D, 0x062D, 0x062E, 0x062E, 0x062E, 0x062E, 0x062F, 0x062F, 0x0630, 0x0630, 0x0631, 0x0631, 0x0632, 0x0632, 0x0633, 0x0633, 0x0633, 0x0633, 0x0634, 0x0634, 0x0634, 0x0634, 0x0635, 0x0635, 0x0635, 0x0635, 0x0636, 0x0636, 0x0636 };
static u32 unacc_data229[] = { 0x0636, 0x0637, 0x0637, 0x0637, 0x0637, 0x0638, 0x0638, 0x0638, 0x0638, 0x0639, 0x0639, 0x0639, 0x0639, 0x063A, 0x063A, 0x063A, 0x063A, 0x0641, 0x0641, 0x0641, 0x0641, 0x0642, 0x0642, 0x0642, 0x0642, 0x0643, 0x0643, 0x0643, 0x0643, 0x0644, 0x0644, 0x0644 };
static u32 unacc_data230[] = { 0x0644, 0x0645, 0x0645, 0x0645, 0x0645, 0x0646, 0x0646, 0x0646, 0x0646, 0x0647, 0x0647, 0x0647, 0x0647, 0x0648, 0x0648, 0x0649, 0x0649, 0x064A, 0x064A, 0x064A, 0x064A, 0x0644, 0x0627, 0x0644, 0x0627, 0x0644, 0x0627, 0x0644, 0x0627, 0x0644, 0x0627, 0x0644,
                               0x0627, 0x0644, 0x0627, 0x0644, 0x0627, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data231[] = { 0x0000, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F };
static u32 unacc_data232[] = { 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F };
static u32 unacc_data233[] = { 0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x2985 };
static u32 unacc_data234[] = { 0x2986, 0x3002, 0x300C, 0x300D, 0x3001, 0x30FB, 0x30F2, 0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5, 0x30E7, 0x30C3, 0x30FC, 0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF, 0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD };
static u32 unacc_data235[] = { 0x30BF, 0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD, 0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF, 0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA, 0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F3, 0x0000, 0x0000 };
static u32 unacc_data236[] = { 0x1160, 0x1100, 0x1101, 0x11AA, 0x1102, 0x11AC, 0x11AD, 0x1103, 0x1104, 0x1105, 0x11B0, 0x11B1, 0x11B2, 0x11B3, 0x11B4, 0x11B5, 0x111A, 0x1106, 0x1107, 0x1108, 0x1121, 0x1109, 0x110A, 0x110B, 0x110C, 0x110D, 0x110E, 0x110F, 0x1110, 0x1111, 0x1112, 0x0000 };
static u32 unacc_data237[] = { 0x0000, 0x0000, 0x1161, 0x1162, 0x1163, 0x1164, 0x1165, 0x1166, 0x0000, 0x0000, 0x1167, 0x1168, 0x1169, 0x116A, 0x116B, 0x116C, 0x0000, 0x0000, 0x116D, 0x116E, 0x116F, 0x1170, 0x1171, 0x1172, 0x0000, 0x0000, 0x1173, 0x1174, 0x1175, 0x0000, 0x0000, 0x0000 };
static u32 unacc_data238[] = { 0x00A2, 0x00A3, 0x00AC, 0x0020, 0x00A6, 0x00A5, 0x20A9, 0x0000, 0x2502, 0x2190, 0x2191, 0x2192, 0x2193, 0x25A0, 0x25CB, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };

// This table has been modified to take care of full width characters
static const u32 *unacc_data_table[UNACC_BLOCK_COUNT] = {
unacc_data0,unacc_data1,unacc_data2,unacc_data3,unacc_data4,unacc_data5,unacc_data6,unacc_data7,unacc_data8,unacc_data9,
unacc_data10,unacc_data11,unacc_data12,unacc_data13,unacc_data14,unacc_data15,unacc_data16,unacc_data17,unacc_data18,unacc_data19,
unacc_data20,unacc_data21,unacc_data22,unacc_data23,unacc_data24,unacc_data25,unacc_data26,unacc_data27,unacc_data28,unacc_data29,
unacc_data30,unacc_data31,unacc_data32,unacc_data33,unacc_data34,unacc_data35,unacc_data36,unacc_data37,unacc_data38,unacc_data39,
unacc_data40,unacc_data41,unacc_data42,unacc_data43,unacc_data44,unacc_data45,unacc_data46,unacc_data47,unacc_data48,unacc_data49,
unacc_data50,unacc_data51,unacc_data52,unacc_data53,unacc_data54,unacc_data55,unacc_data56,unacc_data57,unacc_data58,unacc_data59,
unacc_data60,unacc_data61,unacc_data62,unacc_data63,unacc_data64,unacc_data65,unacc_data66,unacc_data67,unacc_data68,unacc_data69,
unacc_data70,unacc_data71,unacc_data72,unacc_data73,unacc_data74,unacc_data75,unacc_data76,unacc_data77,unacc_data78,unacc_data79,
unacc_data80,unacc_data81,unacc_data82,unacc_data83,unacc_data84,unacc_data85,unacc_data86,unacc_data87,unacc_data88,unacc_data89,
unacc_data90,unacc_data91,unacc_data92,unacc_data93,unacc_data94,unacc_data95,unacc_data96,unacc_data97,unacc_data98,unacc_data99,
unacc_data100,unacc_data101,unacc_data102,unacc_data103,unacc_data104,unacc_data105,unacc_data106,unacc_data107,unacc_data108,unacc_data109,
unacc_data110,unacc_data111,unacc_data112,unacc_data113,unacc_data114,unacc_data115,unacc_data116,unacc_data117,unacc_data118,unacc_data119,
unacc_data120,unacc_data121,unacc_data122,unacc_data123,unacc_data124,unacc_data125,unacc_data126,unacc_data127,unacc_data128,unacc_data129,
unacc_data130,unacc_data131,unacc_data132,unacc_data133,unacc_data134,unacc_data135,unacc_data136,unacc_data137,unacc_data138,unacc_data139,
unacc_data140,unacc_data141,unacc_data142,unacc_data143,unacc_data144,unacc_data145,unacc_data146,unacc_data147,unacc_data148,unacc_data149,
unacc_data150,unacc_data151,unacc_data152,unacc_data153,unacc_data154,unacc_data155,unacc_data156,unacc_data157,unacc_data158,unacc_data159,
unacc_data160,unacc_data161,unacc_data162,unacc_data163,unacc_data164,unacc_data165,unacc_data166,unacc_data167,unacc_data168,unacc_data169,
unacc_data170,unacc_data171,unacc_data172,unacc_data173,unacc_data174,unacc_data175,unacc_data176,unacc_data177,unacc_data178,unacc_data179,
unacc_data180,unacc_data181,unacc_data182,unacc_data183,unacc_data184,unacc_data185,unacc_data186,unacc_data187,unacc_data188,unacc_data189,
unacc_data190,unacc_data191,unacc_data192,unacc_data193,unacc_data194,unacc_data195,unacc_data196,unacc_data197,unacc_data198,unacc_data199,
unacc_data200,unacc_data201,unacc_data202,unacc_data203,unacc_data204,unacc_data205,unacc_data206,unacc_data207,unacc_data208,unacc_data209,
unacc_data210,unacc_data211,unacc_data212,unacc_data213,unacc_data214,unacc_data215,unacc_data216,unacc_data217,unacc_data218,unacc_data219,
unacc_data220,unacc_data221,unacc_data222,unacc_data223,unacc_data224,unacc_data225,unacc_data226,unacc_data227,unacc_data228,unacc_data229,
unacc_data230,unacc_data231,unacc_data232,unacc_data233,unacc_data234,unacc_data235,unacc_data236,unacc_data237,unacc_data238
};
/* Generated by builder. Do not modify. End unacc_tables */


SQLITE_PRIVATE u32 unifuzz_unacc(
    u32 c,
    u32 **p,
    int *l
){
    if (c < 0x10000) {
        u16 pos, index;
        index = unacc_indexes[c >> UNACC_BLOCK_SHIFT];
        pos = c & UNACC_BLOCK_MASK;
        *p = (u32 *) &(unacc_data_table[index][unacc_positions[index][pos]]);
        *l = ((**p == 0) ? 0 : unacc_positions[index][pos + 1] - unacc_positions[index][pos]);
    } else {
        *l = 0;
    }
    return c;
}


SQLITE_PRIVATE u32 unifuzz_fold_unacc(
    u32 c,
    u32 **p,
    int *l
){
    if (c < 0x10000) {
        u16 pos, index;
        pos = fold_data_table[fold_indexes[c >> FOLD_BLOCK_SHIFT]][c & FOLD_BLOCK_MASK];
        c = (u32) ((pos == 0) ? c : pos);
        index = unacc_indexes[c >> UNACC_BLOCK_SHIFT];
        pos = c & UNACC_BLOCK_MASK;
        *p = (u32 *) &(unacc_data_table[index][unacc_positions[index][pos]]);
        *l = ((**p == 0) ? 0 : unacc_positions[index][pos + 1] - unacc_positions[index][pos]);
    } else {
        *l = 0;
    }
    return c;
}


/* Generated by builder. Do not modify. Start lower_defines */
#define LOWER_BLOCK_SHIFT 6
#define LOWER_BLOCK_MASK ((1 << LOWER_BLOCK_SHIFT) - 1)
#define LOWER_BLOCK_SIZE (1 << LOWER_BLOCK_SHIFT)
#define LOWER_BLOCK_COUNT 43
#define LOWER_INDEXES_SIZE (0x10000 >> LOWER_BLOCK_SHIFT)
/* Generated by builder. Do not modify. End lower_defines */

/*  JCD lower  */
static const u16 lower_indexes[LOWER_INDEXES_SIZE] = {
   0,   1,   0,   2,   3,   4,   5,   6,   7,   8,   0,   0,   0,   9,  10,
  11,  12,  13,  14,  15,  16,  17,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,  18,  19,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  20,  21,  22,  23,  24,  25,  26,  27,   0,   0,   0,   0,  28,  29,  30,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  31,  32,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  33,  34,  35,  36,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,  37,  38,   0,  39,  40,  41,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  42,   0,   0
};

static const u16 lower_data0[]   = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data1[]   = { 0x0000, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
                                     0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data2[]   = { 0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
                                     0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x0000, 0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data3[]   = { 0x0101, 0x0000, 0x0103, 0x0000, 0x0105, 0x0000, 0x0107, 0x0000, 0x0109, 0x0000, 0x010B, 0x0000, 0x010D, 0x0000, 0x010F, 0x0000,
                                     0x0111, 0x0000, 0x0113, 0x0000, 0x0115, 0x0000, 0x0117, 0x0000, 0x0119, 0x0000, 0x011B, 0x0000, 0x011D, 0x0000, 0x011F, 0x0000,
                                     0x0121, 0x0000, 0x0123, 0x0000, 0x0125, 0x0000, 0x0127, 0x0000, 0x0129, 0x0000, 0x012B, 0x0000, 0x012D, 0x0000, 0x012F, 0x0000,
                                     0x0069, 0x0000, 0x0133, 0x0000, 0x0135, 0x0000, 0x0137, 0x0000, 0x0000, 0x013A, 0x0000, 0x013C, 0x0000, 0x013E, 0x0000, 0x0140 };
static const u16 lower_data4[]   = { 0x0000, 0x0142, 0x0000, 0x0144, 0x0000, 0x0146, 0x0000, 0x0148, 0x0000, 0x0000, 0x014B, 0x0000, 0x014D, 0x0000, 0x014F, 0x0000,
                                     0x0151, 0x0000, 0x0153, 0x0000, 0x0155, 0x0000, 0x0157, 0x0000, 0x0159, 0x0000, 0x015B, 0x0000, 0x015D, 0x0000, 0x015F, 0x0000,
                                     0x0161, 0x0000, 0x0163, 0x0000, 0x0165, 0x0000, 0x0167, 0x0000, 0x0169, 0x0000, 0x016B, 0x0000, 0x016D, 0x0000, 0x016F, 0x0000,
                                     0x0171, 0x0000, 0x0173, 0x0000, 0x0175, 0x0000, 0x0177, 0x0000, 0x00FF, 0x017A, 0x0000, 0x017C, 0x0000, 0x017E, 0x0000, 0x0000 };
static const u16 lower_data5[]   = { 0x0000, 0x0253, 0x0183, 0x0000, 0x0185, 0x0000, 0x0254, 0x0188, 0x0000, 0x0256, 0x0257, 0x018C, 0x0000, 0x0000, 0x01DD, 0x0259,
                                     0x025B, 0x0192, 0x0000, 0x0260, 0x0263, 0x0000, 0x0269, 0x0268, 0x0199, 0x0000, 0x0000, 0x0000, 0x026F, 0x0272, 0x0000, 0x0275,
                                     0x01A1, 0x0000, 0x01A3, 0x0000, 0x01A5, 0x0000, 0x0280, 0x01A8, 0x0000, 0x0283, 0x0000, 0x0000, 0x01AD, 0x0000, 0x0288, 0x01B0,
                                     0x0000, 0x028A, 0x028B, 0x01B4, 0x0000, 0x01B6, 0x0000, 0x0292, 0x01B9, 0x0000, 0x0000, 0x0000, 0x01BD, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data6[]   = { 0x0000, 0x0000, 0x0000, 0x0000, 0x01C6, 0x01C6, 0x0000, 0x01C9, 0x01C9, 0x0000, 0x01CC, 0x01CC, 0x0000, 0x01CE, 0x0000, 0x01D0,
                                     0x0000, 0x01D2, 0x0000, 0x01D4, 0x0000, 0x01D6, 0x0000, 0x01D8, 0x0000, 0x01DA, 0x0000, 0x01DC, 0x0000, 0x0000, 0x01DF, 0x0000,
                                     0x01E1, 0x0000, 0x01E3, 0x0000, 0x01E5, 0x0000, 0x01E7, 0x0000, 0x01E9, 0x0000, 0x01EB, 0x0000, 0x01ED, 0x0000, 0x01EF, 0x0000,
                                     0x0000, 0x01F3, 0x01F3, 0x0000, 0x01F5, 0x0000, 0x0195, 0x01BF, 0x01F9, 0x0000, 0x01FB, 0x0000, 0x01FD, 0x0000, 0x01FF, 0x0000 };
static const u16 lower_data7[]   = { 0x0201, 0x0000, 0x0203, 0x0000, 0x0205, 0x0000, 0x0207, 0x0000, 0x0209, 0x0000, 0x020B, 0x0000, 0x020D, 0x0000, 0x020F, 0x0000,
                                     0x0211, 0x0000, 0x0213, 0x0000, 0x0215, 0x0000, 0x0217, 0x0000, 0x0219, 0x0000, 0x021B, 0x0000, 0x021D, 0x0000, 0x021F, 0x0000,
                                     0x019E, 0x0000, 0x0223, 0x0000, 0x0225, 0x0000, 0x0227, 0x0000, 0x0229, 0x0000, 0x022B, 0x0000, 0x022D, 0x0000, 0x022F, 0x0000,
                                     0x0231, 0x0000, 0x0233, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2C65, 0x023C, 0x0000, 0x019A, 0x2C66, 0x0000 };
static const u16 lower_data8[]   = { 0x0000, 0x0242, 0x0000, 0x0180, 0x0289, 0x028C, 0x0247, 0x0000, 0x0249, 0x0000, 0x024B, 0x0000, 0x024D, 0x0000, 0x024F, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data9[]   = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0371, 0x0000, 0x0373, 0x0000, 0x0000, 0x0000, 0x0377, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data10[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x03AC, 0x0000, 0x03AD, 0x03AE, 0x03AF, 0x0000, 0x03CC, 0x0000, 0x03CD, 0x03CE,
                                     0x0000, 0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7, 0x03B8, 0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF,
                                     0x03C0, 0x03C1, 0x0000, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7, 0x03C8, 0x03C9, 0x03CA, 0x03CB, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data11[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x03D7,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x03D9, 0x0000, 0x03DB, 0x0000, 0x03DD, 0x0000, 0x03DF, 0x0000,
                                     0x03E1, 0x0000, 0x03E3, 0x0000, 0x03E5, 0x0000, 0x03E7, 0x0000, 0x03E9, 0x0000, 0x03EB, 0x0000, 0x03ED, 0x0000, 0x03EF, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x03B8, 0x0000, 0x0000, 0x03F8, 0x0000, 0x03F2, 0x03FB, 0x0000, 0x0000, 0x037B, 0x037C, 0x037D };
static const u16 lower_data12[]  = { 0x0450, 0x0451, 0x0452, 0x0453, 0x0454, 0x0455, 0x0456, 0x0457, 0x0458, 0x0459, 0x045A, 0x045B, 0x045C, 0x045D, 0x045E, 0x045F,
                                     0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
                                     0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data13[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0461, 0x0000, 0x0463, 0x0000, 0x0465, 0x0000, 0x0467, 0x0000, 0x0469, 0x0000, 0x046B, 0x0000, 0x046D, 0x0000, 0x046F, 0x0000,
                                     0x0471, 0x0000, 0x0473, 0x0000, 0x0475, 0x0000, 0x0477, 0x0000, 0x0479, 0x0000, 0x047B, 0x0000, 0x047D, 0x0000, 0x047F, 0x0000 };
static const u16 lower_data14[]  = { 0x0481, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x048B, 0x0000, 0x048D, 0x0000, 0x048F, 0x0000,
                                     0x0491, 0x0000, 0x0493, 0x0000, 0x0495, 0x0000, 0x0497, 0x0000, 0x0499, 0x0000, 0x049B, 0x0000, 0x049D, 0x0000, 0x049F, 0x0000,
                                     0x04A1, 0x0000, 0x04A3, 0x0000, 0x04A5, 0x0000, 0x04A7, 0x0000, 0x04A9, 0x0000, 0x04AB, 0x0000, 0x04AD, 0x0000, 0x04AF, 0x0000,
                                     0x04B1, 0x0000, 0x04B3, 0x0000, 0x04B5, 0x0000, 0x04B7, 0x0000, 0x04B9, 0x0000, 0x04BB, 0x0000, 0x04BD, 0x0000, 0x04BF, 0x0000 };
static const u16 lower_data15[]  = { 0x04CF, 0x04C2, 0x0000, 0x04C4, 0x0000, 0x04C6, 0x0000, 0x04C8, 0x0000, 0x04CA, 0x0000, 0x04CC, 0x0000, 0x04CE, 0x0000, 0x0000,
                                     0x04D1, 0x0000, 0x04D3, 0x0000, 0x04D5, 0x0000, 0x04D7, 0x0000, 0x04D9, 0x0000, 0x04DB, 0x0000, 0x04DD, 0x0000, 0x04DF, 0x0000,
                                     0x04E1, 0x0000, 0x04E3, 0x0000, 0x04E5, 0x0000, 0x04E7, 0x0000, 0x04E9, 0x0000, 0x04EB, 0x0000, 0x04ED, 0x0000, 0x04EF, 0x0000,
                                     0x04F1, 0x0000, 0x04F3, 0x0000, 0x04F5, 0x0000, 0x04F7, 0x0000, 0x04F9, 0x0000, 0x04FB, 0x0000, 0x04FD, 0x0000, 0x04FF, 0x0000 };
static const u16 lower_data16[]  = { 0x0501, 0x0000, 0x0503, 0x0000, 0x0505, 0x0000, 0x0507, 0x0000, 0x0509, 0x0000, 0x050B, 0x0000, 0x050D, 0x0000, 0x050F, 0x0000,
                                     0x0511, 0x0000, 0x0513, 0x0000, 0x0515, 0x0000, 0x0517, 0x0000, 0x0519, 0x0000, 0x051B, 0x0000, 0x051D, 0x0000, 0x051F, 0x0000,
                                     0x0521, 0x0000, 0x0523, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0561, 0x0562, 0x0563, 0x0564, 0x0565, 0x0566, 0x0567, 0x0568, 0x0569, 0x056A, 0x056B, 0x056C, 0x056D, 0x056E, 0x056F };
static const u16 lower_data17[]  = { 0x0570, 0x0571, 0x0572, 0x0573, 0x0574, 0x0575, 0x0576, 0x0577, 0x0578, 0x0579, 0x057A, 0x057B, 0x057C, 0x057D, 0x057E, 0x057F,
                                     0x0580, 0x0581, 0x0582, 0x0583, 0x0584, 0x0585, 0x0586, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data18[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x2D00, 0x2D01, 0x2D02, 0x2D03, 0x2D04, 0x2D05, 0x2D06, 0x2D07, 0x2D08, 0x2D09, 0x2D0A, 0x2D0B, 0x2D0C, 0x2D0D, 0x2D0E, 0x2D0F,
                                     0x2D10, 0x2D11, 0x2D12, 0x2D13, 0x2D14, 0x2D15, 0x2D16, 0x2D17, 0x2D18, 0x2D19, 0x2D1A, 0x2D1B, 0x2D1C, 0x2D1D, 0x2D1E, 0x2D1F };
static const u16 lower_data19[]  = { 0x2D20, 0x2D21, 0x2D22, 0x2D23, 0x2D24, 0x2D25, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data20[]  = { 0x1E01, 0x0000, 0x1E03, 0x0000, 0x1E05, 0x0000, 0x1E07, 0x0000, 0x1E09, 0x0000, 0x1E0B, 0x0000, 0x1E0D, 0x0000, 0x1E0F, 0x0000,
                                     0x1E11, 0x0000, 0x1E13, 0x0000, 0x1E15, 0x0000, 0x1E17, 0x0000, 0x1E19, 0x0000, 0x1E1B, 0x0000, 0x1E1D, 0x0000, 0x1E1F, 0x0000,
                                     0x1E21, 0x0000, 0x1E23, 0x0000, 0x1E25, 0x0000, 0x1E27, 0x0000, 0x1E29, 0x0000, 0x1E2B, 0x0000, 0x1E2D, 0x0000, 0x1E2F, 0x0000,
                                     0x1E31, 0x0000, 0x1E33, 0x0000, 0x1E35, 0x0000, 0x1E37, 0x0000, 0x1E39, 0x0000, 0x1E3B, 0x0000, 0x1E3D, 0x0000, 0x1E3F, 0x0000 };
static const u16 lower_data21[]  = { 0x1E41, 0x0000, 0x1E43, 0x0000, 0x1E45, 0x0000, 0x1E47, 0x0000, 0x1E49, 0x0000, 0x1E4B, 0x0000, 0x1E4D, 0x0000, 0x1E4F, 0x0000,
                                     0x1E51, 0x0000, 0x1E53, 0x0000, 0x1E55, 0x0000, 0x1E57, 0x0000, 0x1E59, 0x0000, 0x1E5B, 0x0000, 0x1E5D, 0x0000, 0x1E5F, 0x0000,
                                     0x1E61, 0x0000, 0x1E63, 0x0000, 0x1E65, 0x0000, 0x1E67, 0x0000, 0x1E69, 0x0000, 0x1E6B, 0x0000, 0x1E6D, 0x0000, 0x1E6F, 0x0000,
                                     0x1E71, 0x0000, 0x1E73, 0x0000, 0x1E75, 0x0000, 0x1E77, 0x0000, 0x1E79, 0x0000, 0x1E7B, 0x0000, 0x1E7D, 0x0000, 0x1E7F, 0x0000 };
static const u16 lower_data22[]  = { 0x1E81, 0x0000, 0x1E83, 0x0000, 0x1E85, 0x0000, 0x1E87, 0x0000, 0x1E89, 0x0000, 0x1E8B, 0x0000, 0x1E8D, 0x0000, 0x1E8F, 0x0000,
                                     0x1E91, 0x0000, 0x1E93, 0x0000, 0x1E95, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00DF, 0x0000,
                                     0x1EA1, 0x0000, 0x1EA3, 0x0000, 0x1EA5, 0x0000, 0x1EA7, 0x0000, 0x1EA9, 0x0000, 0x1EAB, 0x0000, 0x1EAD, 0x0000, 0x1EAF, 0x0000,
                                     0x1EB1, 0x0000, 0x1EB3, 0x0000, 0x1EB5, 0x0000, 0x1EB7, 0x0000, 0x1EB9, 0x0000, 0x1EBB, 0x0000, 0x1EBD, 0x0000, 0x1EBF, 0x0000 };
static const u16 lower_data23[]  = { 0x1EC1, 0x0000, 0x1EC3, 0x0000, 0x1EC5, 0x0000, 0x1EC7, 0x0000, 0x1EC9, 0x0000, 0x1ECB, 0x0000, 0x1ECD, 0x0000, 0x1ECF, 0x0000,
                                     0x1ED1, 0x0000, 0x1ED3, 0x0000, 0x1ED5, 0x0000, 0x1ED7, 0x0000, 0x1ED9, 0x0000, 0x1EDB, 0x0000, 0x1EDD, 0x0000, 0x1EDF, 0x0000,
                                     0x1EE1, 0x0000, 0x1EE3, 0x0000, 0x1EE5, 0x0000, 0x1EE7, 0x0000, 0x1EE9, 0x0000, 0x1EEB, 0x0000, 0x1EED, 0x0000, 0x1EEF, 0x0000,
                                     0x1EF1, 0x0000, 0x1EF3, 0x0000, 0x1EF5, 0x0000, 0x1EF7, 0x0000, 0x1EF9, 0x0000, 0x1EFB, 0x0000, 0x1EFD, 0x0000, 0x1EFF, 0x0000 };
static const u16 lower_data24[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F00, 0x1F01, 0x1F02, 0x1F03, 0x1F04, 0x1F05, 0x1F06, 0x1F07,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F10, 0x1F11, 0x1F12, 0x1F13, 0x1F14, 0x1F15, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F20, 0x1F21, 0x1F22, 0x1F23, 0x1F24, 0x1F25, 0x1F26, 0x1F27,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F30, 0x1F31, 0x1F32, 0x1F33, 0x1F34, 0x1F35, 0x1F36, 0x1F37 };
static const u16 lower_data25[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F40, 0x1F41, 0x1F42, 0x1F43, 0x1F44, 0x1F45, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F51, 0x0000, 0x1F53, 0x0000, 0x1F55, 0x0000, 0x1F57,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F60, 0x1F61, 0x1F62, 0x1F63, 0x1F64, 0x1F65, 0x1F66, 0x1F67,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data26[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F80, 0x1F81, 0x1F82, 0x1F83, 0x1F84, 0x1F85, 0x1F86, 0x1F87,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F90, 0x1F91, 0x1F92, 0x1F93, 0x1F94, 0x1F95, 0x1F96, 0x1F97,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1FA0, 0x1FA1, 0x1FA2, 0x1FA3, 0x1FA4, 0x1FA5, 0x1FA6, 0x1FA7,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1FB0, 0x1FB1, 0x1F70, 0x1F71, 0x1FB3, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data27[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F72, 0x1F73, 0x1F74, 0x1F75, 0x1FC3, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1FD0, 0x1FD1, 0x1F76, 0x1F77, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1FE0, 0x1FE1, 0x1F7A, 0x1F7B, 0x1FE5, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F78, 0x1F79, 0x1F7C, 0x1F7D, 0x1FF3, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data28[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x03C9, 0x0000, 0x0000, 0x0000, 0x006B, 0x00E5, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x214E, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data29[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x2170, 0x2171, 0x2172, 0x2173, 0x2174, 0x2175, 0x2176, 0x2177, 0x2178, 0x2179, 0x217A, 0x217B, 0x217C, 0x217D, 0x217E, 0x217F,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data30[]  = { 0x0000, 0x0000, 0x0000, 0x2184, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data31[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x24D0, 0x24D1, 0x24D2, 0x24D3, 0x24D4, 0x24D5, 0x24D6, 0x24D7, 0x24D8, 0x24D9 };
static const u16 lower_data32[]  = { 0x24DA, 0x24DB, 0x24DC, 0x24DD, 0x24DE, 0x24DF, 0x24E0, 0x24E1, 0x24E2, 0x24E3, 0x24E4, 0x24E5, 0x24E6, 0x24E7, 0x24E8, 0x24E9,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data33[]  = { 0x2C30, 0x2C31, 0x2C32, 0x2C33, 0x2C34, 0x2C35, 0x2C36, 0x2C37, 0x2C38, 0x2C39, 0x2C3A, 0x2C3B, 0x2C3C, 0x2C3D, 0x2C3E, 0x2C3F,
                                     0x2C40, 0x2C41, 0x2C42, 0x2C43, 0x2C44, 0x2C45, 0x2C46, 0x2C47, 0x2C48, 0x2C49, 0x2C4A, 0x2C4B, 0x2C4C, 0x2C4D, 0x2C4E, 0x2C4F,
                                     0x2C50, 0x2C51, 0x2C52, 0x2C53, 0x2C54, 0x2C55, 0x2C56, 0x2C57, 0x2C58, 0x2C59, 0x2C5A, 0x2C5B, 0x2C5C, 0x2C5D, 0x2C5E, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data34[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x2C61, 0x0000, 0x026B, 0x1D7D, 0x027D, 0x0000, 0x0000, 0x2C68, 0x0000, 0x2C6A, 0x0000, 0x2C6C, 0x0000, 0x0251, 0x0271, 0x0250,
                                     0x0000, 0x0000, 0x2C73, 0x0000, 0x0000, 0x2C76, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data35[]  = { 0x2C81, 0x0000, 0x2C83, 0x0000, 0x2C85, 0x0000, 0x2C87, 0x0000, 0x2C89, 0x0000, 0x2C8B, 0x0000, 0x2C8D, 0x0000, 0x2C8F, 0x0000,
                                     0x2C91, 0x0000, 0x2C93, 0x0000, 0x2C95, 0x0000, 0x2C97, 0x0000, 0x2C99, 0x0000, 0x2C9B, 0x0000, 0x2C9D, 0x0000, 0x2C9F, 0x0000,
                                     0x2CA1, 0x0000, 0x2CA3, 0x0000, 0x2CA5, 0x0000, 0x2CA7, 0x0000, 0x2CA9, 0x0000, 0x2CAB, 0x0000, 0x2CAD, 0x0000, 0x2CAF, 0x0000,
                                     0x2CB1, 0x0000, 0x2CB3, 0x0000, 0x2CB5, 0x0000, 0x2CB7, 0x0000, 0x2CB9, 0x0000, 0x2CBB, 0x0000, 0x2CBD, 0x0000, 0x2CBF, 0x0000 };
static const u16 lower_data36[]  = { 0x2CC1, 0x0000, 0x2CC3, 0x0000, 0x2CC5, 0x0000, 0x2CC7, 0x0000, 0x2CC9, 0x0000, 0x2CCB, 0x0000, 0x2CCD, 0x0000, 0x2CCF, 0x0000,
                                     0x2CD1, 0x0000, 0x2CD3, 0x0000, 0x2CD5, 0x0000, 0x2CD7, 0x0000, 0x2CD9, 0x0000, 0x2CDB, 0x0000, 0x2CDD, 0x0000, 0x2CDF, 0x0000,
                                     0x2CE1, 0x0000, 0x2CE3, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data37[]  = { 0xA641, 0x0000, 0xA643, 0x0000, 0xA645, 0x0000, 0xA647, 0x0000, 0xA649, 0x0000, 0xA64B, 0x0000, 0xA64D, 0x0000, 0xA64F, 0x0000,
                                     0xA651, 0x0000, 0xA653, 0x0000, 0xA655, 0x0000, 0xA657, 0x0000, 0xA659, 0x0000, 0xA65B, 0x0000, 0xA65D, 0x0000, 0xA65F, 0x0000,
                                     0x0000, 0x0000, 0xA663, 0x0000, 0xA665, 0x0000, 0xA667, 0x0000, 0xA669, 0x0000, 0xA66B, 0x0000, 0xA66D, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data38[]  = { 0xA681, 0x0000, 0xA683, 0x0000, 0xA685, 0x0000, 0xA687, 0x0000, 0xA689, 0x0000, 0xA68B, 0x0000, 0xA68D, 0x0000, 0xA68F, 0x0000,
                                     0xA691, 0x0000, 0xA693, 0x0000, 0xA695, 0x0000, 0xA697, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data39[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0xA723, 0x0000, 0xA725, 0x0000, 0xA727, 0x0000, 0xA729, 0x0000, 0xA72B, 0x0000, 0xA72D, 0x0000, 0xA72F, 0x0000,
                                     0x0000, 0x0000, 0xA733, 0x0000, 0xA735, 0x0000, 0xA737, 0x0000, 0xA739, 0x0000, 0xA73B, 0x0000, 0xA73D, 0x0000, 0xA73F, 0x0000 };
static const u16 lower_data40[]  = { 0xA741, 0x0000, 0xA743, 0x0000, 0xA745, 0x0000, 0xA747, 0x0000, 0xA749, 0x0000, 0xA74B, 0x0000, 0xA74D, 0x0000, 0xA74F, 0x0000,
                                     0xA751, 0x0000, 0xA753, 0x0000, 0xA755, 0x0000, 0xA757, 0x0000, 0xA759, 0x0000, 0xA75B, 0x0000, 0xA75D, 0x0000, 0xA75F, 0x0000,
                                     0xA761, 0x0000, 0xA763, 0x0000, 0xA765, 0x0000, 0xA767, 0x0000, 0xA769, 0x0000, 0xA76B, 0x0000, 0xA76D, 0x0000, 0xA76F, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xA77A, 0x0000, 0xA77C, 0x0000, 0x1D79, 0xA77F, 0x0000 };
static const u16 lower_data41[]  = { 0xA781, 0x0000, 0xA783, 0x0000, 0xA785, 0x0000, 0xA787, 0x0000, 0x0000, 0x0000, 0x0000, 0xA78C, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 lower_data42[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0xFF41, 0xFF42, 0xFF43, 0xFF44, 0xFF45, 0xFF46, 0xFF47, 0xFF48, 0xFF49, 0xFF4A, 0xFF4B, 0xFF4C, 0xFF4D, 0xFF4E, 0xFF4F,
                                     0xFF50, 0xFF51, 0xFF52, 0xFF53, 0xFF54, 0xFF55, 0xFF56, 0xFF57, 0xFF58, 0xFF59, 0xFF5A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };

static const u16 *lower_data_table[LOWER_BLOCK_COUNT] = {
lower_data0,lower_data1,lower_data2,lower_data3,lower_data4,lower_data5,lower_data6,lower_data7,lower_data8,lower_data9,
lower_data10,lower_data11,lower_data12,lower_data13,lower_data14,lower_data15,lower_data16,lower_data17,lower_data18,lower_data19,
lower_data20,lower_data21,lower_data22,lower_data23,lower_data24,lower_data25,lower_data26,lower_data27,lower_data28,lower_data29,
lower_data30,lower_data31,lower_data32,lower_data33,lower_data34,lower_data35,lower_data36,lower_data37,lower_data38,lower_data39,
lower_data40,lower_data41,lower_data42
};
/* Generated by JcD. Do not modify. End lower_tables */

SQLITE_PRIVATE u32 unifuzz_lower(
    u32 c
){
    u32 p;
    if (c >= 0x10000) return c;
    p = (lower_data_table[lower_indexes[(c) >> LOWER_BLOCK_SHIFT]][(c) & LOWER_BLOCK_MASK]);
    return (p == 0) ? c : p;
}


/* Generated by builder. Do not modify. Start upper_defines */
#define UPPER_BLOCK_SHIFT 6
#define UPPER_BLOCK_MASK ((1 << UPPER_BLOCK_SHIFT) - 1)
#define UPPER_BLOCK_SIZE (1 << UPPER_BLOCK_SHIFT)
#define UPPER_BLOCK_COUNT 44
#define UPPER_INDEXES_SIZE (0x10000 >> UPPER_BLOCK_SHIFT)
/* Generated by builder. Do not modify. End upper_defines */

/*  JcD upper  */
static const u16 upper_indexes[UPPER_INDEXES_SIZE] = {
   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,   0,   0,  11,  12,
  13,  14,  15,  16,  17,  18,  19,  20,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  21,   0,   0,
  22,  23,  24,  25,  26,  27,  28,  29,   0,   0,   0,   0,   0,  30,  31,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  32,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  33,  34,  35,  36,
  37,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,  38,  39,   0,  40,  41,  42,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,  43,   0
};

static const u16 upper_data0[]   = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data1[]   = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
                                     0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data2[]   = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x039C, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data3[]   = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7, 0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
                                     0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x0000, 0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x0178 };
static const u16 upper_data4[]   = { 0x0000, 0x0100, 0x0000, 0x0102, 0x0000, 0x0104, 0x0000, 0x0106, 0x0000, 0x0108, 0x0000, 0x010A, 0x0000, 0x010C, 0x0000, 0x010E,
                                     0x0000, 0x0110, 0x0000, 0x0112, 0x0000, 0x0114, 0x0000, 0x0116, 0x0000, 0x0118, 0x0000, 0x011A, 0x0000, 0x011C, 0x0000, 0x011E,
                                     0x0000, 0x0120, 0x0000, 0x0122, 0x0000, 0x0124, 0x0000, 0x0126, 0x0000, 0x0128, 0x0000, 0x012A, 0x0000, 0x012C, 0x0000, 0x012E,
                                     0x0000, 0x0049, 0x0000, 0x0132, 0x0000, 0x0134, 0x0000, 0x0136, 0x0000, 0x0000, 0x0139, 0x0000, 0x013B, 0x0000, 0x013D, 0x0000 };
static const u16 upper_data5[]   = { 0x013F, 0x0000, 0x0141, 0x0000, 0x0143, 0x0000, 0x0145, 0x0000, 0x0147, 0x0000, 0x0000, 0x014A, 0x0000, 0x014C, 0x0000, 0x014E,
                                     0x0000, 0x0150, 0x0000, 0x0152, 0x0000, 0x0154, 0x0000, 0x0156, 0x0000, 0x0158, 0x0000, 0x015A, 0x0000, 0x015C, 0x0000, 0x015E,
                                     0x0000, 0x0160, 0x0000, 0x0162, 0x0000, 0x0164, 0x0000, 0x0166, 0x0000, 0x0168, 0x0000, 0x016A, 0x0000, 0x016C, 0x0000, 0x016E,
                                     0x0000, 0x0170, 0x0000, 0x0172, 0x0000, 0x0174, 0x0000, 0x0176, 0x0000, 0x0000, 0x0179, 0x0000, 0x017B, 0x0000, 0x017D, 0x0053 };
static const u16 upper_data6[]   = { 0x0243, 0x0000, 0x0000, 0x0182, 0x0000, 0x0184, 0x0000, 0x0000, 0x0187, 0x0000, 0x0000, 0x0000, 0x018B, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0191, 0x0000, 0x0000, 0x01F6, 0x0000, 0x0000, 0x0000, 0x0198, 0x023D, 0x0000, 0x0000, 0x0000, 0x0220, 0x0000,
                                     0x0000, 0x01A0, 0x0000, 0x01A2, 0x0000, 0x01A4, 0x0000, 0x0000, 0x01A7, 0x0000, 0x0000, 0x0000, 0x0000, 0x01AC, 0x0000, 0x0000,
                                     0x01AF, 0x0000, 0x0000, 0x0000, 0x01B3, 0x0000, 0x01B5, 0x0000, 0x0000, 0x01B8, 0x0000, 0x0000, 0x0000, 0x01BC, 0x0000, 0x01F7 };
static const u16 upper_data7[]   = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x01C4, 0x01C4, 0x0000, 0x01C7, 0x01C7, 0x0000, 0x01CA, 0x01CA, 0x0000, 0x01CD, 0x0000,
                                     0x01CF, 0x0000, 0x01D1, 0x0000, 0x01D3, 0x0000, 0x01D5, 0x0000, 0x01D7, 0x0000, 0x01D9, 0x0000, 0x01DB, 0x018E, 0x0000, 0x01DE,
                                     0x0000, 0x01E0, 0x0000, 0x01E2, 0x0000, 0x01E4, 0x0000, 0x01E6, 0x0000, 0x01E8, 0x0000, 0x01EA, 0x0000, 0x01EC, 0x0000, 0x01EE,
                                     0x0000, 0x0000, 0x01F1, 0x01F1, 0x0000, 0x01F4, 0x0000, 0x0000, 0x0000, 0x01F8, 0x0000, 0x01FA, 0x0000, 0x01FC, 0x0000, 0x01FE };
static const u16 upper_data8[]   = { 0x0000, 0x0200, 0x0000, 0x0202, 0x0000, 0x0204, 0x0000, 0x0206, 0x0000, 0x0208, 0x0000, 0x020A, 0x0000, 0x020C, 0x0000, 0x020E,
                                     0x0000, 0x0210, 0x0000, 0x0212, 0x0000, 0x0214, 0x0000, 0x0216, 0x0000, 0x0218, 0x0000, 0x021A, 0x0000, 0x021C, 0x0000, 0x021E,
                                     0x0000, 0x0000, 0x0000, 0x0222, 0x0000, 0x0224, 0x0000, 0x0226, 0x0000, 0x0228, 0x0000, 0x022A, 0x0000, 0x022C, 0x0000, 0x022E,
                                     0x0000, 0x0230, 0x0000, 0x0232, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x023B, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data9[]   = { 0x0000, 0x0000, 0x0241, 0x0000, 0x0000, 0x0000, 0x0000, 0x0246, 0x0000, 0x0248, 0x0000, 0x024A, 0x0000, 0x024C, 0x0000, 0x024E,
                                     0x2C6F, 0x2C6D, 0x0000, 0x0181, 0x0186, 0x0000, 0x0189, 0x018A, 0x0000, 0x018F, 0x0000, 0x0190, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0193, 0x0000, 0x0000, 0x0194, 0x0000, 0x0000, 0x0000, 0x0000, 0x0197, 0x0196, 0x0000, 0x2C62, 0x0000, 0x0000, 0x0000, 0x019C,
                                     0x0000, 0x2C6E, 0x019D, 0x0000, 0x0000, 0x019F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2C64, 0x0000, 0x0000 };
static const u16 upper_data10[]  = { 0x01A6, 0x0000, 0x0000, 0x01A9, 0x0000, 0x0000, 0x0000, 0x0000, 0x01AE, 0x0244, 0x01B1, 0x01B2, 0x0245, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x01B7, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data11[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0399, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0370, 0x0000, 0x0372, 0x0000, 0x0000, 0x0000, 0x0376, 0x0000, 0x0000, 0x0000, 0x03FD, 0x03FE, 0x03FF, 0x0000, 0x0000 };
static const u16 upper_data12[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0386, 0x0388, 0x0389, 0x038A,
                                     0x0000, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397, 0x0398, 0x0399, 0x039A, 0x039B, 0x039C, 0x039D, 0x039E, 0x039F };
static const u16 upper_data13[]  = { 0x03A0, 0x03A1, 0x03A3, 0x03A3, 0x03A4, 0x03A5, 0x03A6, 0x03A7, 0x03A8, 0x03A9, 0x03AA, 0x03AB, 0x038C, 0x038E, 0x038F, 0x0000,
                                     0x0392, 0x0398, 0x0000, 0x0000, 0x0000, 0x03A6, 0x03A0, 0x03CF, 0x0000, 0x03D8, 0x0000, 0x03DA, 0x0000, 0x03DC, 0x0000, 0x03DE,
                                     0x0000, 0x03E0, 0x0000, 0x03E2, 0x0000, 0x03E4, 0x0000, 0x03E6, 0x0000, 0x03E8, 0x0000, 0x03EA, 0x0000, 0x03EC, 0x0000, 0x03EE,
                                     0x039A, 0x03A1, 0x03F9, 0x0000, 0x0000, 0x0395, 0x0000, 0x0000, 0x03F7, 0x0000, 0x0000, 0x03FA, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data14[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F };
static const u16 upper_data15[]  = { 0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
                                     0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407, 0x0408, 0x0409, 0x040A, 0x040B, 0x040C, 0x040D, 0x040E, 0x040F,
                                     0x0000, 0x0460, 0x0000, 0x0462, 0x0000, 0x0464, 0x0000, 0x0466, 0x0000, 0x0468, 0x0000, 0x046A, 0x0000, 0x046C, 0x0000, 0x046E,
                                     0x0000, 0x0470, 0x0000, 0x0472, 0x0000, 0x0474, 0x0000, 0x0476, 0x0000, 0x0478, 0x0000, 0x047A, 0x0000, 0x047C, 0x0000, 0x047E };
static const u16 upper_data16[]  = { 0x0000, 0x0480, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x048A, 0x0000, 0x048C, 0x0000, 0x048E,
                                     0x0000, 0x0490, 0x0000, 0x0492, 0x0000, 0x0494, 0x0000, 0x0496, 0x0000, 0x0498, 0x0000, 0x049A, 0x0000, 0x049C, 0x0000, 0x049E,
                                     0x0000, 0x04A0, 0x0000, 0x04A2, 0x0000, 0x04A4, 0x0000, 0x04A6, 0x0000, 0x04A8, 0x0000, 0x04AA, 0x0000, 0x04AC, 0x0000, 0x04AE,
                                     0x0000, 0x04B0, 0x0000, 0x04B2, 0x0000, 0x04B4, 0x0000, 0x04B6, 0x0000, 0x04B8, 0x0000, 0x04BA, 0x0000, 0x04BC, 0x0000, 0x04BE };
static const u16 upper_data17[]  = { 0x0000, 0x0000, 0x04C1, 0x0000, 0x04C3, 0x0000, 0x04C5, 0x0000, 0x04C7, 0x0000, 0x04C9, 0x0000, 0x04CB, 0x0000, 0x04CD, 0x04C0,
                                     0x0000, 0x04D0, 0x0000, 0x04D2, 0x0000, 0x04D4, 0x0000, 0x04D6, 0x0000, 0x04D8, 0x0000, 0x04DA, 0x0000, 0x04DC, 0x0000, 0x04DE,
                                     0x0000, 0x04E0, 0x0000, 0x04E2, 0x0000, 0x04E4, 0x0000, 0x04E6, 0x0000, 0x04E8, 0x0000, 0x04EA, 0x0000, 0x04EC, 0x0000, 0x04EE,
                                     0x0000, 0x04F0, 0x0000, 0x04F2, 0x0000, 0x04F4, 0x0000, 0x04F6, 0x0000, 0x04F8, 0x0000, 0x04FA, 0x0000, 0x04FC, 0x0000, 0x04FE };
static const u16 upper_data18[]  = { 0x0000, 0x0500, 0x0000, 0x0502, 0x0000, 0x0504, 0x0000, 0x0506, 0x0000, 0x0508, 0x0000, 0x050A, 0x0000, 0x050C, 0x0000, 0x050E,
                                     0x0000, 0x0510, 0x0000, 0x0512, 0x0000, 0x0514, 0x0000, 0x0516, 0x0000, 0x0518, 0x0000, 0x051A, 0x0000, 0x051C, 0x0000, 0x051E,
                                     0x0000, 0x0520, 0x0000, 0x0522, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data19[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0531, 0x0532, 0x0533, 0x0534, 0x0535, 0x0536, 0x0537, 0x0538, 0x0539, 0x053A, 0x053B, 0x053C, 0x053D, 0x053E, 0x053F,
                                     0x0540, 0x0541, 0x0542, 0x0543, 0x0544, 0x0545, 0x0546, 0x0547, 0x0548, 0x0549, 0x054A, 0x054B, 0x054C, 0x054D, 0x054E, 0x054F };
static const u16 upper_data20[]  = { 0x0550, 0x0551, 0x0552, 0x0553, 0x0554, 0x0555, 0x0556, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data21[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xA77D, 0x0000, 0x0000, 0x0000, 0x2C63, 0x0000, 0x0000 };
static const u16 upper_data22[]  = { 0x0000, 0x1E00, 0x0000, 0x1E02, 0x0000, 0x1E04, 0x0000, 0x1E06, 0x0000, 0x1E08, 0x0000, 0x1E0A, 0x0000, 0x1E0C, 0x0000, 0x1E0E,
                                     0x0000, 0x1E10, 0x0000, 0x1E12, 0x0000, 0x1E14, 0x0000, 0x1E16, 0x0000, 0x1E18, 0x0000, 0x1E1A, 0x0000, 0x1E1C, 0x0000, 0x1E1E,
                                     0x0000, 0x1E20, 0x0000, 0x1E22, 0x0000, 0x1E24, 0x0000, 0x1E26, 0x0000, 0x1E28, 0x0000, 0x1E2A, 0x0000, 0x1E2C, 0x0000, 0x1E2E,
                                     0x0000, 0x1E30, 0x0000, 0x1E32, 0x0000, 0x1E34, 0x0000, 0x1E36, 0x0000, 0x1E38, 0x0000, 0x1E3A, 0x0000, 0x1E3C, 0x0000, 0x1E3E };
static const u16 upper_data23[]  = { 0x0000, 0x1E40, 0x0000, 0x1E42, 0x0000, 0x1E44, 0x0000, 0x1E46, 0x0000, 0x1E48, 0x0000, 0x1E4A, 0x0000, 0x1E4C, 0x0000, 0x1E4E,
                                     0x0000, 0x1E50, 0x0000, 0x1E52, 0x0000, 0x1E54, 0x0000, 0x1E56, 0x0000, 0x1E58, 0x0000, 0x1E5A, 0x0000, 0x1E5C, 0x0000, 0x1E5E,
                                     0x0000, 0x1E60, 0x0000, 0x1E62, 0x0000, 0x1E64, 0x0000, 0x1E66, 0x0000, 0x1E68, 0x0000, 0x1E6A, 0x0000, 0x1E6C, 0x0000, 0x1E6E,
                                     0x0000, 0x1E70, 0x0000, 0x1E72, 0x0000, 0x1E74, 0x0000, 0x1E76, 0x0000, 0x1E78, 0x0000, 0x1E7A, 0x0000, 0x1E7C, 0x0000, 0x1E7E };
static const u16 upper_data24[]  = { 0x0000, 0x1E80, 0x0000, 0x1E82, 0x0000, 0x1E84, 0x0000, 0x1E86, 0x0000, 0x1E88, 0x0000, 0x1E8A, 0x0000, 0x1E8C, 0x0000, 0x1E8E,
                                     0x0000, 0x1E90, 0x0000, 0x1E92, 0x0000, 0x1E94, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1E60, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x1EA0, 0x0000, 0x1EA2, 0x0000, 0x1EA4, 0x0000, 0x1EA6, 0x0000, 0x1EA8, 0x0000, 0x1EAA, 0x0000, 0x1EAC, 0x0000, 0x1EAE,
                                     0x0000, 0x1EB0, 0x0000, 0x1EB2, 0x0000, 0x1EB4, 0x0000, 0x1EB6, 0x0000, 0x1EB8, 0x0000, 0x1EBA, 0x0000, 0x1EBC, 0x0000, 0x1EBE };
static const u16 upper_data25[]  = { 0x0000, 0x1EC0, 0x0000, 0x1EC2, 0x0000, 0x1EC4, 0x0000, 0x1EC6, 0x0000, 0x1EC8, 0x0000, 0x1ECA, 0x0000, 0x1ECC, 0x0000, 0x1ECE,
                                     0x0000, 0x1ED0, 0x0000, 0x1ED2, 0x0000, 0x1ED4, 0x0000, 0x1ED6, 0x0000, 0x1ED8, 0x0000, 0x1EDA, 0x0000, 0x1EDC, 0x0000, 0x1EDE,
                                     0x0000, 0x1EE0, 0x0000, 0x1EE2, 0x0000, 0x1EE4, 0x0000, 0x1EE6, 0x0000, 0x1EE8, 0x0000, 0x1EEA, 0x0000, 0x1EEC, 0x0000, 0x1EEE,
                                     0x0000, 0x1EF0, 0x0000, 0x1EF2, 0x0000, 0x1EF4, 0x0000, 0x1EF6, 0x0000, 0x1EF8, 0x0000, 0x1EFA, 0x0000, 0x1EFC, 0x0000, 0x1EFE };
static const u16 upper_data26[]  = { 0x1F08, 0x1F09, 0x1F0A, 0x1F0B, 0x1F0C, 0x1F0D, 0x1F0E, 0x1F0F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1F18, 0x1F19, 0x1F1A, 0x1F1B, 0x1F1C, 0x1F1D, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1F28, 0x1F29, 0x1F2A, 0x1F2B, 0x1F2C, 0x1F2D, 0x1F2E, 0x1F2F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1F38, 0x1F39, 0x1F3A, 0x1F3B, 0x1F3C, 0x1F3D, 0x1F3E, 0x1F3F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data27[]  = { 0x1F48, 0x1F49, 0x1F4A, 0x1F4B, 0x1F4C, 0x1F4D, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x1F59, 0x0000, 0x1F5B, 0x0000, 0x1F5D, 0x0000, 0x1F5F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1F68, 0x1F69, 0x1F6A, 0x1F6B, 0x1F6C, 0x1F6D, 0x1F6E, 0x1F6F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1FBA, 0x1FBB, 0x1FC8, 0x1FC9, 0x1FCA, 0x1FCB, 0x1FDA, 0x1FDB, 0x1FF8, 0x1FF9, 0x1FEA, 0x1FEB, 0x1FFA, 0x1FFB, 0x0000, 0x0000 };
static const u16 upper_data28[]  = { 0x1F88, 0x1F89, 0x1F8A, 0x1F8B, 0x1F8C, 0x1F8D, 0x1F8E, 0x1F8F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1F98, 0x1F99, 0x1F9A, 0x1F9B, 0x1F9C, 0x1F9D, 0x1F9E, 0x1F9F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1FA8, 0x1FA9, 0x1FAA, 0x1FAB, 0x1FAC, 0x1FAD, 0x1FAE, 0x1FAF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1FB8, 0x1FB9, 0x0000, 0x1FBC, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0399, 0x0000 };
static const u16 upper_data29[]  = { 0x0000, 0x0000, 0x0000, 0x1FCC, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1FD8, 0x1FD9, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1FE8, 0x1FE9, 0x0000, 0x0000, 0x0000, 0x1FEC, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x1FFC, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data30[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2132, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x2160, 0x2161, 0x2162, 0x2163, 0x2164, 0x2165, 0x2166, 0x2167, 0x2168, 0x2169, 0x216A, 0x216B, 0x216C, 0x216D, 0x216E, 0x216F };
static const u16 upper_data31[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x2183, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data32[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x24B6, 0x24B7, 0x24B8, 0x24B9, 0x24BA, 0x24BB, 0x24BC, 0x24BD, 0x24BE, 0x24BF, 0x24C0, 0x24C1, 0x24C2, 0x24C3, 0x24C4, 0x24C5,
                                     0x24C6, 0x24C7, 0x24C8, 0x24C9, 0x24CA, 0x24CB, 0x24CC, 0x24CD, 0x24CE, 0x24CF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data33[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x2C00, 0x2C01, 0x2C02, 0x2C03, 0x2C04, 0x2C05, 0x2C06, 0x2C07, 0x2C08, 0x2C09, 0x2C0A, 0x2C0B, 0x2C0C, 0x2C0D, 0x2C0E, 0x2C0F };
static const u16 upper_data34[]  = { 0x2C10, 0x2C11, 0x2C12, 0x2C13, 0x2C14, 0x2C15, 0x2C16, 0x2C17, 0x2C18, 0x2C19, 0x2C1A, 0x2C1B, 0x2C1C, 0x2C1D, 0x2C1E, 0x2C1F,
                                     0x2C20, 0x2C21, 0x2C22, 0x2C23, 0x2C24, 0x2C25, 0x2C26, 0x2C27, 0x2C28, 0x2C29, 0x2C2A, 0x2C2B, 0x2C2C, 0x2C2D, 0x2C2E, 0x0000,
                                     0x0000, 0x2C60, 0x0000, 0x0000, 0x0000, 0x023A, 0x023E, 0x0000, 0x2C67, 0x0000, 0x2C69, 0x0000, 0x2C6B, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x2C72, 0x0000, 0x0000, 0x2C75, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data35[]  = { 0x0000, 0x2C80, 0x0000, 0x2C82, 0x0000, 0x2C84, 0x0000, 0x2C86, 0x0000, 0x2C88, 0x0000, 0x2C8A, 0x0000, 0x2C8C, 0x0000, 0x2C8E,
                                     0x0000, 0x2C90, 0x0000, 0x2C92, 0x0000, 0x2C94, 0x0000, 0x2C96, 0x0000, 0x2C98, 0x0000, 0x2C9A, 0x0000, 0x2C9C, 0x0000, 0x2C9E,
                                     0x0000, 0x2CA0, 0x0000, 0x2CA2, 0x0000, 0x2CA4, 0x0000, 0x2CA6, 0x0000, 0x2CA8, 0x0000, 0x2CAA, 0x0000, 0x2CAC, 0x0000, 0x2CAE,
                                     0x0000, 0x2CB0, 0x0000, 0x2CB2, 0x0000, 0x2CB4, 0x0000, 0x2CB6, 0x0000, 0x2CB8, 0x0000, 0x2CBA, 0x0000, 0x2CBC, 0x0000, 0x2CBE };
static const u16 upper_data36[]  = { 0x0000, 0x2CC0, 0x0000, 0x2CC2, 0x0000, 0x2CC4, 0x0000, 0x2CC6, 0x0000, 0x2CC8, 0x0000, 0x2CCA, 0x0000, 0x2CCC, 0x0000, 0x2CCE,
                                     0x0000, 0x2CD0, 0x0000, 0x2CD2, 0x0000, 0x2CD4, 0x0000, 0x2CD6, 0x0000, 0x2CD8, 0x0000, 0x2CDA, 0x0000, 0x2CDC, 0x0000, 0x2CDE,
                                     0x0000, 0x2CE0, 0x0000, 0x2CE2, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data37[]  = { 0x10A0, 0x10A1, 0x10A2, 0x10A3, 0x10A4, 0x10A5, 0x10A6, 0x10A7, 0x10A8, 0x10A9, 0x10AA, 0x10AB, 0x10AC, 0x10AD, 0x10AE, 0x10AF,
                                     0x10B0, 0x10B1, 0x10B2, 0x10B3, 0x10B4, 0x10B5, 0x10B6, 0x10B7, 0x10B8, 0x10B9, 0x10BA, 0x10BB, 0x10BC, 0x10BD, 0x10BE, 0x10BF,
                                     0x10C0, 0x10C1, 0x10C2, 0x10C3, 0x10C4, 0x10C5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data38[]  = { 0x0000, 0xA640, 0x0000, 0xA642, 0x0000, 0xA644, 0x0000, 0xA646, 0x0000, 0xA648, 0x0000, 0xA64A, 0x0000, 0xA64C, 0x0000, 0xA64E,
                                     0x0000, 0xA650, 0x0000, 0xA652, 0x0000, 0xA654, 0x0000, 0xA656, 0x0000, 0xA658, 0x0000, 0xA65A, 0x0000, 0xA65C, 0x0000, 0xA65E,
                                     0x0000, 0x0000, 0x0000, 0xA662, 0x0000, 0xA664, 0x0000, 0xA666, 0x0000, 0xA668, 0x0000, 0xA66A, 0x0000, 0xA66C, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data39[]  = { 0x0000, 0xA680, 0x0000, 0xA682, 0x0000, 0xA684, 0x0000, 0xA686, 0x0000, 0xA688, 0x0000, 0xA68A, 0x0000, 0xA68C, 0x0000, 0xA68E,
                                     0x0000, 0xA690, 0x0000, 0xA692, 0x0000, 0xA694, 0x0000, 0xA696, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data40[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0xA722, 0x0000, 0xA724, 0x0000, 0xA726, 0x0000, 0xA728, 0x0000, 0xA72A, 0x0000, 0xA72C, 0x0000, 0xA72E,
                                     0x0000, 0x0000, 0x0000, 0xA732, 0x0000, 0xA734, 0x0000, 0xA736, 0x0000, 0xA738, 0x0000, 0xA73A, 0x0000, 0xA73C, 0x0000, 0xA73E };
static const u16 upper_data41[]  = { 0x0000, 0xA740, 0x0000, 0xA742, 0x0000, 0xA744, 0x0000, 0xA746, 0x0000, 0xA748, 0x0000, 0xA74A, 0x0000, 0xA74C, 0x0000, 0xA74E,
                                     0x0000, 0xA750, 0x0000, 0xA752, 0x0000, 0xA754, 0x0000, 0xA756, 0x0000, 0xA758, 0x0000, 0xA75A, 0x0000, 0xA75C, 0x0000, 0xA75E,
                                     0x0000, 0xA760, 0x0000, 0xA762, 0x0000, 0xA764, 0x0000, 0xA766, 0x0000, 0xA768, 0x0000, 0xA76A, 0x0000, 0xA76C, 0x0000, 0xA76E,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xA779, 0x0000, 0xA77B, 0x0000, 0x0000, 0xA77E };
static const u16 upper_data42[]  = { 0x0000, 0xA780, 0x0000, 0xA782, 0x0000, 0xA784, 0x0000, 0xA786, 0x0000, 0x0000, 0x0000, 0x0000, 0xA78B, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 upper_data43[]  = { 0x0000, 0xFF21, 0xFF22, 0xFF23, 0xFF24, 0xFF25, 0xFF26, 0xFF27, 0xFF28, 0xFF29, 0xFF2A, 0xFF2B, 0xFF2C, 0xFF2D, 0xFF2E, 0xFF2F,
                                     0xFF30, 0xFF31, 0xFF32, 0xFF33, 0xFF34, 0xFF35, 0xFF36, 0xFF37, 0xFF38, 0xFF39, 0xFF3A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };

static const u16 *upper_data_table[UPPER_BLOCK_COUNT] = {
upper_data0,upper_data1,upper_data2,upper_data3,upper_data4,upper_data5,upper_data6,upper_data7,upper_data8,upper_data9,
upper_data10,upper_data11,upper_data12,upper_data13,upper_data14,upper_data15,upper_data16,upper_data17,upper_data18,upper_data19,
upper_data20,upper_data21,upper_data22,upper_data23,upper_data24,upper_data25,upper_data26,upper_data27,upper_data28,upper_data29,
upper_data30,upper_data31,upper_data32,upper_data33,upper_data34,upper_data35,upper_data36,upper_data37,upper_data38,upper_data39,
upper_data40,upper_data41,upper_data42,upper_data43
};
/* Generated by JcD. Do not modify. End upper_tables */

SQLITE_PRIVATE u32 unifuzz_upper(
    u32 c
){
    u32 p;
    if (c >= 0x10000) return c;
    p = (upper_data_table[upper_indexes[(c) >> UPPER_BLOCK_SHIFT]][(c) & UPPER_BLOCK_MASK]);
    return (p == 0) ? c : p;
}


/* Generated by builder. Do not modify. Start title_defines */
#define TITLE_BLOCK_SHIFT 6
#define TITLE_BLOCK_MASK ((1 << TITLE_BLOCK_SHIFT) - 1)
#define TITLE_BLOCK_SIZE (1 << TITLE_BLOCK_SHIFT)
#define TITLE_BLOCK_COUNT 44
#define TITLE_INDEXES_SIZE (0x10000 >> TITLE_BLOCK_SHIFT)
/* Generated by builder. Do not modify. End title_defines */

/* Generated by JcD. Do not modify. Start title_tables */
static const u16 title_indexes[TITLE_INDEXES_SIZE] = {
   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,   0,   0,  11,  12,
  13,  14,  15,  16,  17,  18,  19,  20,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  21,   0,   0,
  22,  23,  24,  25,  26,  27,  28,  29,   0,   0,   0,   0,   0,  30,  31,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  32,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  33,  34,  35,  36,
  37,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,  38,  39,   0,  40,  41,  42,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,  43,   0
};

static const u16 title_data0[]   = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data1[]   = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
                                     0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data2[]   = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x039C, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data3[]   = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7, 0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
                                     0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x0000, 0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x0178 };
static const u16 title_data4[]   = { 0x0000, 0x0100, 0x0000, 0x0102, 0x0000, 0x0104, 0x0000, 0x0106, 0x0000, 0x0108, 0x0000, 0x010A, 0x0000, 0x010C, 0x0000, 0x010E,
                                     0x0000, 0x0110, 0x0000, 0x0112, 0x0000, 0x0114, 0x0000, 0x0116, 0x0000, 0x0118, 0x0000, 0x011A, 0x0000, 0x011C, 0x0000, 0x011E,
                                     0x0000, 0x0120, 0x0000, 0x0122, 0x0000, 0x0124, 0x0000, 0x0126, 0x0000, 0x0128, 0x0000, 0x012A, 0x0000, 0x012C, 0x0000, 0x012E,
                                     0x0000, 0x0049, 0x0000, 0x0132, 0x0000, 0x0134, 0x0000, 0x0136, 0x0000, 0x0000, 0x0139, 0x0000, 0x013B, 0x0000, 0x013D, 0x0000 };
static const u16 title_data5[]   = { 0x013F, 0x0000, 0x0141, 0x0000, 0x0143, 0x0000, 0x0145, 0x0000, 0x0147, 0x0000, 0x0000, 0x014A, 0x0000, 0x014C, 0x0000, 0x014E,
                                     0x0000, 0x0150, 0x0000, 0x0152, 0x0000, 0x0154, 0x0000, 0x0156, 0x0000, 0x0158, 0x0000, 0x015A, 0x0000, 0x015C, 0x0000, 0x015E,
                                     0x0000, 0x0160, 0x0000, 0x0162, 0x0000, 0x0164, 0x0000, 0x0166, 0x0000, 0x0168, 0x0000, 0x016A, 0x0000, 0x016C, 0x0000, 0x016E,
                                     0x0000, 0x0170, 0x0000, 0x0172, 0x0000, 0x0174, 0x0000, 0x0176, 0x0000, 0x0000, 0x0179, 0x0000, 0x017B, 0x0000, 0x017D, 0x0053 };
static const u16 title_data6[]   = { 0x0243, 0x0000, 0x0000, 0x0182, 0x0000, 0x0184, 0x0000, 0x0000, 0x0187, 0x0000, 0x0000, 0x0000, 0x018B, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0191, 0x0000, 0x0000, 0x01F6, 0x0000, 0x0000, 0x0000, 0x0198, 0x023D, 0x0000, 0x0000, 0x0000, 0x0220, 0x0000,
                                     0x0000, 0x01A0, 0x0000, 0x01A2, 0x0000, 0x01A4, 0x0000, 0x0000, 0x01A7, 0x0000, 0x0000, 0x0000, 0x0000, 0x01AC, 0x0000, 0x0000,
                                     0x01AF, 0x0000, 0x0000, 0x0000, 0x01B3, 0x0000, 0x01B5, 0x0000, 0x0000, 0x01B8, 0x0000, 0x0000, 0x0000, 0x01BC, 0x0000, 0x01F7 };
static const u16 title_data7[]   = { 0x0000, 0x0000, 0x0000, 0x0000, 0x01C5, 0x0000, 0x01C5, 0x01C8, 0x0000, 0x01C8, 0x01CB, 0x0000, 0x01CB, 0x0000, 0x01CD, 0x0000,
                                     0x01CF, 0x0000, 0x01D1, 0x0000, 0x01D3, 0x0000, 0x01D5, 0x0000, 0x01D7, 0x0000, 0x01D9, 0x0000, 0x01DB, 0x018E, 0x0000, 0x01DE,
                                     0x0000, 0x01E0, 0x0000, 0x01E2, 0x0000, 0x01E4, 0x0000, 0x01E6, 0x0000, 0x01E8, 0x0000, 0x01EA, 0x0000, 0x01EC, 0x0000, 0x01EE,
                                     0x0000, 0x01F2, 0x0000, 0x01F2, 0x0000, 0x01F4, 0x0000, 0x0000, 0x0000, 0x01F8, 0x0000, 0x01FA, 0x0000, 0x01FC, 0x0000, 0x01FE };
static const u16 title_data8[]   = { 0x0000, 0x0200, 0x0000, 0x0202, 0x0000, 0x0204, 0x0000, 0x0206, 0x0000, 0x0208, 0x0000, 0x020A, 0x0000, 0x020C, 0x0000, 0x020E,
                                     0x0000, 0x0210, 0x0000, 0x0212, 0x0000, 0x0214, 0x0000, 0x0216, 0x0000, 0x0218, 0x0000, 0x021A, 0x0000, 0x021C, 0x0000, 0x021E,
                                     0x0000, 0x0000, 0x0000, 0x0222, 0x0000, 0x0224, 0x0000, 0x0226, 0x0000, 0x0228, 0x0000, 0x022A, 0x0000, 0x022C, 0x0000, 0x022E,
                                     0x0000, 0x0230, 0x0000, 0x0232, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x023B, 0x0000, 0x0000, 0x0000 };
static const u16 title_data9[]   = { 0x0000, 0x0000, 0x0241, 0x0000, 0x0000, 0x0000, 0x0000, 0x0246, 0x0000, 0x0248, 0x0000, 0x024A, 0x0000, 0x024C, 0x0000, 0x024E,
                                     0x2C6F, 0x2C6D, 0x0000, 0x0181, 0x0186, 0x0000, 0x0189, 0x018A, 0x0000, 0x018F, 0x0000, 0x0190, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0193, 0x0000, 0x0000, 0x0194, 0x0000, 0x0000, 0x0000, 0x0000, 0x0197, 0x0196, 0x0000, 0x2C62, 0x0000, 0x0000, 0x0000, 0x019C,
                                     0x0000, 0x2C6E, 0x019D, 0x0000, 0x0000, 0x019F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2C64, 0x0000, 0x0000 };
static const u16 title_data10[]  = { 0x01A6, 0x0000, 0x0000, 0x01A9, 0x0000, 0x0000, 0x0000, 0x0000, 0x01AE, 0x0244, 0x01B1, 0x01B2, 0x0245, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x01B7, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data11[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0399, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0370, 0x0000, 0x0372, 0x0000, 0x0000, 0x0000, 0x0376, 0x0000, 0x0000, 0x0000, 0x03FD, 0x03FE, 0x03FF, 0x0000, 0x0000 };
static const u16 title_data12[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0386, 0x0388, 0x0389, 0x038A,
                                     0x0000, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397, 0x0398, 0x0399, 0x039A, 0x039B, 0x039C, 0x039D, 0x039E, 0x039F };
static const u16 title_data13[]  = { 0x03A0, 0x03A1, 0x03A3, 0x03A3, 0x03A4, 0x03A5, 0x03A6, 0x03A7, 0x03A8, 0x03A9, 0x03AA, 0x03AB, 0x038C, 0x038E, 0x038F, 0x0000,
                                     0x0392, 0x0398, 0x0000, 0x0000, 0x0000, 0x03A6, 0x03A0, 0x03CF, 0x0000, 0x03D8, 0x0000, 0x03DA, 0x0000, 0x03DC, 0x0000, 0x03DE,
                                     0x0000, 0x03E0, 0x0000, 0x03E2, 0x0000, 0x03E4, 0x0000, 0x03E6, 0x0000, 0x03E8, 0x0000, 0x03EA, 0x0000, 0x03EC, 0x0000, 0x03EE,
                                     0x039A, 0x03A1, 0x03F9, 0x0000, 0x0000, 0x0395, 0x0000, 0x0000, 0x03F7, 0x0000, 0x0000, 0x03FA, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data14[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F };
static const u16 title_data15[]  = { 0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
                                     0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407, 0x0408, 0x0409, 0x040A, 0x040B, 0x040C, 0x040D, 0x040E, 0x040F,
                                     0x0000, 0x0460, 0x0000, 0x0462, 0x0000, 0x0464, 0x0000, 0x0466, 0x0000, 0x0468, 0x0000, 0x046A, 0x0000, 0x046C, 0x0000, 0x046E,
                                     0x0000, 0x0470, 0x0000, 0x0472, 0x0000, 0x0474, 0x0000, 0x0476, 0x0000, 0x0478, 0x0000, 0x047A, 0x0000, 0x047C, 0x0000, 0x047E };
static const u16 title_data16[]  = { 0x0000, 0x0480, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x048A, 0x0000, 0x048C, 0x0000, 0x048E,
                                     0x0000, 0x0490, 0x0000, 0x0492, 0x0000, 0x0494, 0x0000, 0x0496, 0x0000, 0x0498, 0x0000, 0x049A, 0x0000, 0x049C, 0x0000, 0x049E,
                                     0x0000, 0x04A0, 0x0000, 0x04A2, 0x0000, 0x04A4, 0x0000, 0x04A6, 0x0000, 0x04A8, 0x0000, 0x04AA, 0x0000, 0x04AC, 0x0000, 0x04AE,
                                     0x0000, 0x04B0, 0x0000, 0x04B2, 0x0000, 0x04B4, 0x0000, 0x04B6, 0x0000, 0x04B8, 0x0000, 0x04BA, 0x0000, 0x04BC, 0x0000, 0x04BE };
static const u16 title_data17[]  = { 0x0000, 0x0000, 0x04C1, 0x0000, 0x04C3, 0x0000, 0x04C5, 0x0000, 0x04C7, 0x0000, 0x04C9, 0x0000, 0x04CB, 0x0000, 0x04CD, 0x04C0,
                                     0x0000, 0x04D0, 0x0000, 0x04D2, 0x0000, 0x04D4, 0x0000, 0x04D6, 0x0000, 0x04D8, 0x0000, 0x04DA, 0x0000, 0x04DC, 0x0000, 0x04DE,
                                     0x0000, 0x04E0, 0x0000, 0x04E2, 0x0000, 0x04E4, 0x0000, 0x04E6, 0x0000, 0x04E8, 0x0000, 0x04EA, 0x0000, 0x04EC, 0x0000, 0x04EE,
                                     0x0000, 0x04F0, 0x0000, 0x04F2, 0x0000, 0x04F4, 0x0000, 0x04F6, 0x0000, 0x04F8, 0x0000, 0x04FA, 0x0000, 0x04FC, 0x0000, 0x04FE };
static const u16 title_data18[]  = { 0x0000, 0x0500, 0x0000, 0x0502, 0x0000, 0x0504, 0x0000, 0x0506, 0x0000, 0x0508, 0x0000, 0x050A, 0x0000, 0x050C, 0x0000, 0x050E,
                                     0x0000, 0x0510, 0x0000, 0x0512, 0x0000, 0x0514, 0x0000, 0x0516, 0x0000, 0x0518, 0x0000, 0x051A, 0x0000, 0x051C, 0x0000, 0x051E,
                                     0x0000, 0x0520, 0x0000, 0x0522, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data19[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0531, 0x0532, 0x0533, 0x0534, 0x0535, 0x0536, 0x0537, 0x0538, 0x0539, 0x053A, 0x053B, 0x053C, 0x053D, 0x053E, 0x053F,
                                     0x0540, 0x0541, 0x0542, 0x0543, 0x0544, 0x0545, 0x0546, 0x0547, 0x0548, 0x0549, 0x054A, 0x054B, 0x054C, 0x054D, 0x054E, 0x054F };
static const u16 title_data20[]  = { 0x0550, 0x0551, 0x0552, 0x0553, 0x0554, 0x0555, 0x0556, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data21[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xA77D, 0x0000, 0x0000, 0x0000, 0x2C63, 0x0000, 0x0000 };
static const u16 title_data22[]  = { 0x0000, 0x1E00, 0x0000, 0x1E02, 0x0000, 0x1E04, 0x0000, 0x1E06, 0x0000, 0x1E08, 0x0000, 0x1E0A, 0x0000, 0x1E0C, 0x0000, 0x1E0E,
                                     0x0000, 0x1E10, 0x0000, 0x1E12, 0x0000, 0x1E14, 0x0000, 0x1E16, 0x0000, 0x1E18, 0x0000, 0x1E1A, 0x0000, 0x1E1C, 0x0000, 0x1E1E,
                                     0x0000, 0x1E20, 0x0000, 0x1E22, 0x0000, 0x1E24, 0x0000, 0x1E26, 0x0000, 0x1E28, 0x0000, 0x1E2A, 0x0000, 0x1E2C, 0x0000, 0x1E2E,
                                     0x0000, 0x1E30, 0x0000, 0x1E32, 0x0000, 0x1E34, 0x0000, 0x1E36, 0x0000, 0x1E38, 0x0000, 0x1E3A, 0x0000, 0x1E3C, 0x0000, 0x1E3E };
static const u16 title_data23[]  = { 0x0000, 0x1E40, 0x0000, 0x1E42, 0x0000, 0x1E44, 0x0000, 0x1E46, 0x0000, 0x1E48, 0x0000, 0x1E4A, 0x0000, 0x1E4C, 0x0000, 0x1E4E,
                                     0x0000, 0x1E50, 0x0000, 0x1E52, 0x0000, 0x1E54, 0x0000, 0x1E56, 0x0000, 0x1E58, 0x0000, 0x1E5A, 0x0000, 0x1E5C, 0x0000, 0x1E5E,
                                     0x0000, 0x1E60, 0x0000, 0x1E62, 0x0000, 0x1E64, 0x0000, 0x1E66, 0x0000, 0x1E68, 0x0000, 0x1E6A, 0x0000, 0x1E6C, 0x0000, 0x1E6E,
                                     0x0000, 0x1E70, 0x0000, 0x1E72, 0x0000, 0x1E74, 0x0000, 0x1E76, 0x0000, 0x1E78, 0x0000, 0x1E7A, 0x0000, 0x1E7C, 0x0000, 0x1E7E };
static const u16 title_data24[]  = { 0x0000, 0x1E80, 0x0000, 0x1E82, 0x0000, 0x1E84, 0x0000, 0x1E86, 0x0000, 0x1E88, 0x0000, 0x1E8A, 0x0000, 0x1E8C, 0x0000, 0x1E8E,
                                     0x0000, 0x1E90, 0x0000, 0x1E92, 0x0000, 0x1E94, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1E60, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x1EA0, 0x0000, 0x1EA2, 0x0000, 0x1EA4, 0x0000, 0x1EA6, 0x0000, 0x1EA8, 0x0000, 0x1EAA, 0x0000, 0x1EAC, 0x0000, 0x1EAE,
                                     0x0000, 0x1EB0, 0x0000, 0x1EB2, 0x0000, 0x1EB4, 0x0000, 0x1EB6, 0x0000, 0x1EB8, 0x0000, 0x1EBA, 0x0000, 0x1EBC, 0x0000, 0x1EBE };
static const u16 title_data25[]  = { 0x0000, 0x1EC0, 0x0000, 0x1EC2, 0x0000, 0x1EC4, 0x0000, 0x1EC6, 0x0000, 0x1EC8, 0x0000, 0x1ECA, 0x0000, 0x1ECC, 0x0000, 0x1ECE,
                                     0x0000, 0x1ED0, 0x0000, 0x1ED2, 0x0000, 0x1ED4, 0x0000, 0x1ED6, 0x0000, 0x1ED8, 0x0000, 0x1EDA, 0x0000, 0x1EDC, 0x0000, 0x1EDE,
                                     0x0000, 0x1EE0, 0x0000, 0x1EE2, 0x0000, 0x1EE4, 0x0000, 0x1EE6, 0x0000, 0x1EE8, 0x0000, 0x1EEA, 0x0000, 0x1EEC, 0x0000, 0x1EEE,
                                     0x0000, 0x1EF0, 0x0000, 0x1EF2, 0x0000, 0x1EF4, 0x0000, 0x1EF6, 0x0000, 0x1EF8, 0x0000, 0x1EFA, 0x0000, 0x1EFC, 0x0000, 0x1EFE };
static const u16 title_data26[]  = { 0x1F08, 0x1F09, 0x1F0A, 0x1F0B, 0x1F0C, 0x1F0D, 0x1F0E, 0x1F0F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1F18, 0x1F19, 0x1F1A, 0x1F1B, 0x1F1C, 0x1F1D, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1F28, 0x1F29, 0x1F2A, 0x1F2B, 0x1F2C, 0x1F2D, 0x1F2E, 0x1F2F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1F38, 0x1F39, 0x1F3A, 0x1F3B, 0x1F3C, 0x1F3D, 0x1F3E, 0x1F3F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data27[]  = { 0x1F48, 0x1F49, 0x1F4A, 0x1F4B, 0x1F4C, 0x1F4D, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x1F59, 0x0000, 0x1F5B, 0x0000, 0x1F5D, 0x0000, 0x1F5F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1F68, 0x1F69, 0x1F6A, 0x1F6B, 0x1F6C, 0x1F6D, 0x1F6E, 0x1F6F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1FBA, 0x1FBB, 0x1FC8, 0x1FC9, 0x1FCA, 0x1FCB, 0x1FDA, 0x1FDB, 0x1FF8, 0x1FF9, 0x1FEA, 0x1FEB, 0x1FFA, 0x1FFB, 0x0000, 0x0000 };
static const u16 title_data28[]  = { 0x1F88, 0x1F89, 0x1F8A, 0x1F8B, 0x1F8C, 0x1F8D, 0x1F8E, 0x1F8F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1F98, 0x1F99, 0x1F9A, 0x1F9B, 0x1F9C, 0x1F9D, 0x1F9E, 0x1F9F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1FA8, 0x1FA9, 0x1FAA, 0x1FAB, 0x1FAC, 0x1FAD, 0x1FAE, 0x1FAF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1FB8, 0x1FB9, 0x0000, 0x1FBC, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0399, 0x0000 };
static const u16 title_data29[]  = { 0x0000, 0x0000, 0x0000, 0x1FCC, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1FD8, 0x1FD9, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x1FE8, 0x1FE9, 0x0000, 0x0000, 0x0000, 0x1FEC, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x1FFC, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data30[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2132, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x2160, 0x2161, 0x2162, 0x2163, 0x2164, 0x2165, 0x2166, 0x2167, 0x2168, 0x2169, 0x216A, 0x216B, 0x216C, 0x216D, 0x216E, 0x216F };
static const u16 title_data31[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x2183, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data32[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x24B6, 0x24B7, 0x24B8, 0x24B9, 0x24BA, 0x24BB, 0x24BC, 0x24BD, 0x24BE, 0x24BF, 0x24C0, 0x24C1, 0x24C2, 0x24C3, 0x24C4, 0x24C5,
                                     0x24C6, 0x24C7, 0x24C8, 0x24C9, 0x24CA, 0x24CB, 0x24CC, 0x24CD, 0x24CE, 0x24CF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data33[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x2C00, 0x2C01, 0x2C02, 0x2C03, 0x2C04, 0x2C05, 0x2C06, 0x2C07, 0x2C08, 0x2C09, 0x2C0A, 0x2C0B, 0x2C0C, 0x2C0D, 0x2C0E, 0x2C0F };
static const u16 title_data34[]  = { 0x2C10, 0x2C11, 0x2C12, 0x2C13, 0x2C14, 0x2C15, 0x2C16, 0x2C17, 0x2C18, 0x2C19, 0x2C1A, 0x2C1B, 0x2C1C, 0x2C1D, 0x2C1E, 0x2C1F,
                                     0x2C20, 0x2C21, 0x2C22, 0x2C23, 0x2C24, 0x2C25, 0x2C26, 0x2C27, 0x2C28, 0x2C29, 0x2C2A, 0x2C2B, 0x2C2C, 0x2C2D, 0x2C2E, 0x0000,
                                     0x0000, 0x2C60, 0x0000, 0x0000, 0x0000, 0x023A, 0x023E, 0x0000, 0x2C67, 0x0000, 0x2C69, 0x0000, 0x2C6B, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x2C72, 0x0000, 0x0000, 0x2C75, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data35[]  = { 0x0000, 0x2C80, 0x0000, 0x2C82, 0x0000, 0x2C84, 0x0000, 0x2C86, 0x0000, 0x2C88, 0x0000, 0x2C8A, 0x0000, 0x2C8C, 0x0000, 0x2C8E,
                                     0x0000, 0x2C90, 0x0000, 0x2C92, 0x0000, 0x2C94, 0x0000, 0x2C96, 0x0000, 0x2C98, 0x0000, 0x2C9A, 0x0000, 0x2C9C, 0x0000, 0x2C9E,
                                     0x0000, 0x2CA0, 0x0000, 0x2CA2, 0x0000, 0x2CA4, 0x0000, 0x2CA6, 0x0000, 0x2CA8, 0x0000, 0x2CAA, 0x0000, 0x2CAC, 0x0000, 0x2CAE,
                                     0x0000, 0x2CB0, 0x0000, 0x2CB2, 0x0000, 0x2CB4, 0x0000, 0x2CB6, 0x0000, 0x2CB8, 0x0000, 0x2CBA, 0x0000, 0x2CBC, 0x0000, 0x2CBE };
static const u16 title_data36[]  = { 0x0000, 0x2CC0, 0x0000, 0x2CC2, 0x0000, 0x2CC4, 0x0000, 0x2CC6, 0x0000, 0x2CC8, 0x0000, 0x2CCA, 0x0000, 0x2CCC, 0x0000, 0x2CCE,
                                     0x0000, 0x2CD0, 0x0000, 0x2CD2, 0x0000, 0x2CD4, 0x0000, 0x2CD6, 0x0000, 0x2CD8, 0x0000, 0x2CDA, 0x0000, 0x2CDC, 0x0000, 0x2CDE,
                                     0x0000, 0x2CE0, 0x0000, 0x2CE2, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data37[]  = { 0x10A0, 0x10A1, 0x10A2, 0x10A3, 0x10A4, 0x10A5, 0x10A6, 0x10A7, 0x10A8, 0x10A9, 0x10AA, 0x10AB, 0x10AC, 0x10AD, 0x10AE, 0x10AF,
                                     0x10B0, 0x10B1, 0x10B2, 0x10B3, 0x10B4, 0x10B5, 0x10B6, 0x10B7, 0x10B8, 0x10B9, 0x10BA, 0x10BB, 0x10BC, 0x10BD, 0x10BE, 0x10BF,
                                     0x10C0, 0x10C1, 0x10C2, 0x10C3, 0x10C4, 0x10C5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data38[]  = { 0x0000, 0xA640, 0x0000, 0xA642, 0x0000, 0xA644, 0x0000, 0xA646, 0x0000, 0xA648, 0x0000, 0xA64A, 0x0000, 0xA64C, 0x0000, 0xA64E,
                                     0x0000, 0xA650, 0x0000, 0xA652, 0x0000, 0xA654, 0x0000, 0xA656, 0x0000, 0xA658, 0x0000, 0xA65A, 0x0000, 0xA65C, 0x0000, 0xA65E,
                                     0x0000, 0x0000, 0x0000, 0xA662, 0x0000, 0xA664, 0x0000, 0xA666, 0x0000, 0xA668, 0x0000, 0xA66A, 0x0000, 0xA66C, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data39[]  = { 0x0000, 0xA680, 0x0000, 0xA682, 0x0000, 0xA684, 0x0000, 0xA686, 0x0000, 0xA688, 0x0000, 0xA68A, 0x0000, 0xA68C, 0x0000, 0xA68E,
                                     0x0000, 0xA690, 0x0000, 0xA692, 0x0000, 0xA694, 0x0000, 0xA696, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data40[]  = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0xA722, 0x0000, 0xA724, 0x0000, 0xA726, 0x0000, 0xA728, 0x0000, 0xA72A, 0x0000, 0xA72C, 0x0000, 0xA72E,
                                     0x0000, 0x0000, 0x0000, 0xA732, 0x0000, 0xA734, 0x0000, 0xA736, 0x0000, 0xA738, 0x0000, 0xA73A, 0x0000, 0xA73C, 0x0000, 0xA73E };
static const u16 title_data41[]  = { 0x0000, 0xA740, 0x0000, 0xA742, 0x0000, 0xA744, 0x0000, 0xA746, 0x0000, 0xA748, 0x0000, 0xA74A, 0x0000, 0xA74C, 0x0000, 0xA74E,
                                     0x0000, 0xA750, 0x0000, 0xA752, 0x0000, 0xA754, 0x0000, 0xA756, 0x0000, 0xA758, 0x0000, 0xA75A, 0x0000, 0xA75C, 0x0000, 0xA75E,
                                     0x0000, 0xA760, 0x0000, 0xA762, 0x0000, 0xA764, 0x0000, 0xA766, 0x0000, 0xA768, 0x0000, 0xA76A, 0x0000, 0xA76C, 0x0000, 0xA76E,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xA779, 0x0000, 0xA77B, 0x0000, 0x0000, 0xA77E };
static const u16 title_data42[]  = { 0x0000, 0xA780, 0x0000, 0xA782, 0x0000, 0xA784, 0x0000, 0xA786, 0x0000, 0x0000, 0x0000, 0x0000, 0xA78B, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
static const u16 title_data43[]  = { 0x0000, 0xFF21, 0xFF22, 0xFF23, 0xFF24, 0xFF25, 0xFF26, 0xFF27, 0xFF28, 0xFF29, 0xFF2A, 0xFF2B, 0xFF2C, 0xFF2D, 0xFF2E, 0xFF2F,
                                     0xFF30, 0xFF31, 0xFF32, 0xFF33, 0xFF34, 0xFF35, 0xFF36, 0xFF37, 0xFF38, 0xFF39, 0xFF3A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };

static const u16 *title_data_table[TITLE_BLOCK_COUNT] = {
title_data0,title_data1,title_data2,title_data3,title_data4,title_data5,title_data6,title_data7,title_data8,title_data9,
title_data10,title_data11,title_data12,title_data13,title_data14,title_data15,title_data16,title_data17,title_data18,title_data19,
title_data20,title_data21,title_data22,title_data23,title_data24,title_data25,title_data26,title_data27,title_data28,title_data29,
title_data30,title_data31,title_data32,title_data33,title_data34,title_data35,title_data36,title_data37,title_data38,title_data39,
title_data40,title_data41,title_data42,title_data43
};
/* Generated by JcD. Do not modify. End title_tables */

SQLITE_PRIVATE u32 unifuzz_title(
    u32 c
){
    u32 p;
    if (c >= 0x10000) return c;
    p = (title_data_table[title_indexes[(c) >> TITLE_BLOCK_SHIFT]][(c) & TITLE_BLOCK_MASK]);
    return (p == 0) ? c : p;
}


SQLITE_PRIVATE const u8 hexdigits[] = "0123456789ABCDEF";


/*
** Structure used to store the codepoint ranges for whitespaces and
** control characters (some are not actual whitespaces)
** but it's extremely unlikely that this could cause any toxic behavior.
** The table must be sorted on increasing range start point.
*/
typedef struct spacing_ {
    u32 lo;
    u32 hi;
} SpacingT;

SQLITE_PRIVATE SpacingT space_table[] = {
    {0x000000, 0x000020},   // control characters, most are unlikely
    {0x000080, 0x0000A0},   // control characters, other set also most unlikely
    {0x001680, 0x001680},	// Ogham sp mark
    {0x00180E, 0x00180E},	// Mongolian vowel separator
    {0x002000, 0x00200A},	// En quad, Em quad, En sp, Em sp, 3/em sp, 4/em sp, 6/em sp, Figure sp, Punctuation sp, Thin sp, Hair sp,
    {0x002028, 0x002029},	// Line separator, Paragraph separator
    {0x00205F, 0x00205F},	// Medium mathematical sp
    {0x003000, 0x003000},	// Ideographic sp
    {0x00FEFF, 0x00FEFF},   // BOM
    {0xFFFFFF, 0xFFFFFF}    // end of table
};


/*
** Allocate nByte bytes of space using sqlite3_malloc(). If the
** allocation fails, call sqlite3_result_error_nomem() to notify
** the database handle that malloc() has failed.
*/
SQLITE_PRIVATE void *contextMalloc(
    sqlite3_context *context,
    int nByte
){
    void *z = sqlite3_malloc((int) nByte);
	if ((z == 0) && (nByte > 0)) {
    	sqlite3_result_error_nomem(context);
    }
    return z;
}

/*
** Reallocate nByte bytes of space using sqlite3_realloc(). If the
** allocation fails, call sqlite3_result_error_nomem() to notify
** the database handle that realloc() has failed.
*/
SQLITE_PRIVATE void *contextRealloc(
    sqlite3_context *context,
    void* pPrior,
    int nByte
){
    void *z = sqlite3_realloc(pPrior, (int) nByte);
    if ((z == 0) && (nByte > 0)) {
		sqlite3_result_error_nomem(context);
    }
    return z;
}


/*
** Implementation of the VERSION(*) function.  The result is the version
** of the unicode library that is running and the unifuzz version.
*/
SQLITE_PRIVATE void versionFunc(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    UNUSED_PARAMETER(argc);
    UNUSED_PARAMETER(argv);

    sqlite3_result_text(context, VERSION_STRINGS, -1, SQLITE_STATIC);
}



/*
** Maximum length (in bytes) of the pattern in a LIKE or GLOB
** operator.
*/
#ifndef SQLITE_MAX_LIKE_PATTERN_LENGTH
# define SQLITE_MAX_LIKE_PATTERN_LENGTH 50000
#endif

/*
** A structure defining how to do GLOB-style comparisons.
*/
typedef struct compareInfo {
    u8 matchAll;
    u8 matchOne;
    u8 matchSet;
    u8 noCase;
} compareInfoT;

static compareInfoT      globInfo = { '*', '?', '[', 0 };
/* The correct SQL-92 behavior is for the LIKE operator to ignore
** case.  Thus  'a' LIKE 'A' would be true. */
static compareInfoT  likeInfoNorm = { '%', '_',   0, 1 };
/* If SQLITE_CASE_SENSITIVE_LIKE is defined, then the LIKE operator
** is case sensitive causing 'a' LIKE 'A' to be false */
//static compareInfoT   likeInfoAlt = { '%', '_',   0, 0 };

/*
** Compare two UTF-32 strings for equality where the first string can
** potentially be a "glob" expression.  Return true (1) if they
** are the same and false (0) if they are different.
**
** Globbing rules:
**
**      '*'       Matches any sequence of zero or more characters.
**
**      '?'       Matches exactly one character.
**
**     [...]      Matches one character from the enclosed list of
**                characters.
**
**     [^...]     Matches one character not in the enclosed list.
**
** With the [...] and [^...] matching, a ']' character can be included
** in the list by making it the first character after '[' or '^'.  A
** range of characters can be specified using '-'.  Example:
** "[a-z]" matches any single lower-case letter.  To match a '-', make
** it the last character in the list.
**
** This routine is usually quick, but can be N**2 in the worst case.
**
** Hints: to match '*' or '?', put them in "[]".  Like this:
**
**         abc[*]xyz        Matches "abc*xyz" only
*/
SQLITE_PRIVATE int patternCompare(
    sqlite3_context *context,
    const u32 *zPattern,              /* The glob pattern */
    const u32 *zString,               /* The string to compare against the glob */
    const struct compareInfo *pInfo,  /* Information about how to do the compare */
    const u32 esc                     /* The escape character */
){
    u32 c, c2;
    u32 invert;
    u32 seen;
    u32 matchOne = (u32) pInfo->matchOne;
    u32 matchAll = (u32) pInfo->matchAll;
    u32 matchSet = (u32) pInfo->matchSet;
    int prevEscape = 0;               /* True if the previous character was 'escape' */

    while ((c = *zPattern++) != 0) {
        if ((! prevEscape) && (c == matchAll)) {
            while (((c = *zPattern++) == matchAll) || (c == matchOne)) {
                if ((c == matchOne) && (*zString++ == 0)) {
                    return 0;
                }
            }
            if (c == 0) {
                return 1;
            }
            else if (c == esc) {
                if ((c = *zPattern++) == 0) {
                    return 0;
                }
            } else if (c == matchSet) {
assert(esc == 0);         /* This is GLOB, not LIKE */
//assert(matchSet < 0x80);  /* '[' is a single-byte character */               no more needed: 1 char can be anything
                while ((*zString) && (patternCompare(context, &zPattern[-1], zString, pInfo, esc) == 0)) {
                    zString++;
                }
                return(*zString != 0);
            }
            while ((c2 = *zString++) != 0) {
                while ((c2 != 0) && (c2 != c )) {
                    c2 = *zString++;
                }
                if (c2 == 0) {
                    return 0;
                }
                if (patternCompare(context, zPattern, zString, pInfo, esc)) {
                    return 1;
                }
            }
            return 0;
        } else if ((!prevEscape) && (c == matchOne)) {
            if (*zString++ == 0) {
                return 0;
            }
        } else if (c == matchSet) {
            u32 prior_c = 0;
assert(esc == 0);    /* This only occurs for GLOB, not LIKE */
            seen = 0;
            invert = 0;
            c = *zString++;
            if (c == 0) {
                return 0;
            }
            c2 = *zPattern++;
            if (c2 == '^') {
                invert = 1;
                c2 = *zPattern++;
            }
            if (c2 == ']') {
                if (c == ']') {
                    seen = 1;
                }
                c2 = *zPattern++;
            }
            while ((c2 != 0) && (c2 != ']')) {
                if ((c2 == '-') && (*zPattern != ']') && (*zPattern != 0) && (prior_c > 0)) {
                    c2 = *zPattern++;
                    if ((c >= prior_c) && (c <= c2)) {
                        seen = 1;
                    }
                    prior_c = 0;
                } else {
                    if (c == c2) {
                        seen = 1;
                    }
                    prior_c = c2;
                }
                c2 = *zPattern++;
            }
            if ((c2 == 0) || ((seen ^ invert) == 0)) {
                return 0;
            }
        } else if ((esc == c) && (!prevEscape)) {
            prevEscape = 1;
        } else {
            c2 = *zString++;
            if (c != c2) {
                return 0;
            }
            prevEscape = 0;
        }
    }
    return(*zString == 0);
}


#define UNIFUZZ_MAX_PRINTF_ARGS     32

SQLITE_PRIVATE void printfFunc(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    u8 *nullStr = (u8 *)"<NULL>";
    u8 *fmt;
    typedef union {
        i64 i;
        double d;
        void *p;
    } argT;
    struct {
        argT args[UNIFUZZ_MAX_PRINTF_ARGS];
    } argpile = { 0 };
    u8 *pargs = (u8 *) &argpile;
    int i;
    if ((argc < 1) || (sqlite3_value_type(argv[0]) != SQLITE3_TEXT)) {
        sqlite3_result_error(context, "printf() requires a format string.", -1);
        return;
    }
    fmt = (u8 *) sqlite3_value_text(argv[0]);
    for (i = 1; i < argc; i++) {
        switch (sqlite3_value_type(argv[i])) {
            case (SQLITE_INTEGER) :
                *(u64 *) pargs = sqlite3_value_int64(argv[i]);
                pargs += sizeof(u64);
                break;
            case (SQLITE_FLOAT) :
                *(double *) pargs = sqlite3_value_double(argv[i]);
                pargs += sizeof(double);
                break;
            case (SQLITE3_TEXT) :
                *(u8 **) pargs = (u8 *) sqlite3_value_text(argv[i]);
                pargs += sizeof(u8 *);
                break;
            case (SQLITE_BLOB) :
                *(u8 **) pargs = (u8 *) sqlite3_value_text(argv[i]);      // this may give surprising results!
                pargs += sizeof(u8 *);
                break;
            case (SQLITE_NULL) :
                *(u8 **) pargs = nullStr;
                pargs += sizeof(u8 *);
                break;
        }
    }
    if (--argc > 0) {
        sqlite3_result_text(context, sqlite3_mprintf((const char *)fmt, argpile), -1, 0);
    } else {
        sqlite3_result_text(context, (const char *)fmt, -1, 0);
    }
}



#if defined(UNIFUZZ_UTF8) || defined(UNIFUZZ_UTF_BOTH)

/*
**==========================================================================================================
**==========================================================================================================
**
**          UTF-8 section
**
**==========================================================================================================
**==========================================================================================================
*/

/*
** This lookup table is used to help decode the first byte of
** a multi-byte UTF8 character.
*/
static const u8 sqlite3Utf8Trans1[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,
};

/*
** Translate a single UTF-8 character.  Return the unicode value.
**
** During translation, assume that the byte that zTerm points
** is a 0x00.
**
** Write a pointer to the next unread byte back into *pzNext.
**
** Notes On Invalid UTF-8:
**
**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to
**     be encoded as a multi-byte character.  Any multi-byte character that
**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.
**
**  *  This routine never allows a UTF16 surrogate value to be encoded.
**     If a multi-byte character attempts to encode a value between
**     0xd800 and 0xe000 then it is rendered as 0xfffd.
**
**  *  Bytes in the range of 0x80 through 0xbf which occur as the first
**     byte of a character are interpreted as single-byte characters
**     and rendered as themselves even though they are technically
**     invalid characters.
**
**  *  This routine accepts an infinite number of different UTF8 encodings
**     for unicode values 0x80 and greater.  It does not change over-length
**     encodings to 0xfffd as some systems recommend.
*/
#define READ_UTF8(zIn, zTerm, c) {                                                          \
    c = *(zIn++);                                                                           \
    if (c >= 0xC0) {                                                                        \
        c = sqlite3Utf8Trans1[c - 0xC0];                                                    \
        while ((zIn < zTerm) && ((*zIn & 0xC0) == 0x80)) {                                  \
            c = (c << 6) + (0x3F & *(zIn++));                                               \
        }                                                                                   \
        if ((c < 0x80) || ((c & 0xFFFFF800) == 0xD800) || ((c & 0xFFFFFFFE) == 0xFFFE)) {   \
            c = 0xFFFD;                                                                     \
        }                                                                                   \
    }                                                                                       \
}


#define WRITE_UTF8(zOut, c) {                          \
    if (c < 0x00080) {                                 \
        *zOut++ = (u8) (c & 0xFF);                     \
    } else if (c < 0x00800) {                          \
        *zOut++ = 0xC0 + (u8) ((c >>  6) & 0x1F);      \
        *zOut++ = 0x80 + (u8) ( c        & 0x3F);      \
    } else if (c < 0x10000) {                          \
        *zOut++ = 0xE0 + (u8) ((c >> 12) & 0x0F);      \
        *zOut++ = 0x80 + (u8) ((c >>  6) & 0x3F);      \
        *zOut++ = 0x80 + (u8) ( c        & 0x3F);      \
    } else {                                           \
        *zOut++ = 0xF0 + (u8) ((c >> 18) & 0x07);      \
        *zOut++ = 0x80 + (u8) ((c >> 12) & 0x3F);      \
        *zOut++ = 0x80 + (u8) ((c >>  6) & 0x3F);      \
        *zOut++ = 0x80 + (u8) ( c        & 0x3F);      \
    }                                                  \
}


/*
**==========================================================================================================
**
**          UTF-8 version of scalars functions
**
**==========================================================================================================
*/


/*
** u8 *unifuzz_utf8_unacc_utf8(sqlite3_context *context, u8 *inStr, int inBytes, int *outBytes)
**
** conversion of an UTF-8 input string of nBytes bytes into
** an unaccented zero-terminated UTF-8 string
** the character length of the output string is updated
*/
SQLITE_PRIVATE u8 *unifuzz_utf8_unacc_utf8(
    sqlite3_context *context,
    u8 *inStr,
    int inBytes,
    int *outBytes
){
    int l, k, outalloc, outbytesleft;
    u32 c, *uac;
    u8 *p, *term, *outStr, *q, *q0;

    // UTF-8 should be no longer than UTF-8, but unaccent may need more
    outalloc = outbytesleft = inBytes + inBytes % 4 + UNIFUZZ_CHUNK;
    outStr = (u8 *) contextMalloc(context, outalloc + sizeof(u8));
    if (outStr == 0) return 0;
    term = inStr + inBytes;
    q = outStr;
    if (inBytes) {
        for (p = inStr; p < term; ) {
            READ_UTF8(p, term, c)
            c = unifuzz_unacc(c, &uac, &l);
            q0 = q;
            if (l > 0) {
                // each char from unaccent _could_ need up to 4 u8 to code in UTF-8
                // we allocate as per worst case and might end up with few more bytes than strictly necessary
                while (outbytesleft < (int) (l * 4)) {
                    k = q - outStr;
                    outalloc += UNIFUZZ_CHUNK;
                    outbytesleft += UNIFUZZ_CHUNK;
                    outStr = (u8 *) contextRealloc(context, outStr, outalloc + 1);
                    if (outStr == 0) return 0;
                    q = outStr + k;
                    q0 = q;
                }
                for (k = 0; k < l; k++, uac++) {
                    WRITE_UTF8(q, *uac)      // we do not know how many u8 will be written
                }
            } else {
                WRITE_UTF8(q, c)
            }
            outbytesleft -= q - q0;          // now, we know exactly
        }
    }
    *q = 0;
    *outBytes = q - outStr;
    return(outStr);
}


/*
** u32 *unifuzz_utf8_unacc_utf32(sqlite3_context *context, u8 *inStr, int inBytes, int *outChars, int fold)
**
** conversion of an UTF-8 input string of nBytes bytes into
** a folded unaccented zero-terminated UTF-32 string
** the character length of the output string is updated
*/
SQLITE_PRIVATE u32 *unifuzz_utf8_unacc_utf32(
    sqlite3_context *context,
    u8 *inStr,
    int inBytes,
    int *outChars,
    int fold
){
    int l, k, outalloc, used;
    u8 *p, *term;
    u32 c, *uac, *outStr, *q;

    // UTF-32 will be 4 times as long as character-wise input, but unaccent can need more.
    //
    // Since we don't know exactly what will be the actual length needed (either in Unicode
    // characters or in bytes) we initially allocate as many u32 for output as inBytes.
    // We may end up having allocated too few, (then we realloc if needed) or too much by
    // a small margin (in the non degenerate case).  Anyway, the result is typically used
    // at once and then freed, so allocating a few more u32 than strictly necessary should
    // be harmless in normal use: you are not supposed to launch LIKE or TYPOS on megabytes
    // strings.
    outalloc = inBytes * sizeof(u32) + UNIFUZZ_CHUNK;
    outStr = (u32 *) contextMalloc(context, outalloc + sizeof(u32));
    if (outStr == 0) return 0;
    term = inStr + inBytes;
    q = outStr;
    used = 0;
    if (inBytes) {
        for (p = inStr; p < term; ) {
            READ_UTF8(p, term, c)
            if (fold) {
                c = unifuzz_fold_unacc(c, &uac, &l);
            } else {
                c = unifuzz_unacc(c, &uac, &l);
            }
            if (l > 0) {
                if (l > 1) {
                    while ((int) (outalloc - used * sizeof(u32)) < l) {
                        outalloc += UNIFUZZ_CHUNK;
                        outStr = (u32 *) contextRealloc(context, outStr, outalloc + sizeof(u32));
                        if (outStr == 0) return 0;
                        q = outStr + used;
                    }
                }
                for (k = 0; k < l; k++) {
                    *q++ = *uac++;
                    used++;
                }
            } else {
                *q++ = c;
                used++;
            }
        }
    }
    *q = 0;
    *outChars = used;
    return(outStr);
}


/*
** Implementation of the like() SQL function.  This function implements
** the build-in LIKE operator.  The first argument to the function is the
** pattern and the second argument is the string.  So, the SQL statements:
**
**       A LIKE B
**
** is implemented as like(B,A).
**
** This same function (with a different compareInfo structure) computes
** the GLOB operator.
*/
SQLITE_PRIVATE void likeFunc8(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    u32 escape = 0, *s1, *s2;
    int l1, l2, ll;
    compareInfoT *pInfo;
    const u8 *zA, *zB;
    if ((SQLITE_NULL == sqlite3_value_type(argv[0])) || (SQLITE_NULL == sqlite3_value_type(argv[1]))) {
        sqlite3_result_null(context);
        return;
    }
    zB = sqlite3_value_text(argv[0]);
    zA = sqlite3_value_text(argv[1]);
    l2 = sqlite3_value_bytes(argv[0]);
    l1 = sqlite3_value_bytes(argv[1]);
    if (argc == 3) {
        /* The escape character string must consist of a single UTF-8 character.
        ** Otherwise, return an error.
        */
        const u8 *zEsc = sqlite3_value_text(argv[2]);
        if (zEsc == 0) return;
        READ_UTF8(zEsc, 0, escape)
        if (*zEsc != 0) {
            sqlite3_result_error(context, "ESCAPE expression must be a single character", -1);
            return;
        }
    }
    /* Limit the length of the LIKE or GLOB pattern to avoid problems
    ** of deep recursion and N*N behavior in patternCompare*().
    */
    if (l2 > SQLITE_MAX_LIKE_PATTERN_LENGTH) {
        sqlite3_result_error(context, "LIKE or GLOB pattern too complex", -1);
        return;
    }
    pInfo = sqlite3_user_data(context);
    s1 = unifuzz_utf8_unacc_utf32(context, (u8 *) zA, l1, &ll, pInfo->noCase);          // conditional fold
    if (s1 != 0) {
        s2 = (u32 *) sqlite3_get_auxdata(context, 0);                                   // try to recover last value used, if any
        if (s2 == 0) {
            s2 = unifuzz_utf8_unacc_utf32(context, (u8 *) zB, l2, &ll, pInfo->noCase);  // conditional fold
            sqlite3_set_auxdata(context, 0, s2, sqlite3_free);                          // save data for next call, if any
        }
        if (s2 != 0) {
            sqlite3_result_int(context, patternCompare(context, s2, s1, pInfo, escape));
        }
        sqlite3_free(s1);
    }
}


/*
** Implementation of the UNACCENT() SQL function.
** This function decomposes each character in the supplied string
** to its components and strips any accents present in the string.
**
** This function may result to a longer output string compared
** to the original input string. Memory has been properly reallocated
** to accomodate for the extra memory length required.
*/
SQLITE_PRIVATE void unaccFunc8(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    const u8 *z1;
    u8 *z2;
    int n, l;
    UNUSED_PARAMETER(argc);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
        sqlite3_result_null(context);
        return;
    }
    z1 = (u8 *) sqlite3_value_text(argv[0]);
    n = sqlite3_value_bytes(argv[0]);
    z2 = unifuzz_utf8_unacc_utf8(context, (u8 *) z1, n, &l);
    if (z2 != 0) {
        sqlite3_result_text(context, (char *) z2, l, sqlite3_free);
    }
}


/*
** Implementation of the FLIP() SQL function.
** This function is essentially a Unicode strrev(): it returns the
** Unicode character by Unicode character reverse of its input.
** Strings in non C form are likely to be meaningless after this!
*/
SQLITE_PRIVATE void flipFunc8(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    const u8 *z1, *p, *p0;
    u8 *z2, *q;
    int n, l;
    u32 c;
    UNUSED_PARAMETER(argc);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
        sqlite3_result_null(context);
        return;
    }
    z1 = (u8 *) sqlite3_value_text(argv[0]);
    n = sqlite3_value_bytes(argv[0]);
    z2 = contextMalloc(context, n + sizeof(u8));
    if (z2) {
        for (p = (u8 *) z1, q = z2 + n; *p; ) {
            p0 = p;
            READ_UTF8(p, z1 + n, c)
            l = p - p0;
            q -= l;
            WRITE_UTF8(q, c)
            q -= l;
        }
        q[n] = 0;
        sqlite3_result_text(context, (const char *)z2, n, sqlite3_free);
    }
}


/*
** Implementation of the FOLD(), LOWER(), UPPER(), TITLE()
** scalar SQL functions.
**
** The conversion to be made depends on (sqlite3_context *)context
** where a pointer to a specific case conversion function is stored.
*/
SQLITE_PRIVATE void caseFunc8(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    const u8 *z1, *p, *term;
    u8 *z2, *q;
    int k, n, outalloc, outbytesleft;
    u32 c;
    typedef u32 (*PFN_CASEFUNC)(u32);
    PFN_CASEFUNC func;
    UNUSED_PARAMETER(argc);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
        sqlite3_result_null(context);
        return;
    }
    z1 = (u8 *) sqlite3_value_text(argv[0]);
    n = sqlite3_value_bytes(argv[0]);
    term = (u8 *) z1 + n;
    outalloc = outbytesleft = n + UNIFUZZ_CHUNK;
    z2 = contextMalloc(context, outalloc + 1);
    if (z2) {
        func = (PFN_CASEFUNC) sqlite3_user_data(context);
        for (p = (u8 *) z1, q = z2; *p; ) {
            READ_UTF8(p, term, c)
            // Special quick & dirty hack for German eszet (lower- and upper-case).
            // It uses the fact that UTF-8 encodings for those characters need more than one byte.
            // As we have allocated as many output positions as input _bytes_, we can safely
            // expand each eszet into the 'ss' or 'SS' equivalent without risk of overflow.
            // This helps keep the tables lookup fast (no length table, no realloc).
            switch (c) {
                case 0x00DF :         // 'ß'  ->  'SS'
                    if ((func == unifuzz_upper) || (func == unifuzz_title)) {
                        c = (u32) 'S';
                        WRITE_UTF8(q, c)
                    }
                    break;
                case 0x1E9E :         // uppercase 'ß'  -->  'ss'
                    if (func == unifuzz_lower) {
                        c = (u32) 's';
                        WRITE_UTF8(q, c)
                    }
                    break;
                default :
                    c = func(c);
                    if (outbytesleft < 4) {
                        k = q - z2;
                        outalloc += UNIFUZZ_CHUNK;
                        outbytesleft += UNIFUZZ_CHUNK;
                        z2 = (u8 *) contextRealloc(context, z2, outalloc + 1);
                        if (z2 == 0) return;
                        q = z2 + k;
                    }
            }
            WRITE_UTF8(q, c)
        }
        *q = 0;
        sqlite3_result_text(context, (char *) z2, q - z2, sqlite3_free);
    }
}


/*
** Implementation of the PROPER() SQL function.
** This function case applies TITLE() to the first TITLE-isable
** characer, then LOWER() up to the end of the current "word"
** and repeats to end of string.
*/
SQLITE_PRIVATE void properFunc8(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    const u8 *z1, *p, *term;
    u8 *z2, *q;
    int k, n, outalloc, outbytesleft, head;
    u32 c, up, lo;
    UNUSED_PARAMETER(argc);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
        sqlite3_result_null(context);
        return;
    }
    z1 = (u8 *) sqlite3_value_text(argv[0]);
    n = sqlite3_value_bytes(argv[0]);
    term = (u8 *) z1 + n;
    outalloc = outbytesleft = n + UNIFUZZ_CHUNK;
    z2 = contextMalloc(context, outalloc + 1);
    if (z2) {
        p = (u8 *) z1;
        q = z2;
        head = 1;
        while (*p) {
            READ_UTF8(p, term, c)
            up = unifuzz_title(c);
            lo = unifuzz_lower(c);
            if (up != lo) {                 // crude criterion for discrimating letters candidate for title case at head of word
                if (head) {
                    c = up;                 // 'ß' can't normally appear at the head of a word. If it does, it is written verbatim.
                    head = 0;
                } else {
                    c = lo;
                }
            } else {
                // Here again, we can safely stuff 'ss' for each uppercase 'ß' we encounter (if ever!)
                if (c == 0x1E9E) {          // uppercase 'ß'  -->  'ss'
                    c = (u32) 's';
                    WRITE_UTF8(q, c)
                } else {
                    c = lo;
                }
                switch (c) {
                    case                    // put here a list of letters codepoints for which up == lo
                            0x00DF :        // 'ß' no uppercase form in practice
                        break;
                    default :
                        head = 1;           // a non letter has been encountered: rearm head
                }
                if (outbytesleft < 4) {
                    k = q - z2;
                    outalloc += UNIFUZZ_CHUNK;
                    outbytesleft += UNIFUZZ_CHUNK;
                    z2 = (u8 *) contextRealloc(context, z2, outalloc + 1);
                    if (z2 == 0) return;
                    q = z2 + k;
                }
            }
            WRITE_UTF8(q, c)
        }
        *q = 0;
        sqlite3_result_text(context, (char *) z2, q - z2, sqlite3_free);
    }
}


/*
** Implementation of the HEXW() SQL function.
** This function returns the hexadecimal text representation of its numeric argument.
*/
SQLITE_PRIVATE void hexwFunc8(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    u64 z1;
    u8 *buf, *q;
    int i;
    UNUSED_PARAMETER(argc);
    switch (sqlite3_value_type(argv[0])) {
        case SQLITE_INTEGER :
        case SQLITE_FLOAT :
        case SQLITE_TEXT :
            z1 = (u64) sqlite3_value_int64(argv[0]);
            buf = q = (u8 *) contextMalloc(context, (2 * sizeof(i64)) + 1);
            for (i = 60; i >= 0; i -= 4) {
                *q++ = hexdigits[(z1 >> i) & (u64) 0x0FLL];
            }
            *q = 0;
            sqlite3_result_text(context, (const char *)buf, 2 * sizeof(i64), sqlite3_free);
            break;
        case SQLITE_BLOB :
        case SQLITE_NULL :
            sqlite3_result_null(context);
            return;
    }
}


/*
** Implementation of the ASCW(text [, position]) SQL function.
** This function returns the Unicode codepoint numeric value of the character
** at "position" in "text". Positions starts at 1.  Negative values of position
** are allowed and designate positions from the end of text, last character of
** text being -1.
** If the second parameter is omitted, then position default to 1.
** Position values outside the defined character indexes yield a null result
*/
SQLITE_PRIVATE void ascwFunc8(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    const u8 *z1, *p;
    u8 c8;
    int n, skip;
    u32 c;
    sqlite3_result_null(context);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
        return;
    }
    p = z1 = (u8 *) sqlite3_value_text(argv[0]);
    n = sqlite3_value_bytes(argv[0]);
    if (n == 0) {
        sqlite3_result_int(context, 0);
    } else {
        if (argc == 2) {
            skip = sqlite3_value_int(argv[1]);
        } else {
            skip = 1;
        }
        if (skip > 0) {
            for (p = z1; (p < z1 + n) && (skip > 0); skip--) {
                READ_UTF8(p, z1 + n, c);
            }
            if (skip > 0) {
//                sqlite3_result_null(context);
                return;
            }
        } else if (skip < 0) {
            // reading UTF-8 backwards like this relies on well-formed Unicode strings
            for (p = z1 + n; (p > z1) && (skip < 0); skip++) {
                c8 = *--p;
                if (c8 & 0x80) {
                    while (((*--p) & 0xC0) != 0xC0) {
                        ;
                    }
                }
            }
            if (skip < 0) {
//                sqlite3_result_null(context);
                return;
            }
            READ_UTF8(p, z1 + n, c);
        } else {
//            sqlite3_result_null(context);
            return;
        }
        sqlite3_result_int(context, c);
    }
}



/*
** Implementation of the CHRW() SQL function.
** This function returns a string containing the Unicode character
** corresponding to its numeric argument in decimal or hex form.
*/
SQLITE_PRIVATE void chrwFunc8(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    u8 *buf, *p, *q;
    int n;
    u32 c;
    UNUSED_PARAMETER(argc);
    // initialize c to suppress warning
    c = 0;
    switch (sqlite3_value_type(argv[0])) {
        case SQLITE_INTEGER :
        case SQLITE_FLOAT :
        case SQLITE_TEXT :
            buf = p = contextMalloc(context, 6);
            if (p) {
                c = (u32) sqlite3_value_int(argv[0]);
            } else {
                return;
            }
            break;
        case SQLITE_BLOB :
            q = (u8 *) sqlite3_value_blob(argv[0]);
            n = sqlite3_value_bytes(argv[0]);
            if (n == 0) {
                sqlite3_result_null(context);
            } else {
                buf = p = contextMalloc(context, 6);
                if (p) {
                    n = min(n, 3);
                    for (c = 0; n--; ) {
                        c = (c << 8) + *q++;
                    }
                } else {
                    return;
                }
            }
            break;
        case SQLITE_NULL :
            sqlite3_result_null(context);
            return;
    }
    // refuse to let invalid Unicode code points in
    if (    ((c >= 0xD800) && (c <= 0xDFFF)) ||
            ((c >= 0xFDD0) && (c <= 0xFDEF)) ||
            ((c & 0xFFFE) == 0xFFFFE) ||
            (c > 0x10FFFF)                      ) {
        c = 0xFFFD;
    } else {
        WRITE_UTF8(p, c)
        *p = 0;
        sqlite3_result_text(context, (const char *)buf, p - buf, sqlite3_free);
    }
}


SQLITE_PRIVATE void strposFunc8(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
	const u8 *z;
	const u8 *z1;
	const u8 *z2;
	int len;
	int len1;
	// u64 instnum;
	i64 instnum;
	int pass;

//	assert((argc == 2) || (argc == 3));
    sqlite3_result_null(context);
	if ((sqlite3_value_type(argv[1]) == SQLITE_NULL) || ((argc == 3) && (sqlite3_value_type(argv[2]) != SQLITE_INTEGER))) {
		return;
	}

	z = (u8 *) sqlite3_value_text(argv[0]);
	if (z == 0) {
		return;
	}
	len = sqlite3_value_bytes(argv[0]);

	z1 = (u8 *) sqlite3_value_text(argv[1]);
	if (z1 == 0) {
		return;
	}
	len1 = sqlite3_value_bytes(argv[1]);
	if (len1 > len) len1 = len;

	if (argc == 3) {
		instnum = sqlite3_value_int64(argv[2]);
		if (instnum == 0) {
			instnum = 1;
		}
	} else {
		instnum = 1;
	}

	if (instnum < 0) {
		pass = -1;
		z2 = z + len - len1;
	} else {
		pass = 1;
		z2 = z;
	}

	while ((z2 >= z) && ((z2 + len1) <= (z + len))) {
		if (memcmp(z2, z1, len1) == 0) {
			instnum -= pass;
			if (instnum == 0) break;
		}
		z2 += pass;
	}

	u64 pos = 0;
	if (instnum == 0) {
		// count the utf-8 chars until here
		u8 c;
		while (z <= z2) {
			c = *z++;
			if (c >= 0xC0) {
				while ((z < z2) && ((*z & 0xC0) == 0x80)) {
					z++;
				}
			}
			++pos;
		}
	}
	sqlite3_result_int64(context, pos);
}


/*
** Given a string (s) in the first argument and a non-negative integer (n)
** in the second returns a string that contains s contatenated n times.
*/
SQLITE_PRIVATE void xeroxFunc8(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
){
	const u8 *z1;
	u8 *z2;
	int iCount, nLen, nTLen, i;

    sqlite3_result_null(context);
	if ((argc != 2) || (sqlite3_value_type(argv[0]) == SQLITE_NULL)) {
//        sqlite3_result_null(context);
		return;
	}

	iCount = sqlite3_value_int(argv[1]);
	if (iCount < 0) {
//        sqlite3_result_null(context);
	} else if (iCount == 0) {
		sqlite3_result_text(context, "", 0, SQLITE_TRANSIENT);
	} else {
		z1 = (u8 *) sqlite3_value_text(argv[0]);
		nLen  = sqlite3_value_bytes(argv[0]);
		nTLen = nLen * iCount;
		z2 = (u8 *) contextMalloc(context, nTLen + 1);
		if (z2 == 0) return;

		for (i = 0; i < iCount; ++i) {
			memcpy(z2 + i * nLen, z1, nLen);
		}
		sqlite3_result_text(context, (const char *)z2, nTLen, sqlite3_free);
	}
}


/*
** given 2 strings (s1, s2) returns the string s1 with the characters NOT in s2 removed
** assumes strings are UTF-8 encoded
*/
SQLITE_PRIVATE void strfilterFunc8(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
){
	const u8 *inp;        /* first parameter string (searched string) */
	int inplng;           /* input string length in bytes */
	const u8 *valid;      /* second parameter string (contains valid characters) */
	int validlng;
	const u8 *runinp, *runvalid;
	u8 *out;              /* output string */
	u8 *outp;
	u32 c1 = 0;
	u32 c2 = 0;

	assert(argc == 2);

	if ((sqlite3_value_type(argv[0]) == SQLITE_NULL) || (sqlite3_value_type(argv[1]) == SQLITE_NULL)) {
		sqlite3_result_null(context); 
	} else {
		inp = (u8 *) sqlite3_value_text(argv[0]);
		inplng = sqlite3_value_bytes(argv[0]);
		valid = (u8 *) sqlite3_value_text(argv[1]);
		validlng = sqlite3_value_bytes(argv[1]);
		if ((inplng == 0) || (validlng == 0)) {
			sqlite3_result_text(context, (const char *)inp, 0, SQLITE_TRANSIENT);
		} else {
			/* 
			** maybe we could allocate less, but that would imply 2 passes, rather waste 
			** (possibly) some memory
			*/
			out = (u8 *) contextMalloc(context, inplng); 
			if (out) {
				for (runinp = inp, outp = out; runinp < inp + inplng; ) {
					READ_UTF8(runinp, inp + inplng, c1)
					for (runvalid = valid; runvalid < valid + validlng; ) {
						READ_UTF8(runvalid, valid + validlng, c2)
						if (c1 == c2) {
							WRITE_UTF8(outp, c1)
						}
					}
				}
				sqlite3_result_text(context, (const char *)out, outp - out, sqlite3_free);
			}
		}
	}
}



/*
** given 2 strings (s1, s2) returns the string s1 with the characters IN s2 removed
** assumes strings are UTF-8 encoded
*/
SQLITE_PRIVATE void strtabooFunc8(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
){
	const u8 *inp;        /* first parameter string (searched string) */
	int inplng;           /* input string length in bytes */
	const u8 *taboo;      /* second parameter string (contains taboo characters) */
	int taboolng;
	int copy;
	const u8 *runinp, *runtaboo;
	u8 *out;              /* output string */
	u8 *outp;
	u32 c1 = 0;
	u32 c2 = 0;

	assert(argc == 2);

	if ((sqlite3_value_type(argv[0]) == SQLITE_NULL) || (sqlite3_value_type(argv[1]) == SQLITE_NULL)) {
		sqlite3_result_null(context); 
	} else {
		inp = (u8 *) sqlite3_value_text(argv[0]);
		inplng = sqlite3_value_bytes(argv[0]);
		taboo = (u8 *) sqlite3_value_text(argv[1]);
		taboolng = sqlite3_value_bytes(argv[1]);
		if ((inplng == 0) || (taboolng == 0)) {
			sqlite3_result_text(context, (const char *)inp, inplng, SQLITE_TRANSIENT);
		} else {
			/* 
			** maybe we could allocate less, but that would imply 2 passes, rather waste 
			** (possibly) some memory
			*/
			out = (u8 *) contextMalloc(context, inplng); 
			if (out) {
				for (runinp = inp, outp = out; runinp < inp + inplng; ) {
					READ_UTF8(runinp, inp + inplng, c1)
					copy = 1;
					for (runtaboo = taboo; runtaboo < taboo + taboolng; ) {
						READ_UTF8(runtaboo, taboo + taboolng, c2)
						if (c1 == c2) {
							copy = 0;
						}
					}
					if (copy) {
						WRITE_UTF8(outp, c1)
					}
				}
				sqlite3_result_text(context, (const char *)out, outp - out, sqlite3_free);
			}
		}
	}
}



SQLITE_PRIVATE void typosFunc8(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    u8 *st1, *st2;
    int i, j, len1, len2, l1, l2;
    u32 *s1, *s2;
    typedef int cell;
    cell *r, *r_0, *r_1, *r_2, cost;
    UNUSED_PARAMETER(argc);
    if ((sqlite3_value_type(argv[0]) == SQLITE_NULL) || (sqlite3_value_type(argv[1]) == SQLITE_NULL)) {
        sqlite3_result_null(context);
        return;
    }
    st1 = (u8 *) sqlite3_value_text(argv[0]);
    len1 = sqlite3_value_bytes(argv[0]) / sizeof(u8);
    st2 = (u8 *) sqlite3_value_text(argv[1]);
    len2 = sqlite3_value_bytes(argv[1]) / sizeof(u8);
    s1 = unifuzz_utf8_unacc_utf32(context, st1, len1, &l1, 1);              // fold
    if (s1 != 0) {
        s2 = unifuzz_utf8_unacc_utf32(context, st2, len2, &l2, 1);          // fold
        if (s2 != 0) {
            if (l2 != 0) {
                for (i = 0; i < l2; i++) {
                    if ((s2[i] == '_') && (i < l1)) {
                        s2[i] = s1[i];
                    } else if (s2[i] == '%') {
                        l2 = i;     // pattern chars before %
                        if (l1 > l2) {
                            l1 = l2;
                        }
                        break;
                    }
                }
            }
            if (l2 == 0) {
                sqlite3_result_int(context, l1);
            } else {
                if (l1 == 0) {
                    sqlite3_result_int(context, l2);
                } else {
                    if (l1 * l2 > UNIFUZZ_TYPOS_LIMIT) {
						sqlite3_result_error(context, "Arguments of TYPOS exceed limit.", -1);
					} else {
		                r = (cell *) contextMalloc(context, 3 * (l2 + 1) * sizeof(cell));
	                    if (r) {
   		                    r_0 = r;
       		                r_1 = r_0 + l2 + 1;
           		            r_2 = r_1 + l2 + 1;
               		        for (i = 0; i <= l2; i++) {
                   		        r_0[i] = i;
                       		}
                       		for (i = 1; i <= l1; i++) {
                           		cell *tmp = r_2;
                           		r_2 = r_1;
                           		r_1 = r_0;
                           		r_0 = tmp;
                           		r_0[0] = i;
                           		for (j = 1; j <= l2; j++) {
	                                cost = ((s1[i - 1] != s2[j - 1]) ? 1 : 0);
	                                r_0[j] = min(r_1[j] + 1, min(r_0[j - 1] + 1, r_1[j - 1] + cost));   // ins, del, chg
	                                if (cost && (i > 1) && (j > 1) && (s1[i - 1] == s2[j - 2]) && (s1[i - 2] == s2[j - 1])) {
	                                    r_0[j] = min(r_0[j], r_1[j - 1]);   // Xpos
	                                }
	                            }
   		                    }
       		                sqlite3_result_int(context, (int) r_0[l2]);
           		            sqlite3_free(r);
						}
                    }
                }
            }
            sqlite3_free(s2);
        }
        sqlite3_free(s1);
    }
}
#endif  // UNIFUZZ_UTF8 || UNIFUZZ_UTF_BOTH




/*
**==========================================================================================================
**==========================================================================================================
**
**          UTF-16 section
**
**==========================================================================================================
**==========================================================================================================
*/

/*
** Macros for reading / skipping / writing one UTF-16 encoded Unicode character
*/
#define READ_UTF16(zIn, zTerm, c) {                                                     \
    c = (*zIn++);                                                                       \
    if ((c >= 0xD800) && (c <= 0xDCFF)) {                                               \
        if (zIn < zTerm) {                                                              \
            u16 c2 = (*zIn++);                                                          \
            c = (c2 & 0x3FF) + ((c & 0x3F) << 10) + (((c & 0x3C0) + 0x40) << 10);       \
        } else {                                                                        \
            c = 0xFFFD;                                                                 \
        }                                                                               \
    }                                                                                   \
}


#define WRITE_UTF16(zOut, c) {                                                          \
    if (c <= 0xFFFF) {                                                                  \
        *zOut++ = (u16) c;                                                              \
    } else {                                                                            \
        *zOut++ = (u16) (0xD800 + (((c >> 10) & 0x7C0) - 0x40) + ((c >> 10) & 0x3F));   \
        *zOut++ = (u16) (0xDC00 + (c & 0x3FF));                                         \
    }                                                                                   \
}


/*
**==========================================================================================================
**
**          UTF-16 version of scalars functions: mandatory even when UTF_8 only is required
**
**          This trick permits loading of the whole extension from SQL without error.
**
**==========================================================================================================
*/

/*
** u16 *unifuzz_utf16_unacc_utf16(sqlite3_context *context, u16 *inStr, int inBytes, int *outBytes)
**
** conversion of an UTF-16 input string of nBytes bytes into
** a unaccented zero-terminated UTF-16 string
** the character length of the output string is updated
*/
SQLITE_PRIVATE u16 *unifuzz_utf16_unacc_utf16(
    sqlite3_context *context,
    u16 *inStr,
    int inBytes,
    int *outBytes
){
    int l, k, outalloc, outU16left;
    u32 c, *uac;
    u16 *p, *term, *outStr, *q, *q0;

    // UTF-16 should be no longer than UTF-16, but unaccent can need more
    outalloc = outU16left = inBytes + inBytes % 4 + UNIFUZZ_CHUNK;
    outStr = (u16 *) contextMalloc(context, outalloc + sizeof(u16));
    if (outStr == 0) return 0;
    term = inStr + inBytes / sizeof(u16);
    q = outStr;
    if (inBytes) {
        for (p = inStr; p < term; ) {
            READ_UTF16(p, term, c)
            c = unifuzz_unacc(c, &uac, &l);
            q0 = q;
            if (l > 0) {
                // each char from unaccent _could_ need 2 u16 to code in UTF-16
                // we allocate as per worst case and might end up with few more bytes than strictly necessary
                while (outU16left < (int) (l * 2 * sizeof(u16))) {
                    k = q - outStr;
                    outalloc += UNIFUZZ_CHUNK;
                    outU16left += UNIFUZZ_CHUNK;
                    outStr = (u16 *) contextRealloc(context, outStr, outalloc + sizeof(u16));
                    if (outStr == 0) return 0;
                    q = outStr + k;
                    q0 = q;
                }
                for (k = 0; k < l; k++, uac++) {
                    WRITE_UTF16(q, *uac)       // we do not know how many u16 will be written
                }
            } else {
                WRITE_UTF16(q, c)
            }
            outU16left -= q - q0;            // now, we know exactly
        }
    }
    *q = 0;
    *outBytes = (q - outStr) * sizeof(u16);
    return(outStr);
}


/*
** u32 *unifuzz_utf16_unacc_utf32(sqlite3_context *context, u16 *inStr, int inBytes, int *outChars, int fold)
**
** conversion of an UTF-16 input string of nBytes bytes into
** a folded (?) unaccented zero-terminated UTF-32 string
** the character length of the output string is updated
*/
SQLITE_PRIVATE u32 *unifuzz_utf16_unacc_utf32(
    sqlite3_context *context,
    u16 *inStr,
    int inBytes,
    int *outChars,
    int fold
){
    int l, k, outalloc, used;
    u16 *p, *term;
    u32 c, *uac, *outStr, *q;

    // UTF-32 will be 4 times as long than character-wise input, but unaccent can need more
    //
    // Since we don't know exactly what will be the actual length needed (either in Unicode
    // characters or in bytes) we initially allocate as many u32 for output as inBytes.
    // We may end up having allocated too few, (then we realloc if needed) or too much by
    // a small margin (in the non degenerate case).  Anyway, the result is typically used
    // at once and then freed, so allocating a few more u32 than strictly necessary should
    // be harmless in normal use: you are not supposed to launch LIKE or TYPOS on megabytes
    // strings.
    outalloc = ((inBytes + 1) / sizeof(u16)) * sizeof(u32) + UNIFUZZ_CHUNK;
    outStr = (u32 *) contextMalloc(context, outalloc + sizeof(u32));
    if (outStr == 0) return 0;
    term = inStr + inBytes / sizeof(u16);
    q = outStr;
    used = 0;
    if (inBytes) {
        for (p = inStr; p < term; ) {
            READ_UTF16(p, term, c)
            if (fold) {
                c = unifuzz_fold_unacc(c, &uac, &l);
            } else {
                c = unifuzz_unacc(c, &uac, &l);
            }
            if (l > 0) {
                if (l > 1) {
                    while ((int) (outalloc - used * sizeof(u32)) < l) {
                        outalloc += UNIFUZZ_CHUNK;
                        outStr = (u32 *) contextRealloc(context, outStr, outalloc + sizeof(u32));
                        if (outStr == 0) return 0;
                        q = outStr + used;
                    }
                }
                for (k = 0; k < l; k++) {
                    *q++ = *uac++;
                    used++;
                }
            } else {
                *q++ = c;
                used++;
            }
        }
    }
    *q = 0;
    *outChars = used;
    return(outStr);
}


/*
** Implementation of the like() SQL function.  This function implements
** the build-in LIKE operator.  The first argument to the function is the
** pattern and the second argument is the string.  So, the SQL statements:
**
**       A LIKE B
**
** is implemented as like(B,A).
**
** This same function (with a different compareInfo structure) computes
** the GLOB operator.
*/
SQLITE_PRIVATE void likeFunc16(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    const u16 *zA, *zB;
    u32 escape = 0, *s1, *s2;
    int l1, l2, ll;
    compareInfoT *pInfo;
    if ((SQLITE_NULL == sqlite3_value_type(argv[0])) || (SQLITE_NULL == sqlite3_value_type(argv[1]))) {
        sqlite3_result_null(context);
        return;
    }
    zB = sqlite3_value_text16(argv[0]);
    l2 = sqlite3_value_bytes16(argv[0]);
    zA = sqlite3_value_text16(argv[1]);
    l1 = sqlite3_value_bytes16(argv[1]);
    /* Limit the length of the LIKE or GLOB pattern to avoid problems
    ** of deep recursion and N*N behavior in patternCompare().
    */
    if (l2 > SQLITE_MAX_LIKE_PATTERN_LENGTH) {
        sqlite3_result_error(context, "LIKE or GLOB pattern too complex", -1);
        return;
    }
    if (argc == 3) {
        /* The escape character string must consist of a single UTF-8 character.
        ** Otherwise, return an error.
        */
        const u16 *zEsc = sqlite3_value_text16(argv[2]);
        if (zEsc == 0) return;
        READ_UTF16(zEsc, zEsc + 4, escape); // allow for reading more than one byte
        if (*zEsc != 0) {
            sqlite3_result_error(context, "ESCAPE expression must be a single character", -1);
            return;
        }
    }
    pInfo = sqlite3_user_data(context);
    s1 = unifuzz_utf16_unacc_utf32(context, (u16 *) zA, l1, &ll, pInfo->noCase);        // conditional fold
    if (s1 != 0) {
        s2 = (u32 *) sqlite3_get_auxdata(context, 0);                                   // try to recover last value used, if any
        if (s2 == 0) {
            s2 = unifuzz_utf16_unacc_utf32(context, (u16 *) zB, l2, &ll, pInfo->noCase);// conditional fold
            sqlite3_set_auxdata(context, 0, s2, sqlite3_free);                          // save data for next call, if any
        }
        if (s2 != 0) {
            sqlite3_result_int(context, patternCompare(context, s2, s1, pInfo, escape));
        }
        sqlite3_free(s1);
    }
}


/*
** Implementation of the UNACCENT() SQL function.
** This function folds each character in the supplied string
** and strips any accents present in the string.
**
** This function may result to a longer output string compared
** to the original input string. Memory has been properly reallocated
** to accomodate for the extra memory length required.
*/
SQLITE_PRIVATE void unaccFunc16(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    const u16 *z1;
    u16 *z2;
    int n, l;
    UNUSED_PARAMETER(argc);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
        sqlite3_result_null(context);
        return;
    }
    z1 = (u16 *) sqlite3_value_text16(argv[0]);
    n = sqlite3_value_bytes16(argv[0]);
    z2 = unifuzz_utf16_unacc_utf16(context, (u16 *) z1, n, &l);
    if (z2 != 0) {
        sqlite3_result_text16(context, z2, l, sqlite3_free);
    }
}


/*
** Implementation of the FOLD(), LOWER(), UPPER(), TITLE()
** scalar SQL functions.
**
** The conversion to be made depends on (sqlite3_context *)context
** where a pointer to a specific case conversion function is stored.
*/
SQLITE_PRIVATE void caseFunc16(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    const u16 *z1, *p, *term;
    u16 *z2, *q;
    int k, n, outalloc, outU16left;
    u32 c;
    typedef u32 (*PFN_CASEFUNC)(u32);
    PFN_CASEFUNC func;
    UNUSED_PARAMETER(argc);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
        sqlite3_result_null(context);
        return;
    }
    z1 = (u16 *) sqlite3_value_text16(argv[0]);
    n = sqlite3_value_bytes16(argv[0]);
    term = (u16 *) z1 + (n / sizeof(u16));
    outalloc = outU16left = n + UNIFUZZ_CHUNK;
    z2 = contextMalloc(context, outalloc + sizeof(u16));
    if (z2) {
        func = (PFN_CASEFUNC) sqlite3_user_data(context);
        for (p = (u16 *) z1, q = z2; *p; ) {
            READ_UTF16(p, term, c)
            // Special quick & dirty hack for German eszet (lower- and upper-case).
            // Unlike UTF-8, UTF-16 encodings for those characters need one more position.
            switch (c) {
                case 0x00DF :         // 'ß'  ->  'SS'
                    if ((func == unifuzz_upper) || (func == unifuzz_title)) {
                        c = (u32) 'S';
                        WRITE_UTF16(q, c)
                    }
                    break;
                case 0x1E9E :         // uppercase 'ß'  -->  'ss'
                    if (func == unifuzz_lower) {
                        c = (u32) 's';
                        WRITE_UTF16(q, c)
                    }
                    break;
                default :
                    c = func(c);
                    if (outU16left < 4) {
                        k = q - z2;
                        outalloc += UNIFUZZ_CHUNK;
                        outU16left += UNIFUZZ_CHUNK;
                        z2 = (u16 *) contextRealloc(context, z2, outalloc + sizeof(u16));
                        if (z2 == 0) return;
                        q = z2 + k;
                    }
            }
            WRITE_UTF16(q, c)
        }
        *q = 0;
        sqlite3_result_text16(context, z2, (q - z2) * sizeof(u16), sqlite3_free);
    }
}


/*
**==========================================================================================================
**
**          UTF-16 version of scalars functions: optional functions for UTF_16 only or UTF_BOTH
**
**==========================================================================================================
*/


#if defined(UNIFUZZ_UTF16) || defined(UNIFUZZ_UTF_BOTH)

/*
** Implementation of the FLIP() SQL function.
** This function is essentially a Unicode strrev(): it returns the
** Unicode character by Unicode Character reverse of its input.
*/
SQLITE_PRIVATE void flipFunc16(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    const u16 *z1, *p, *p0;
    u16 *z2, *q;
    int n, l;
    u32 c;
    UNUSED_PARAMETER(argc);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
        sqlite3_result_null(context);
        return;
    }
    z1 = (u16 *) sqlite3_value_text16(argv[0]);
    n = sqlite3_value_bytes16(argv[0]) / sizeof(u16);
    z2 = contextMalloc(context, (n + 1) * sizeof(u16));
    if (z2) {
        for (p = (u16 *) z1, q = z2 + n; *p; ) {
            p0 = p;
            READ_UTF16(p, z1 + n, c)
            l = p - p0;
            q -= l;
            WRITE_UTF16(q, c)
            q -= l;
        }
        q[n] = 0;
        sqlite3_result_text16(context, z2, n * sizeof(u16), sqlite3_free);
    }
}


/*
** Implementation of the PROPER() SQL function.
** This function case does TITLE() to the first TITLE-isable
** characer, then LOWER() up to the end of the current "word"
** and repeats to end of string.
*/
SQLITE_PRIVATE void properFunc16(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    const u16 *z1, *p, *term;
    u16 *z2, *q;
    int k, n, outalloc, outU16left, head;
    u32 c, up, lo;
    UNUSED_PARAMETER(argc);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
        sqlite3_result_null(context);
        return;
    }
    z1 = (u16 *) sqlite3_value_text16(argv[0]);
    n = sqlite3_value_bytes16(argv[0]);
    term = (u16 *) z1 + (n / sizeof(u16));
    outalloc = outU16left = n + UNIFUZZ_CHUNK;
    z2 = contextMalloc(context, outalloc + sizeof(u16));
    if (z2) {
        p = (u16 *) z1;
        q = z2;
        head = 1;
        while (*p) {
            READ_UTF16(p, term, c)
            up = unifuzz_title(c);
            lo = unifuzz_lower(c);
            if (up != lo) {                 // crude criterion for discrimating letters candidate for title case at head of word
                if (head) {
                    c = up;                 // 'ß' can't normally appear at the head of a word. If it does, it is written verbatim.
                    head = 0;
                } else {
                    c = lo;
                }
            } else {
                // Here again, we try to stuff 'ss' for each uppercase 'ß' we encounter (if ever!)
                if (c == 0x1E9E) {          // uppercase 'ß'  -->  'ss', but only if we have room for it
                    c = (u32) 's';
                    WRITE_UTF16(q, c)
                } else {
                    c = lo;
                }
                switch (c) {
                    case                    // put here a list of letters codepoints for which up == lo
                        0x00DF :           // 'ß' no uppercase form in practice
                        break;
                    default :
                        head = 1;           // a non letter has been encountered: rearm head
                }
                if (outU16left < 4) {
                    k = q - z2;
                    outalloc += UNIFUZZ_CHUNK;
                    outU16left += UNIFUZZ_CHUNK;
                    z2 = (u16 *) contextRealloc(context, z2, outalloc + sizeof(u16));
                    if (z2 == 0) return;
                    q = z2 + k;
                }
            }
            WRITE_UTF16(q, c)
        }
        *q = 0;
        sqlite3_result_text16(context, z2, (q - z2) * sizeof(u16), sqlite3_free);
    }
}


/*
** Implementation of the HEXW() SQL function.
** This function returns the hexadecimal text representation of its integral argument.
*/
SQLITE_PRIVATE void hexwFunc16(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    i64 z1;
    u16 *buf, *q;
    int i;
    UNUSED_PARAMETER(argc);
    switch (sqlite3_value_type(argv[0])) {
        case SQLITE_INTEGER :
        case SQLITE_FLOAT :
        case SQLITE_TEXT :
            z1 = (u64) sqlite3_value_int64(argv[0]);
            buf = q = (u16 *) contextMalloc(context, ((2 * sizeof(i64)) + 1) * sizeof(u16));
            for (i = 60; i >= 0; i -= 4) {
                *q++ = (u16) hexdigits[(z1 >> i) & (u64) 0x0FLL];
            }
            *q = 0;
            sqlite3_result_text16(context, buf, 2 * sizeof(i64) * sizeof(u16), sqlite3_free);
            break;
        case SQLITE_BLOB :
        case SQLITE_NULL :
            sqlite3_result_null(context);
            return;
    }
}


/*
** Implementation of the ASCW(text [, position]) SQL function.
** This function returns the Unicode codepoint numeric value of the character
** at "position" in "text". Positions starts at 1.  Negative values of position
** are allowed and designate positions from the end of text, last character of
** text being -1.
** If the second parameter is omitted, then position default to 1.
** Position values outside the defined character indexes yield a null result
*/
SQLITE_PRIVATE void ascwFunc16(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    const u16 *z1, *p;
    u16 c16;
    int n, skip;
    u32 c;
    sqlite3_result_null(context);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
//        sqlite3_result_null(context);
        return;
    }
    z1 = (u16 *) sqlite3_value_text16(argv[0]);
    n = sqlite3_value_bytes16(argv[0]) / sizeof(u16);
    if (n == 0) {
        sqlite3_result_int(context, 0);
    } else {
        if (argc == 2) {
            skip = sqlite3_value_int(argv[1]);
        } else {
            skip = 1;
        }
        if (skip > 0) {
            for (p = z1; (p < z1 + n) && (skip > 0); skip--) {
                READ_UTF16(p, z1 + n, c);
            }
            if (skip > 0) {
//                sqlite3_result_null(context);
                return;
            }
        } else if (skip < 0) {
            // reading UTF-16 backwards like this relies on well-formed UTF strings
            for (p = z1 + n; (p > z1) && (skip < 0); skip++) {
                c16 = *--p;
                if ((c16 & 0xFC00) == 0xDC00) {
                    --p;
                }
            }
            if (skip < 0) {
//                sqlite3_result_null(context);
                return;
            }
            READ_UTF16(p, z1 + n, c);
        } else {
//            sqlite3_result_null(context);
            return;
        }
        sqlite3_result_int(context, c);

    }
}


/*
** Implementation of the CHRW() SQL function.
** This function returns a string containing the Unicode character
** corresponding to its numeric argument in decimal or hex form.
*/
SQLITE_PRIVATE void chrwFunc16(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    u16 *buf, *p;
    u8 *q;
    int n;
    u32 c;
    UNUSED_PARAMETER(argc);
    // initialize c to suppress warning
    c = 0;
    switch (sqlite3_value_type(argv[0])) {
        case SQLITE_INTEGER :
        case SQLITE_FLOAT :
        case SQLITE_TEXT :
            buf = p = contextMalloc(context, 6);
            if (p) {
                c = (u32) sqlite3_value_int(argv[0]);
            } else {
                return;
            }
            break;
        case SQLITE_BLOB :
            q = (u8 *) sqlite3_value_blob(argv[0]);
            n = sqlite3_value_bytes(argv[0]);
            if (n == 0) {
                sqlite3_result_null(context);
            } else {
                buf = p = contextMalloc(context, 6);
                if (p) {
                    n = min(n, 3);
                    for (c = 0; n--; ) {
                        c = (c << 8) + *q++;
                    }
                } else {
                    return;
                }
            }
            break;
        case SQLITE_NULL :
            sqlite3_result_null(context);
            return;
    }
    // refuse to let invalid Unicode code points in
    if (    ((c >= 0xD800) && (c <= 0xDFFF)) ||
            ((c >= 0xFDD0) && (c <= 0xFDEF)) ||
            ((c & 0xFFFE) == 0xFFFFE) ||
            (c > 0x10FFFF)                      ) {
        c = 0xFFFD;
    } else {
        WRITE_UTF16(p, c)
        *p = 0;
        sqlite3_result_text16(context, buf, (p - buf) * sizeof(u16), sqlite3_free);
    }
}


SQLITE_PRIVATE void strposFunc16(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
	const u16 *z;
	const u16 *z1;
	const u16 *z2;
	int len;
	int len1;
	// u64 instnum;
	i64 instnum;
	int pass;

//	assert((argc == 2) || (argc == 3));
    sqlite3_result_null(context);
	if ((sqlite3_value_type(argv[1]) == SQLITE_NULL) || ((argc == 3) && (sqlite3_value_type(argv[2]) != SQLITE_INTEGER))) {
		return;
	}

	z = (u16 *) sqlite3_value_text16(argv[0]);
	if (z == 0) {
		return;
	}

	z1 = (u16 *) sqlite3_value_blob(argv[1]);
	if (z1 == 0) {
		return;
	}

	if (argc >= 3) {
		instnum = sqlite3_value_int64(argv[2]);
		if (instnum == 0) {
			instnum = 1;
		}
	} else {
		instnum = 1;
	}

	len = sqlite3_value_bytes(argv[0]);
	len1 = sqlite3_value_bytes(argv[1]);
	if (instnum < 0) {
		pass = -1;
		z2 = z + len - len1;
	} else {
		pass = 1;
		z2 = z;
	}

	while ((z2 >= z) && ((z2 + len1) <= (z + len))) {
		if (memcmp(z2, z1, len1) == 0) {
			instnum -= pass;
			if (instnum == 0) break;
		}
		z2 += pass;
	}

	u64 pos = 0;
	if (instnum == 0) {
		if (sqlite3_value_type(argv[0]) != SQLITE_BLOB) {
			// count the utf-16 chars until here
			u16 c;
			while (z <= z2) {
				c = *((u16 *) z);
				z += 2;
				if ((c >= 0xD800) && (c <= 0xDCFF)) {
					if (z <= z2) {
						z += 2;
					}
				}
				++pos;
			}
		} else {
			pos = (u64) ((z2 - z) + 1);
		}
	}
	sqlite3_result_int64(context, pos);
}


/*
** Given a string (s) in the first argument and a non-negative integer (n)
** in the second returns a string that contains s contatenated n times.
*/
SQLITE_PRIVATE void xeroxFunc16(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
){
	const u16 *z1;
	u16 *z2;
	int iCount, nLen, nTLen, i;

	if ((argc != 2) || (sqlite3_value_type(argv[0]) == SQLITE_NULL)) {
        sqlite3_result_null(context);
		return;
	}

	iCount = sqlite3_value_int(argv[1]);
	if (iCount < 0) {
        sqlite3_result_null(context);
	} else if (iCount == 0) {
		sqlite3_result_text16(context, "", 0, SQLITE_TRANSIENT);
	} else {
		z1 = (u16 *) sqlite3_value_text16(argv[0]);
		nLen  = sqlite3_value_bytes16(argv[0]);
		nTLen = nLen * iCount * sizeof(u16);
		z2 = (u16 *) contextMalloc(context, nTLen + sizeof(u16));
		if (z2 == 0) return;

		for (i = 0; i < iCount; ++i) {
			memcpy(z2 + i * nLen, z1, nLen);
		}
		sqlite3_result_text16(context, z2, nTLen, sqlite3_free);
	}
}


/*
** given 2 string (s1,s2) returns the string s1 with the characters NOT in s2 removed
** assumes strings are UTF-16 encoded
*/
SQLITE_PRIVATE void strfilterFunc16(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
){
	const u16 *inp;       /* first parameter string (searched string) */
	int inplng;           /* input string length in bytes */
	const u16 *valid;     /* second parameter string (vcontains valid characters) */
	int validlng;
	const u16 *runinp, *runvalid;
	u16 *out;              /* output string */
	u16 *outp;
	u32 c1 = 0;
	u32 c2 = 0;

	assert(argc == 2);

	if ((sqlite3_value_type(argv[0]) == SQLITE_NULL) || (sqlite3_value_type(argv[1]) == SQLITE_NULL)) {
		sqlite3_result_null(context); 
	} else {
		inp = (u16 *) sqlite3_value_text16(argv[0]);
		inplng = sqlite3_value_bytes16(argv[0]);
		valid = (u16 *) sqlite3_value_text16(argv[1]);
		validlng = sqlite3_value_bytes16(argv[1]);
		if ((inplng == 0) || (validlng == 0)) {
			sqlite3_result_text16(context, inp, 0, SQLITE_TRANSIENT);
		} else {
			/* 
			** maybe we could allocate less, but that would imply 2 passes, rather waste 
			** (possibly) some memory
			*/
			out = (u16 *) contextMalloc(context, inplng); 
			if (out) {
				for (runinp = inp, outp = out; runinp < inp + inplng; ) {
					READ_UTF16(runinp, inp + inplng, c1)
					for (runvalid = valid; runvalid < valid + validlng; ) {
						READ_UTF16(runvalid, valid + validlng, c2)
						if (c1 == c2) {
							WRITE_UTF16(outp, c1)
						}
					}
				}
				sqlite3_result_text16(context, out, (outp - out) * sizeof(u16), sqlite3_free);
			}
		}
	}
}



/*
** given 2 string (s1,s2) returns the string s1 with the characters IN s2 removed
** assumes strings are UTF-16 encoded
*/
SQLITE_PRIVATE void strtabooFunc16(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
){
	const u16 *inp;       /* first parameter string (searched string) */
	int inplng;           /* input string length in bytes */
	const u16 *taboo;     /* second parameter string (vcontains taboo characters) */
	int taboolng;
	int copy;
	const u16 *runinp, *runtaboo;
	u16 *out;             /* output string */
	u16 *outp;
	u32 c1 = 0;
	u32 c2 = 0;

	assert(argc == 2);

	if ((sqlite3_value_type(argv[0]) == SQLITE_NULL) || (sqlite3_value_type(argv[1]) == SQLITE_NULL)) {
		sqlite3_result_null(context); 
	} else {
		inp = (u16 *) sqlite3_value_text16(argv[0]);
		inplng = sqlite3_value_bytes16(argv[0]);
		taboo = (u16 *) sqlite3_value_text16(argv[1]);
		taboolng = sqlite3_value_bytes16(argv[1]);
		if ((inplng == 0) || (taboolng == 0)) {
			sqlite3_result_text16(context, inp, inplng, SQLITE_TRANSIENT);
		} else {
			/* 
			** maybe we could allocate less, but that would imply 2 passes, rather waste 
			** (possibly) some memory
			*/
			out = (u16 *) contextMalloc(context, inplng); 
			if (out) {
				for (runinp = inp, outp = out; runinp < inp + inplng; ) {
					READ_UTF16(runinp, inp + inplng, c1)
					copy = 1;
					for (runtaboo = taboo; runtaboo < taboo + taboolng; ) {
						READ_UTF16(runtaboo, taboo + taboolng, c2)
						if (c1 == c2) {
							copy = 0;
						}
					}
					if (copy) {
						WRITE_UTF16(outp, c1)
					}
				}
				sqlite3_result_text16(context, out, (outp - out) * sizeof(u16), sqlite3_free);
			}
		}
	}
}



SQLITE_PRIVATE void typosFunc16(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
){
    u16 *st1, *st2;
    int i, j;
    int len1, len2, l1, l2;
    u32 *s1, *s2;
    typedef int cell;
    cell *r, *r_0, *r_1, *r_2, cost;
    UNUSED_PARAMETER(argc);
    if ((sqlite3_value_type(argv[0]) == SQLITE_NULL) || (sqlite3_value_type(argv[1]) == SQLITE_NULL)) {
        sqlite3_result_null(context);
        return;
    }
    st1 = (u16 *) sqlite3_value_text16(argv[0]);
    len1 = sqlite3_value_bytes16(argv[0]) / sizeof(u16);
    st2 = (u16*) sqlite3_value_text16(argv[1]);
    len2 = sqlite3_value_bytes16(argv[1]) / sizeof(u16);
    s1 = unifuzz_utf16_unacc_utf32(context, st1, len1, &l1, 1);             // fold
    if (s1 != 0) {
        s2 = unifuzz_utf16_unacc_utf32(context, st2, len2, &l2, 1);         // fold
        if (s2 != 0) {
            if (l2 != 0) {
                for (i = 0; i < l2; i++) {
                    if ((s2[i] == '_') && (i < l1)) {
                        s2[i] = s1[i];
                    } else if (s2[i] == '%') {
                        l2 = i;     // pattern chars before %
                        if (l1 > l2) {
                            l1 = l2;
                        }
                        break;
                    }
                }
            }
            if (l2 == 0) {
                sqlite3_result_int(context, l1);
            } else {
                if (l1 == 0) {
                    sqlite3_result_int(context, l2);
                } else {
                    if (l1 * l2 > UNIFUZZ_TYPOS_LIMIT) {
				        sqlite3_result_error(context, "Arguments of TYPOS exceed limit.", -1);
					} else {
                   		r = (cell *) contextMalloc(context, 3 * (l2 + 1) * sizeof(cell));
                   		if (r) {
	                        r_0 = r;
	                        r_1 = r_0 + l2 + 1;
	                        r_2 = r_1 + l2 + 1;
	                        for (i = 0; i <= l2; i++) {
	                            r_0[i] = i;
	                        }
	                        for (i = 1; i <= l1; i++) {
	                            cell *tmp = r_2;
	                            r_2 = r_1;
	                            r_1 = r_0;
	                            r_0 = tmp;
	                            r_0[0] = i;
	                            for (j = 1; j <= l2; j++) {
	                                cost = ((s1[i - 1] != s2[j - 1]) ? 1 : 0);
	                                r_0[j] = min(r_1[j] + 1, min(r_0[j - 1] + 1, r_1[j - 1] + cost));   // ins, del, chg
	                                if (cost && (i > 1) && (j > 1) && (s1[i - 1] == s2[j - 2]) && (s1[i - 2] == s2[j - 1])) {
	                                    r_0[j] = min(r_0[j], r_1[j - 1]);   // Xpos
	                                }
	                            }
	                        }
	                        sqlite3_result_int(context, (int) r_0[l2]);
	                        sqlite3_free(r);
	                    }
                    }
                }
            }
            sqlite3_free(s2);
        }
        sqlite3_free(s1);
    }
}
#endif  // UNIFUZZ_UTF16 || UNIFUZZ_UTF_BOTH


/*
**==========================================================================================================
**
**          collation functions
**
**==========================================================================================================
*/


#ifndef NO_WINDOWS_COLLATION

/*
** Structure used to store the codepoint for the acceptable signs.
** The soft-hyphen has been included as well: its exact same appearance
** as the minus-hyphen makes it very hard to spot and it can get in with
** data collected from office programs quite easily.
** Again it's very unlikely that this could cause unexpected behavior (here!).
** The table must be sorted on increasing range start point.
*/

typedef struct sign_ {
    u32 codepoint;
    int sign;
} SignT;

SQLITE_PRIVATE SignT sign_table[] = {
    {0x00002B, +1},
    {0x00002D, -1},
    {0x0000AD, -1},
    {0x00207A, +1},
    {0x00207B, -1},
    {0x00208A, +1},
    {0x00208B, -1},
    {0x00FB29, +1},
    {0x00FE62, +1},
    {0x00FE63, -1},
    {0x00FF0B, +1},
    {0x00FF0D, -1},
    {0xFFFFFF,  0}  // end of table
};


/*
** Structure used to store the codepoint ranges for the acceptable digits.
** Only full 0..9 ranges are accepted for decimal systems only.
** The table must be sorted on increasing range start point.
*/

typedef struct numeric_ {
    u32 lo;
    u32 hi;
    int digit;		// start digit
} NumericT;

SQLITE_PRIVATE NumericT numeric_table[] = {
    {0x000030, 0x000039, 0},    // pure ASCII digits
    {0x0000B2, 0x0000B3, 2},    // SUPERSCRIPT 2 TO 3 \_____ how cleverly devised!
    {0x0000B9, 0X0000B9, 1},    // SUPERSCRIPT 1      /
    {0x000660, 0X000669, 0},    // ARABIC-INDIC DIGITS
    {0x0006F0, 0X0006F9, 0},    // EXTENDED ARABIC-INDIC DIGITS
    {0x0007C0, 0X0007C9, 0},    // NKO DIGITS
    {0x000966, 0X00096F, 0},    // DEVANAGARI DIGITS
    {0x0009E6, 0X0009EF, 0},    // BENGALI DIGITS
    {0x000A66, 0X000A6F, 0},    // GURMUKHI DIGITS
    {0x000AE6, 0X000AEF, 0},    // GUJARATI DIGITS
    {0x000B66, 0X000B6F, 0},    // ORIYA DIGITS
    {0x000BE6, 0X000BEF, 0},    // TAMIL DIGITS
    {0x000C66, 0X000C6F, 0},    // TELUGU DIGITS
    {0x000CE6, 0X000CEF, 0},    // KANNADA DIGITS
    {0x000D66, 0X000D6F, 0},    // MALAYALAM DIGITS
    {0x000E50, 0X000E59, 0},    // THAI DIGITS
    {0x000ED0, 0X000ED9, 0},    // LAO DIGITS
    {0x000F20, 0X000F29, 0},    // TIBETAN DIGITS
    {0x001040, 0X001049, 0},    // MYANMAR DIGITS
    {0x001090, 0X001099, 0},    // MYANMAR SHAN DIGITS
    {0x0017E0, 0X0017E9, 0},    // KHMER DIGITS
    {0x001810, 0X001819, 0},    // MONGOLIAN DIGITS
    {0x001946, 0X00194F, 0},    // LIMBU DIGITS
    {0x0019D0, 0X0019D9, 0},    // NEW TAI LUE DIGITS
    {0x001B50, 0X001B59, 0},    // BALINESE DIGITS
    {0x001BB0, 0X001BB9, 0},    // SUNDANESE DIGITS
    {0x001C40, 0X001C49, 0},    // LEPCHA DIGITS
    {0x001C50, 0X001C59, 0},    // OL CHIKI DIGITS
    {0x002070, 0X002070, 0},    // SUPERSCRIPT 0
    {0x002074, 0X002079, 4},    // SUPERSCRIPT 4 TO 9
    {0x002080, 0X002089, 0},    // SUBSCRIPT DIGITS
    {0x002460, 0X002468, 1},    // CIRCLED DIGITS 1 TO 9
    {0x0024EA, 0X0024EA, 0},    // CIRCLED DIGIT 0         another sign of careful, clever planning ...
    {0x00A620, 0X00A629, 0},    // VAI DIGITS
    {0x00A8D0, 0X00A8D9, 0},    // SAURASHTRA DIGITS
    {0x00A900, 0X00A909, 0},    // KAYAH LI DIGITS
    {0x00AA50, 0X00AA59, 0},    // CHAM DIGITS
    {0x00FF10, 0X00FF19, 0},    // FULLWIDTH DIGITS
    {0x0104A0, 0X0104A9, 0},    // OSMANYA DIGITS
    {0x01D7CE, 0X01D7D7, 0},    // MATHEMATICAL BOLD DIGITS
    {0x01D7D8, 0X01D7E1, 0},    // MATHEMATICAL DOUBLE-STRUCK DIGITS
    {0x01D7E2, 0X01D7EB, 0},    // MATHEMATICAL SANS-SERIF DIGITS
    {0x01D7EC, 0X01D7F5, 0},    // MATHEMATICAL SANS-SERIF BOLD DIGITS
    {0x01D7F6, 0X01D7FF, 0},    // MATHEMATICAL MONOSPACE DIGITS
    {0xFFFFFF, 0xFFFFFF, 0}     // end of table
};


/*
** str is a UTF-16 encoded unicode string. Returns the number of Unicode
** characters in the first nByte of str (or up to the first 0x0000, whichever
** comes first).
*/
SQLITE_PRIVATE int Utf16CharLen(const u16 *str, int nByte){
    int r = 0;
    u16 c;
    const u16 *zTerm;
    zTerm = &str[nByte / sizeof(u16)];
//assert(str <= zTerm);
    while ((*str != 0) && (str < zTerm)) {
        c = (*str++);
        if ((c >= 0xD800) && (c <= 0xDCFF)) {
            if (str < zTerm) {
                str++;
            }
        }
        r++;
    }
    return r;
}


/*
** The built-in collating sequence NOCASE is extended to accomodate the
** Unicode case folding mapping tables to normalize characters to their
** fold equivalents and test them for equality.
**
** This collation uses a Windows function, UTF-16LE is mandatory
*/

#ifndef LOCALE_INVARIANT
# define LOCALE_INVARIANT 0x007F
#endif

SQLITE_PRIVATE int nocase_collate(
    void *encoding,
    int nKey1,
    const void *pKey1,
    int nKey2,
    const void *pKey2
){
    int l1, l2;
    UNUSED_PARAMETER(encoding);
    l1 = Utf16CharLen((u16 *) pKey1, nKey1);
    l2 = Utf16CharLen((u16 *) pKey2, nKey2);

    return CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE | NORM_IGNOREWIDTH | NORM_IGNOREKANATYPE | SORT_STRINGSORT, (u16 *) pKey1, l1, (u16 *) pKey2, l2) - 2;
}


/*
** This collation uses a Windows function, UTF-16LE is mandatory
** similar to the above but ignores diacritics
*/

SQLITE_PRIVATE int unaccented_collate(
    void *encoding,
    int nKey1,
    const void *pKey1,
    int nKey2,
    const void *pKey2
){
    int l1, l2;
    UNUSED_PARAMETER(encoding);
    l1 = Utf16CharLen((u16 *) pKey1, nKey1);
    l2 = Utf16CharLen((u16 *) pKey2, nKey2);

    return CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE | NORM_IGNORENONSPACE | NORM_IGNOREWIDTH | NORM_IGNOREKANATYPE | SORT_STRINGSORT, (u16 *) pKey1, l1, (u16 *) pKey2, l2) - 2;
}


/*
** This collation uses a Windows function, UTF-16LE is mandatory
** similar to the above but also ignores hyphens and apostrophes, symbols and punctuation
*/

SQLITE_PRIVATE int names_collate(
    void *encoding,
    int nKey1,
    const void *pKey1,
    int nKey2,
    const void *pKey2
){
    int l1, l2;
    UNUSED_PARAMETER(encoding);
    l1 = Utf16CharLen((u16 *) pKey1, nKey1);
    l2 = Utf16CharLen((u16 *) pKey2, nKey2);

    return CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE | NORM_IGNORENONSPACE | NORM_IGNORESYMBOLS | NORM_IGNOREWIDTH | NORM_IGNOREKANATYPE, (u16 *) pKey1, l1, (u16 *) pKey2, l2) - 2;
}


/*
** UTF-16 version of _atoi64
** updates the running pointer p so that it points either to past the end of string
** or the first character after the numeric prefix
*/
SQLITE_PRIVATE i64 utf16_atoi64(
	const u16 **p,
	int len
){
    const u16 *q, *pTerm = &(((u16 *) *p)[len / sizeof(u16)]);
    i64 value = 0;
    u32 uc;
    int i, isnumber, lead0, n, sign = 1;
    if (len == 0) return 0;
    while (*p < pTerm) {    /* skip any whitespace */
        q = *p;
        READ_UTF16((*p), pTerm, uc);
        for (i = 0; i < sizeof(space_table) / sizeof(SpacingT); i++) {
            if (uc < space_table[i].lo) {
                *p = q;  // position back the pointer at the first non-whitespace character
                break;
            } else if (uc <= space_table[i].hi) {
                uc = 0;
                break;
            }
        }
        if (uc) break;  /* uc is not a whitespace */
    }
    if (*p < pTerm) {
        q = *p;
        READ_UTF16((*p), pTerm, uc);
        for (i = 0; i < sizeof(sign_table) / sizeof(SignT); i++) { /* is uc a minus sign? */
            if (uc < sign_table[i].codepoint) {
                *p = q;  // position back the pointer at the first non-sign character
                break;
            } else if (uc == sign_table[i].codepoint) {
                sign = sign_table[i].sign;
                break;
            }
        }
    }
    isnumber = 1;  /* read digits, if any */
    lead0 = 1;
    while (isnumber && (*p < pTerm)) {
        q = *p;
        READ_UTF16((*p), pTerm, uc);
        for (i = 0; i < sizeof(numeric_table) / sizeof(NumericT); i++) {
            if (uc < numeric_table[i].lo) {
                isnumber = 0;
                *p = q;  // position back the pointer at the first non-numeric character
                break;
            } else if (uc <= numeric_table[i].hi) {
                n = numeric_table[i].digit + (u8) (uc - numeric_table[i].lo);
                if (! ((n == '0') && (lead0))) {
                    value = 10 * value + n;     // do not check for overflow
                    lead0 = 0;
                }
                break;
            }
        }
    }
    return value * sign;
}


/*
** UTF-16 version of _atou64
** updates the running pointer p so that it points either to past the end of string
** or the first character after the numeric prefix
**
** This version only allows for digits (as per table above)
*/

// function for "work in progress" functions above

// SQLITE_PRIVATE i64 utf16_atou64(
// 	const u16 **p,
// 	int len
// ){
//     const u16 *q, *pTerm = &(((u16 *) *p)[len / sizeof(u16)]);
//     i64 value = 0;
//     u32 uc;
//     int i, isnumber, lead0, n;
//     if (len == 0) return 0;
//     isnumber = 1;  /* read digits, if any */
//     lead0 = 1;
//     while (isnumber && (*p < pTerm)) {
//         q = *p;
//         READ_UTF16((*p), pTerm, uc);
//         for (i = 0; i < sizeof(numeric_table) / sizeof(NumericT); i++) {
//             if (uc < numeric_table[i].lo) {
//                 isnumber = 0;
//                 *p = q;  // position back the pointer at the first non-numeric character
//                 break;
//             } else if (uc <= numeric_table[i].hi) {
//                 n = numeric_table[i].digit + (u8) (uc - numeric_table[i].lo);
//                 if (! ((n == '0') && (lead0))) {
//                     value = 10 * value + n;     // do not check for overflow
//                     lead0 = 0;
//                 }
//                 break;
//             }
//         }
//     }
//     return value;
// }


/*
** This collation performs a numeric sort on text data,
** where items are expected to have a variable-size numeric
** prefix optionally followed by a non-numeric part.
** Here "numeric characters" mean all Unicode codepoints for
** numerics having 0..9 equivalence.  This doesn't include
** roman numerals nor exotic or ancient digit representations
** which don't use the decimal positional system.
** Only decimal (digits, litterally) positional systems
** are dealt with (see table above).
** Whitespaces are allowed only before the numeric part (see table above).
** Numeric part can have up to one plus or minus sign (see table above).
** Items not starting by "spaces" then "sign" then "digits" have value 0.
** No attempt is made to check for 64-bit signed integer overflow.
**
** The optional rest of the strings are sorted by the NOCASE collation
** defined above.  For this we need the strings to be UTF-16LE encoded.
*/

SQLITE_PRIVATE int numerics_collate(
    void *encoding,
    int nKey1,
    const void *pKey1,
    int nKey2,
    const void *pKey2
){
    i64 n1, n2;
    const u16 *p1, *p2;
    if (pKey1 == 0) {
        n1 = 0;
    } else {
        p1 = (u16 *) pKey1;
        n1 = utf16_atoi64(&p1, nKey1);
    }
    if (pKey2 == 0) {
        n2 = 0;
    } else {
        p2 = (u16 *) pKey2;
        n2 = utf16_atoi64(&p2, nKey2);
    }
    if (n1 < n2) {
        return -1;
    } else if (n1 == n2) {
        return(nocase_collate(encoding, (&(((u8 *) pKey1)[nKey1]) - (u8 *) p1), p1, (&(((u8 *) pKey2)[nKey2]) - (u8 *) p2), p2));
    } else {
        return 1;
    }
}


/* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ work in progress!

SQLITE_PRIVATE int readnextdigitEx(
	const u16 **p,
	int len
){
    const u16 *q, *pTerm = &(((u16 *) *p)[len / sizeof(u16)]);
    u32 uc;
    int i, n = -1;
    q = *p;
    READ_UTF16((*p), pTerm, uc);
    for (i = 0; i < sizeof(numeric_table) / sizeof(NumericT); i++) {
        if (uc < numeric_table[i].lo) {
            *p = q;  // position back the pointer at the first non-numeric character
            break;
        } else if (uc <= numeric_table[i].hi) {
            n = numeric_table[i].digit + (u8) (uc - numeric_table[i].lo);
            break;
        }
    }
	return(n)
}


SQLITE_PRIVATE int naturalsort(
    void *encoding,
    int nKey1,
    const void *pKey1,
    int nKey2,
    const void *pKey2
){
	u16 *s1, *s2;
    i64 n1, n2;
	int n;
	
	n1 = readnextdigitEx(&pKey1, nKey1);
	if (n1 >= 0) {		// next chunk of key1 is numeric
    const u16 *p1, *p2;
    if (pKey1 == 0) {
        n1 = 0;
    } else {
        p1 = (u16 *) pKey1;
        n1 = utf16_atou64(&p1, nKey1);
    }
    if (pKey2 == 0) {
        n2 = 0;
    } else {
        p2 = (u16 *) pKey2;
        n2 = utf16_atou64(&p2, nKey2);
    }
    if (n1 < n2) {
        return -1;
    } else if (n1 == n2) {
        return(nocase_collate(encoding, (&(((u8 *) pKey1)[nKey1]) - (u8 *) p1), p1, (&(((u8 *) pKey2)[nKey2]) - (u8 *) p2), p2));
    } else {
        return 1;
    }

// u16 *unifuzz_utf16_unacc_utf16(sqlite3_context *context, u16 *inStr, int inBytes, int *outBytes)

	int ret = 0;
	u16 *z1l, *z1h, *z2l, *z2h;
	char *szOne, *szTwo;
	

	szOne = AU3_GetString(&p_AU3_Params[0]);
	szTwo = AU3_GetString(&p_AU3_Params[1]);
	CString strFirst = szOne;
	strFirst.MakeLower();
	CString strNext = szTwo;
	strNext.MakeLower();

	// Free temporary storage
	AU3_FreeString(szOne);
	AU3_FreeString(szTwo);

	// Allocate and build the return variable
	pMyResult = AU3_AllocVar();

	// The Alphanum Algorithm is an improved sorting algorithm for strings
	// containing numbers.  Instead of sorting numbers in ASCII order like
	// a standard sort, this algorithm sorts numbers in numeric order.
	//
	// The Alphanum Algorithm is discussed at http://www.DaveKoelle.com
	//
	// This code is a MFC port of the Perl script Alphanum.pl

	static const wchar_t szNumbers[] = L"0123456789";  

	while (n == 0) {        //Get next "chunk"
		//A chunk is either a group of digit(s) or a group of non-digit(s)
		CString strFirstChunk;
		CString strFirstChar( strFirst[0] );
		if (strFirstChar.FindOneOf( szNumbers ) != -1) {
			strFirstChunk = strFirst.SpanIncluding(szNumbers);
		} else {
			strFirstChunk = strFirst.SpanExcluding(szNumbers);
		}

		CString strNextChunk;
		strFirstChar = strNext[0];
		if (strFirstChar.FindOneOf( szNumbers ) != -1) {
			strNextChunk = strNext.SpanIncluding(szNumbers);
		} else {
			strNextChunk = strNext.SpanExcluding(szNumbers);
		}
		
		// exit loop if we've run out of chunks, ie strings are equal
		if (strFirstChunk == "" && strNextChunk == "")
			break;

		//Remove the extracted chunks from the strings
		strFirst = strFirst.Mid(strFirstChunk.GetLength(), strFirst.GetLength() - strFirstChunk.GetLength());
		strNext  = strNext.Mid(strNextChunk.GetLength(), strNext.GetLength() - strNextChunk.GetLength());

		// Now Compare the chunks
		//# Case 1: They both contain letters
		if (strFirstChunk.FindOneOf(szNumbers) == -1 && strNextChunk.FindOneOf(szNumbers)  == -1) {
			n = strFirstChunk.Compare( strNextChunk );
		} else {
			//Case 2: They both contain numbers
			if (strFirstChunk.FindOneOf(szNumbers) != -1 && strNextChunk.FindOneOf(szNumbers) != -1) {
				//Convert the numeric strings to long values
				long lFirst = _wtol( strFirstChunk );
				long lNext = _wtol( strNextChunk );

				n = 0;
				if ( lFirst > lNext ) {
					n = 1;
				} else if ( lFirst < lNext ) {
					n = -1;
				}
			} else {
				// Case 3: One has letters, one has numbers;
				// or one is empty
				n = strFirstChunk.Compare( strNextChunk );

				// If these are equal, make one (which one
				// is arbitrary) come before
				// the other   (or else we will be stuck
				// in this "while n==0" loop)
				if (n == 0) {
					n = 1;
				}
			}
		}
	}
	return ret;
}
$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */


#endif	// NO_WINDOWS_COLLATION





/*
**==========================================================================================================
**
**          library registration
**
**==========================================================================================================
*/

/*
** Standard C entry point _directly_ invoked from application code.
**
** Do NOT use it to load via an SQL statement, it won't work properly.  See notes below.
*/
#ifdef WIN32
DLL_EXPORT
#endif
int sqlite3_extension_init(
    sqlite3 *db,
    char **pzErrMsg,
    const sqlite3_api_routines *pApi
){
    struct FuncScalar {
        const char *zName;                        /* Function name */
        int nArg;                                 /* Number of arguments */
        int enc;                                  /* Optimal text encoding */
        void *pContext;                           /* sqlite3_user_data() context */
        void (*xFunc)(sqlite3_context*, int, sqlite3_value**); /* function */
        int allow_busy;                           /* 1 if SQLite may return SQLITE_BUSY when trying to override */
    } scalars[] = {
#if defined(UNIFUZZ_UTF8) || defined(UNIFUZZ_UTF_BOTH)
#ifdef UNIFUZZ_OVERRIDE_SCALARS
        {"upper",           1,  SQLITE_UTF8,     (void *) unifuzz_upper, caseFunc8       , 1},
        {"lower",           1,  SQLITE_UTF8,     (void *) unifuzz_lower, caseFunc8       , 1},
        {"like",            2,  SQLITE_UTF8,     (void *) &likeInfoNorm, likeFunc8       , 1},
        {"like",            3,  SQLITE_UTF8,     (void *) &likeInfoNorm, likeFunc8       , 1},
        {"glob",            2,  SQLITE_UTF8,         (void *) &globInfo, likeFunc8       , 1},
        /* include some UTF-16 functions as well to avoid problems when loading extension from SQL */
        {"upper",           1,  SQLITE_UTF16,    (void *) unifuzz_upper, caseFunc16      , 0},
        {"lower",           1,  SQLITE_UTF16,    (void *) unifuzz_lower, caseFunc16      , 0},
        {"like",            2,  SQLITE_UTF16,    (void *) &likeInfoNorm, likeFunc16      , 0},
        {"like",            3,  SQLITE_UTF16,    (void *) &likeInfoNorm, likeFunc16      , 0},
        {"glob",            2,  SQLITE_UTF16,        (void *) &globInfo, likeFunc16      , 0},
#else
        {"upperU",          1,  SQLITE_UTF8,     (void *) unifuzz_upper, caseFunc8       , 0},
        {"lowerU",          1,  SQLITE_UTF8,     (void *) unifuzz_lower, caseFunc8       , 0},
        {"likeU",           2,  SQLITE_UTF8,     (void *) &likeInfoNorm, likeFunc8       , 0},
        {"likeU",           3,  SQLITE_UTF8,     (void *) &likeInfoNorm, likeFunc8       , 0},
        {"globU",           2,  SQLITE_UTF8,         (void *) &globInfo, likeFunc8       , 0},
#endif
        {"title",           1,  SQLITE_UTF8,     (void *) unifuzz_title, caseFunc8       , 0},
        {"fold",            1,  SQLITE_UTF8,      (void *) unifuzz_fold, caseFunc8       , 0},
        {"flip",            1,  SQLITE_UTF8,                          0, flipFunc8       , 0},
        {"unaccent",        1,  SQLITE_UTF8,                          0, unaccFunc8      , 0},
        {"proper",          1,  SQLITE_UTF8,                          0, properFunc8     , 0},
        {"typos",           2,  SQLITE_UTF8,                          0, typosFunc8      , 0},
        {"ascw",            1,  SQLITE_UTF8,                          0, ascwFunc8       , 0},
        {"ascw",            2,  SQLITE_UTF8,                          0, ascwFunc8       , 0},
        {"chrw",            1,  SQLITE_UTF8,                          0, chrwFunc8       , 0},
        {"hexw",            1,  SQLITE_UTF8,                          0, hexwFunc8       , 0},
        {"strpos",          2,  SQLITE_UTF8,                          0, strposFunc8     , 0},
        {"strpos",          3,  SQLITE_UTF8,                          0, strposFunc8     , 0},
        {"strdup",          2,  SQLITE_UTF8,                          0, xeroxFunc8      , 0},
        {"strfilter",       2,  SQLITE_UTF8,                          0, strfilterFunc8  , 0},
        {"strtaboo",        2,  SQLITE_UTF8,                          0, strtabooFunc8   , 0},
#endif
#if defined(UNIFUZZ_UTF16) || defined(UNIFUZZ_UTF_BOTH)
#ifdef UNIFUZZ_OVERRIDE_SCALARS
#if ! defined(UNIFUZZ_UTF_BOTH)
        // don't register those functions twice if they are already in for UTF-8 case (OK, it's a  bit weird!)
        {"upper",           1,  SQLITE_UTF16,    (void *) unifuzz_upper, caseFunc16      , 0},
        {"lower",           1,  SQLITE_UTF16,    (void *) unifuzz_lower, caseFunc16      , 0},
        {"like",            2,  SQLITE_UTF16,    (void *) &likeInfoNorm, likeFunc16      , 0},
        {"like",            3,  SQLITE_UTF16,    (void *) &likeInfoNorm, likeFunc16      , 0},
        {"glob",            2,  SQLITE_UTF16,        (void *) &globInfo, likeFunc16      , 0},
#endif
#else
        {"upperU",          1,  SQLITE_UTF16,    (void *) unifuzz_upper, caseFunc16      , 0},
        {"lowerU",          1,  SQLITE_UTF16,    (void *) unifuzz_lower, caseFunc16      , 0},
        {"likeU",           2,  SQLITE_UTF16,    (void *) &likeInfoNorm, likeFunc16      , 0},
        {"likeU",           3,  SQLITE_UTF16,    (void *) &likeInfoNorm, likeFunc16      , 0},
        {"globU",           2,  SQLITE_UTF16,        (void *) &globInfo, likeFunc16      , 0},
#endif
        {"title",           1,  SQLITE_UTF16,    (void *) unifuzz_title, caseFunc16      , 0},
        {"fold",            1,  SQLITE_UTF16,     (void *) unifuzz_fold, caseFunc16      , 0},
        {"flip",            1,  SQLITE_UTF16,                         0, flipFunc16      , 0},
        {"unaccent",        1,  SQLITE_UTF16,                         0, unaccFunc16     , 0},
        {"proper",          1,  SQLITE_UTF16,                         0, properFunc16    , 0},
        {"typos",           2,  SQLITE_UTF16,                         0, typosFunc16     , 0},
        {"ascw",            1,  SQLITE_UTF16,                         0, ascwFunc16      , 0},
        {"ascw",            2,  SQLITE_UTF16,                         0, ascwFunc16      , 0},
        {"chrw",            1,  SQLITE_UTF16,                         0, chrwFunc16      , 0},
        {"hexw",            1,  SQLITE_UTF16,                         0, hexwFunc16      , 0},
        {"strpos",          2,  SQLITE_UTF16,                         0, strposFunc16    , 0},
        {"strpos",          3,  SQLITE_UTF16,                         0, strposFunc16    , 0},
        {"strdup",          2,  SQLITE_UTF16,                         0, xeroxFunc16     , 0},
        {"strfilter",       2,  SQLITE_UTF16,                         0, strfilterFunc16 , 0},
        {"strtaboo",        2,  SQLITE_UTF16,                         0, strtabooFunc16  , 0},
#endif
        {"printf",         -1,  SQLITE_ANY,                           0, printfFunc      , 0},
        {"unifuzz",         0,  SQLITE_ANY,                           0, versionFunc     , 0}
    };
    int i, rc = SQLITE_OK;
    SQLITE_EXTENSION_INIT2(pApi)
    UNUSED_PARAMETER(pzErrMsg);

    for(i = 0; ((i < (int) ((sizeof(scalars) / sizeof(struct FuncScalar)))) && (rc == SQLITE_OK)); i++){
        struct FuncScalar *p = &scalars[i];
        rc = sqlite3_create_function(db, p->zName, p->nArg, p->enc, p->pContext, p->xFunc, 0, 0);
        if ((rc == SQLITE_BUSY) && (p->allow_busy)) {
            rc = SQLITE_OK;
        }
    }

#ifndef NO_WINDOWS_COLLATION
    /* Also override the default NOCASE case-insensitive collation sequence. */
    // Warning: encoding UTF-16LE is mandatory
    if (rc == SQLITE_OK)
#ifdef UNIFUZZ_OVERRIDE_NOCASE
        rc = sqlite3_create_collation(db, "NOCASE",    SQLITE_UTF16LE, 0, nocase_collate);
        rc = sqlite3_create_collation(db, "RMNOCASE",  SQLITE_UTF16LE, 0, nocase_collate);	// special for Tom Holden!
#else
        rc = sqlite3_create_collation(db, "NOCASEU",   SQLITE_UTF16LE, 0, nocase_collate);
#endif
    if ((rc == SQLITE_OK) || (rc == SQLITE_BUSY))
        rc = sqlite3_create_collation(db, "NAMES",     SQLITE_UTF16LE, 0, names_collate);
    if (rc == SQLITE_OK)
        rc = sqlite3_create_collation(db, "UNACCENTED",SQLITE_UTF16LE, 0, unaccented_collate);
    if (rc == SQLITE_OK)
        rc = sqlite3_create_collation(db, "NUMERICS",  SQLITE_UTF16LE, 0, numerics_collate);
#endif		// NO_WINDOWS_COLLATION

assert(rc == 0);
    return rc;
}



#ifdef AUTOLOAD_KLUDGE

// WARNING: the code below introduces a dependancy on sqlite3.dll

// ============================================================
// Entry point for wrappers, drivers (like ODBC) or DB managers
// ============================================================
//
// This is a little backward to have to do this, but here we go.
//
// What is the problem?
//---------------------
//
//   Current SQLite has a design problem where, if you issue
//
//         SELECT load_extension('myfunctions.dll', ['MyEntryPoint']);
//
// then your dynamic (Windows) or shared (Un*x) library will NOT be able
// to register any native SQLite scalar function in the list:
//
//         UPPER(s)           for UTF-8
//         LOWER(s)           for UTF-8
//         LIKE(s1, s2)       for UTF-8
//         LIKE(s1, s2, esc)  for UTF-8
//         GLOB(s1, s2)       for UTF-8
//
// Be warned that this issue probably concerns every overridable internal
// function as well.
//
// Why is that?
//-------------
//
// The code of SQLite currently (3.6.20) doesn't allow any of those internal
// functions to be overriden by the loading of an extension IF A VM IS ACTIVE.
// This means that if a statement is currently executed, the registration will
// fail.
// Indeed, there is code to explicitely detect this and return SQLITE_BUSY
// without registering the offending function.
//
// The problem with this is that if you use a wrapper or a third-party DB
// manager, your only choice to load an extension is precisely to issue a
// SELECT load_extension() statement.
// Hence overriding any internal function (for the encoding already registered)
// is impossible when you don't call SQLite directly.
//
// This is a (strange in my view) design "feature".
//
// Is there a workaround?
//-----------------------
//
// Well, yes and no!  You can still manage to load your functions, but they
// won't be available for the connection you've already established, but only
// for newly opened bases afterwards.
//
// What we do is use another loading mecanism: auto_extension
//
//  First you issue a statement like this:
//
//        SELECT load_extension('myextension.dll', 'auto_load');
//
// auto_load() invokes sqlite3_auto_extension(), passing SQLite the entry
// point of your registration function, then returns OK.
//
// The next time your application opens a connection to _any_ SQLite base
// SQLite will automatically invoke your registration function for this new
// connection.  Since at this time there is no active SQL statement, your
// extension is welcome to override any function for any encoding.
//
// The drawback of this method is that your first connection is still unable
// to use your extension.  You can use a dummy base to perform the call and
// close it right after that.
//
// Another annoying consequence is that you won't have access to the native
// version of the overloaded functions anymore (except by the connection
// used to start the loading process), because autoload works for _all_
// _subsequent_ connections only.
//
//

#ifdef WIN32
DLL_EXPORT
#endif
int auto_load(
    sqlite3 *db,
    char **pzErrMsg,
    const sqlite3_api_routines *pApi
){
    UNUSED_PARAMETER(db);
    UNUSED_PARAMETER(pzErrMsg);
    UNUSED_PARAMETER(pApi);
    sqlite3_auto_extension((void*)sqlite3_extension_init);
    return SQLITE_OK;
}


#ifdef WIN32
DLL_EXPORT
#endif
int auto_unload(
    sqlite3 *db,
    char **pzErrMsg,
    const sqlite3_api_routines *pApi
){
    UNUSED_PARAMETER(db);
    UNUSED_PARAMETER(pzErrMsg);
    UNUSED_PARAMETER(pApi);
    sqlite3_reset_auto_extension();
    return SQLITE_OK;
}

#endif  // AUTOLOAD_KLUDGE



#if 0
#if ((defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BORLANDC__)) && (!defined(SQLITE_CORE)))
int __stdcall DllMain(void *hinstDLL, unsigned long fdwReason, void *lpReserved)
{
    UNUSED_PARAMETER(hinstDLL);
    UNUSED_PARAMETER(lpReserved);
    switch (fdwReason) {
        case 0 :            // DLL_PROCESS_DETACH
            break;
        case 1 :            // DLL_PROCESS_ATTACH
            return TRUE;
        case 2 :            // DLL_THREAD_ATTACH
            break;
        case 3 :            // DLL_THREAD_DETACH
            break;
    };
}
#endif
#endif // 0

#ifdef __cplusplus
}
#endif

