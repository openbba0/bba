/*
 * Copyright (C) 2015-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumlibc.h"

gpointer
gum_memset (gpointer dst,
            gint c,
            gsize n)
{
  gsize offset;

  for (offset = 0; offset != n; offset++)
    ((guint8 *) dst)[offset] = c;

  return dst;
}

