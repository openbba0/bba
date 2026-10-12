/*
 * Copyright (C) 2008-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Stefano Moioli <smxdev4@gmail.com>
 * Copyright (C) 2024 Yannis Juglaret <yjuglaret@mozilla.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gum.h"

#include "gum-init.h"
#include "gumexceptorbackend.h"
#include "guminterceptor-priv.h"
#include "gummemory-priv.h"
#include "gumprintf.h"
#include "gumtls-priv.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static void gum_destructor_invoke (GumDestructorFunc destructor);

static void gum_do_init (void);

static GumAddress * gum_address_copy (const GumAddress * address);
static void gum_address_free (GumAddress * address);

static GumCpuFeatures gum_do_query_cpu_features (void);

static gboolean gum_initialized = FALSE;
static GSList * gum_early_destructors = NULL;
static GSList * gum_final_destructors = NULL;

G_DEFINE_QUARK (gum-error-quark, gum_error)

G_DEFINE_BOXED_TYPE (GumAddress, gum_address, gum_address_copy,
                     gum_address_free)

void
gum_init (void)
{
  if (gum_initialized)
    return;
  gum_initialized = TRUE;

  gum_internal_heap_ref ();
  gum_do_init ();
}

void
gum_shutdown (void)
{
  g_slist_foreach (gum_early_destructors, (GFunc) gum_destructor_invoke, NULL);
  g_slist_free (gum_early_destructors);
  gum_early_destructors = NULL;
}

void
gum_deinit (void)
{
  g_assert (gum_initialized);

  gum_shutdown ();

  _gum_tls_deinit ();

  g_slist_foreach (gum_final_destructors, (GFunc) gum_destructor_invoke, NULL);
  g_slist_free (gum_final_destructors);
  gum_final_destructors = NULL;

  _gum_interceptor_deinit ();

  gum_initialized = FALSE;
}

static void
gum_do_init (void)
{
  _gum_tls_init ();
  _gum_interceptor_init ();
  _gum_tls_realize ();
}

void
_gum_register_early_destructor (GumDestructorFunc destructor)
{
  gum_early_destructors = g_slist_prepend (gum_early_destructors,
      GUM_FUNCPTR_TO_POINTER (destructor));
}

void
_gum_register_destructor (GumDestructorFunc destructor)
{
  gum_final_destructors = g_slist_prepend (gum_final_destructors,
      GUM_FUNCPTR_TO_POINTER (destructor));
}

static void
gum_destructor_invoke (GumDestructorFunc destructor)
{
  destructor ();
}

void
gum_prepare_to_fork (void)
{
  _gum_exceptor_backend_prepare_to_fork ();
}

void
gum_recover_from_fork_in_parent (void)
{
  _gum_exceptor_backend_recover_from_fork_in_parent ();
}

void
gum_recover_from_fork_in_child (void)
{
  _gum_exceptor_backend_recover_from_fork_in_child ();
}

void
gum_panic (const gchar * format,
           ...)
{
  va_list args;

  va_start (args, format);
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, format, args);
  va_end (args);

  g_abort ();
}

static GumAddress *
gum_address_copy (const GumAddress * address)
{
  return g_slice_dup (GumAddress, address);
}

static void
gum_address_free (GumAddress * address)
{
  g_slice_free (GumAddress, address);
}

GumCpuFeatures
gum_query_cpu_features (void)
{
  static gsize cached_result = 0;

  if (g_once_init_enter (&cached_result))
  {
    GumCpuFeatures features = gum_do_query_cpu_features ();

    g_once_init_leave (&cached_result, features + 1);
  }

  return cached_result - 1;
}


static GumCpuFeatures
gum_do_query_cpu_features (void)
{
  return 0;
}

