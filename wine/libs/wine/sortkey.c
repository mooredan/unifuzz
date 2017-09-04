/*
 * Unicode sort key generation
 *
 * Copyright 2003 Dmitry Timoshkov
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
#include "wine/unicode.h"

extern unsigned int wine_decompose( WCHAR ch, WCHAR *dst, unsigned int dstlen );
extern const unsigned int collation_table[];

static inline int compare_unicode_weights(int flags, const WCHAR *str1, int len1,
                                          const WCHAR *str2, int len2)
{
    unsigned int ce1, ce2;
    int ret;

    /* 32-bit collation element table format:
     * unicode weight - high 16 bit, diacritic weight - high 8 bit of low 16 bit,
     * case weight - high 4 bit of low 8 bit.
     */
    while (len1 > 0 && len2 > 0)
    {
        if (flags & NORM_IGNORESYMBOLS)
        {
            int skip = 0;
            /* FIXME: not tested */
            if (get_char_typeW(*str1) & (C1_PUNCT | C1_SPACE))
            {
                str1++;
                len1--;
                skip = 1;
            }
            if (get_char_typeW(*str2) & (C1_PUNCT | C1_SPACE))
            {
                str2++;
                len2--;
                skip = 1;
            }
            if (skip) continue;
        }

       /* hyphen and apostrophe are treated differently depending on
        * whether SORT_STRINGSORT specified or not
        */
        if (!(flags & SORT_STRINGSORT))
        {
            if (*str1 == '-' || *str1 == '\'')
            {
                if (*str2 != '-' && *str2 != '\'')
                {
                    str1++;
                    len1--;
                    continue;
                }
            }
            else if (*str2 == '-' || *str2 == '\'')
            {
                str2++;
                len2--;
                continue;
            }
        }

        ce1 = collation_table[collation_table[*str1 >> 8] + (*str1 & 0xff)];
        ce2 = collation_table[collation_table[*str2 >> 8] + (*str2 & 0xff)];

        if (ce1 != (unsigned int)-1 && ce2 != (unsigned int)-1)
            ret = (ce1 >> 16) - (ce2 >> 16);
        else
            ret = *str1 - *str2;

        if (ret) return ret;

        str1++;
        str2++;
        len1--;
        len2--;
    }
    while (len1 && !*str1)
    {
        str1++;
        len1--;
    }
    while (len2 && !*str2)
    {
        str2++;
        len2--;
    }
    return len1 - len2;
}

static inline int compare_diacritic_weights(int flags, const WCHAR *str1, int len1,
                                            const WCHAR *str2, int len2)
{
    unsigned int ce1, ce2;
    int ret;

    /* 32-bit collation element table format:
     * unicode weight - high 16 bit, diacritic weight - high 8 bit of low 16 bit,
     * case weight - high 4 bit of low 8 bit.
     */
    while (len1 > 0 && len2 > 0)
    {
        if (flags & NORM_IGNORESYMBOLS)
        {
            int skip = 0;
            /* FIXME: not tested */
            if (get_char_typeW(*str1) & (C1_PUNCT | C1_SPACE))
            {
                str1++;
                len1--;
                skip = 1;
            }
            if (get_char_typeW(*str2) & (C1_PUNCT | C1_SPACE))
            {
                str2++;
                len2--;
                skip = 1;
            }
            if (skip) continue;
        }

        ce1 = collation_table[collation_table[*str1 >> 8] + (*str1 & 0xff)];
        ce2 = collation_table[collation_table[*str2 >> 8] + (*str2 & 0xff)];

        if (ce1 != (unsigned int)-1 && ce2 != (unsigned int)-1)
            ret = ((ce1 >> 8) & 0xff) - ((ce2 >> 8) & 0xff);
        else
            ret = *str1 - *str2;

        if (ret) return ret;

        str1++;
        str2++;
        len1--;
        len2--;
    }
    while (len1 && !*str1)
    {
        str1++;
        len1--;
    }
    while (len2 && !*str2)
    {
        str2++;
        len2--;
    }
    return len1 - len2;
}

static inline int compare_case_weights(int flags, const WCHAR *str1, int len1,
                                       const WCHAR *str2, int len2)
{
    unsigned int ce1, ce2;
    int ret;

    /* 32-bit collation element table format:
     * unicode weight - high 16 bit, diacritic weight - high 8 bit of low 16 bit,
     * case weight - high 4 bit of low 8 bit.
     */
    while (len1 > 0 && len2 > 0)
    {
        if (flags & NORM_IGNORESYMBOLS)
        {
            int skip = 0;
            /* FIXME: not tested */
            if (get_char_typeW(*str1) & (C1_PUNCT | C1_SPACE))
            {
                str1++;
                len1--;
                skip = 1;
            }
            if (get_char_typeW(*str2) & (C1_PUNCT | C1_SPACE))
            {
                str2++;
                len2--;
                skip = 1;
            }
            if (skip) continue;
        }

        ce1 = collation_table[collation_table[*str1 >> 8] + (*str1 & 0xff)];
        ce2 = collation_table[collation_table[*str2 >> 8] + (*str2 & 0xff)];

        if (ce1 != (unsigned int)-1 && ce2 != (unsigned int)-1)
            ret = ((ce1 >> 4) & 0x0f) - ((ce2 >> 4) & 0x0f);
        else
            ret = *str1 - *str2;

        if (ret) return ret;

        str1++;
        str2++;
        len1--;
        len2--;
    }
    while (len1 && !*str1)
    {
        str1++;
        len1--;
    }
    while (len2 && !*str2)
    {
        str2++;
        len2--;
    }
    return len1 - len2;
}

int wine_compare_string(int flags, const WCHAR *str1, int len1,
                        const WCHAR *str2, int len2)
{
    int ret;

    ret = compare_unicode_weights(flags, str1, len1, str2, len2);
    if (!ret)
    {
        if (!(flags & NORM_IGNORENONSPACE))
            ret = compare_diacritic_weights(flags, str1, len1, str2, len2);
        if (!ret && !(flags & NORM_IGNORECASE))
            ret = compare_case_weights(flags, str1, len1, str2, len2);
    }
    return ret;
}
