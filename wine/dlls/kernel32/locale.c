/*
 * Locale support
 *
 * Copyright 1995 Martin von Loewis
 * Copyright 1998 David Lee Lambert
 * Copyright 2000 Julio César Gázquez
 * Copyright 2002 Alexandre Julliard for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <locale.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#ifdef __APPLE__
# include <CoreFoundation/CFLocale.h>
# include <CoreFoundation/CFString.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winuser.h"  /* for RT_STRINGW */
#include "winternl.h"
#include "wine/unicode.h"
#include "winnls.h"
#include "winerror.h"
#include "winver.h"
#include "kernel_private.h"
#include "wine/debug.h"

// WINE_DEFAULT_DEBUG_CHANNEL(nls);

#define LOCALE_LOCALEINFOFLAGSMASK (LOCALE_NOUSEROVERRIDE|LOCALE_USE_CP_ACP|\
                                    LOCALE_RETURN_NUMBER|LOCALE_RETURN_GENITIVE_NAMES)
#define MB_FLAGSMASK (MB_PRECOMPOSED|MB_COMPOSITE|MB_USEGLYPHCHARS|MB_ERR_INVALID_CHARS)
#define WC_FLAGSMASK (WC_DISCARDNS|WC_SEPCHARS|WC_DEFAULTCHAR|WC_ERR_INVALID_CHARS|\
                      WC_COMPOSITECHECK|WC_NO_BEST_FIT_CHARS)

/* current code pages */


struct locale_name
{
    WCHAR  win_name[128];   /* Windows name ("en-US") */
    WCHAR  lang[128];       /* language ("en") (note: buffer contains the other strings too) */
    WCHAR *country;         /* country ("US") */
    WCHAR *charset;         /* charset ("UTF-8") for Unix format only */
    WCHAR *script;          /* script ("Latn") for Windows format only */
    WCHAR *modifier;        /* modifier or sort order */
    LCID   lcid;            /* corresponding LCID */
    int    matches;         /* number of elements matching LCID (0..4) */
    UINT   codepage;        /* codepage corresponding to charset */
};

/* locale ids corresponding to the various Unix locale parameters */


static CRITICAL_SECTION cache_section;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &cache_section,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": cache_section") }
};
static CRITICAL_SECTION cache_section = { &critsect_debug, -1, 0, 0, 0, 0 };


/******************************************************************************
 *           CompareStringW    (KERNEL32.@)
 *
 * See CompareStringA.
 */
INT WINAPI CompareStringW(LCID lcid, DWORD flags,
                          LPCWSTR str1, INT len1, LPCWSTR str2, INT len2)
{
    return CompareStringEx(NULL, flags, str1, len1, str2, len2, NULL, NULL, 0);
}





/******************************************************************************
 *           CompareStringEx    (KERNEL32.@)
 */
INT WINAPI CompareStringEx(LPCWSTR locale, DWORD flags, LPCWSTR str1, INT len1,
                           LPCWSTR str2, INT len2, LPNLSVERSIONINFO version, LPVOID reserved, LPARAM lParam)
{
    DWORD supported_flags = NORM_IGNORECASE|NORM_IGNORENONSPACE|NORM_IGNORESYMBOLS|SORT_STRINGSORT
                           |NORM_IGNOREKANATYPE|NORM_IGNOREWIDTH|LOCALE_USE_CP_ACP;
    DWORD semistub_flags = NORM_LINGUISTIC_CASING|LINGUISTIC_IGNORECASE|0x10000000;
    /* 0x10000000 is related to diacritics in Arabic, Japanese, and Hebrew */
    INT ret;
    // static int once;

    // if (version) FIXME("unexpected version parameter\n");
    // if (reserved) FIXME("unexpected reserved value\n");
    // if (lParam) FIXME("unexpected lParam\n");

    if (!str1 || !str2)
    {
        // SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    if (flags & ~(supported_flags|semistub_flags))
    {
        // SetLastError(ERROR_INVALID_FLAGS);
        return 0;
    }

  //   if (flags & semistub_flags)
  //   {
  //       if (!once++)
  //           FIXME("semi-stub behavior for flag(s) 0x%x\n", flags & semistub_flags);
  //   }

    if (len1 < 0) len1 = strlenW(str1);
    if (len2 < 0) len2 = strlenW(str2);

    ret = wine_compare_string(flags, str1, len1, str2, len2);

    if (ret) /* need to translate result */
        return (ret < 0) ? CSTR_LESS_THAN : CSTR_GREATER_THAN;
    return CSTR_EQUAL;
}






