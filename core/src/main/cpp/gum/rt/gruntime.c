#include "glib.h"
#include "spindle_str.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define GUM_RT_TYPE_BASE 64
#define GUM_RT_TYPE_MAX 512
#define GUM_RT_DISPOSE_GUARD 0x40000000

typedef struct {
  GType id;
  GType parent;
  guint class_size;
  guint instance_size;
  GClassInitFunc class_init;
  GInstanceInitFunc instance_init;
  GBoxedCopyFunc boxed_copy;
  GBoxedFreeFunc boxed_free;
  gboolean is_interface;
  GObjectClass *klass;
  volatile gsize ready;
  struct {
    GType iface;
    GInterfaceInitFunc init;
    GClassInitFunc default_init;
    guint vtable_size;
  } ifaces[GUM_RT_MAX_IFACES];
  guint n_ifaces;
} GumTypeSlot;

static GumTypeSlot type_slots[GUM_RT_TYPE_MAX];
static guint n_type_slots;
static pthread_mutex_t type_lock = PTHREAD_MUTEX_INITIALIZER;
static guint next_quark = 2;

static void
base_dispose (GObject *object)
{
  (void) object;
}

static void
base_finalize (GObject *object)
{
  (void) object;
}

static GObjectClass base_object_class = {
  .g_type_class = { G_TYPE_OBJECT },
  .dispose = base_dispose,
  .finalize = base_finalize,
};

static void
oom (void)
{
  abort ();
}

gpointer
gum_rt_steal_pointer (gpointer pp)
{
  gpointer *ptr = pp;
  gpointer val = *ptr;

  *ptr = NULL;
  return val;
}

GQuark
gum_rt_file_error_quark (void)
{
  return 1;
}

GQuark
gum_rt_alloc_quark (void)
{
  return __atomic_fetch_add (&next_quark, 1, __ATOMIC_RELAXED);
}

static GumTypeSlot *
type_slot (GType id)
{
  guint index;

  if (id < GUM_RT_TYPE_BASE)
    return NULL;
  index = (guint) (id - GUM_RT_TYPE_BASE);
  if (index >= n_type_slots)
    return NULL;
  return &type_slots[index];
}

static GType
type_add (GumTypeSlot *slot)
{
  guint index;

  pthread_mutex_lock (&type_lock);
  if (n_type_slots >= GUM_RT_TYPE_MAX)
    oom ();
  index = n_type_slots++;
  slot->id = GUM_RT_TYPE_BASE + index;
  type_slots[index] = *slot;
  pthread_mutex_unlock (&type_lock);
  return slot->id;
}

GType
gum_rt_alloc_type_id (void)
{
  GumTypeSlot slot;

  memset (&slot, 0, sizeof (slot));
  return type_add (&slot);
}

GType
gum_rt_register_class (GType parent_type, guint class_size,
                       GClassInitFunc class_init, guint instance_size,
                       GInstanceInitFunc instance_init)
{
  GumTypeSlot slot;

  memset (&slot, 0, sizeof (slot));
  slot.parent = parent_type;
  slot.class_size = class_size;
  slot.class_init = class_init;
  slot.instance_size = instance_size;
  slot.instance_init = instance_init;
  return type_add (&slot);
}

GType
gum_rt_register_interface (guint class_size, GClassInitFunc class_init)
{
  GumTypeSlot slot;

  memset (&slot, 0, sizeof (slot));
  slot.class_size = class_size;
  slot.class_init = class_init;
  slot.is_interface = TRUE;
  return type_add (&slot);
}

GType
gum_rt_register_boxed (GBoxedCopyFunc copy_func, GBoxedFreeFunc free_func)
{
  GumTypeSlot slot;

  memset (&slot, 0, sizeof (slot));
  slot.boxed_copy = copy_func;
  slot.boxed_free = free_func;
  return type_add (&slot);
}

void
g_type_interface_add_prerequisite (GType interface_type, GType prerequisite_type)
{
  (void) interface_type;
  (void) prerequisite_type;
}

void
g_type_add_interface_static (GType instance_type, GType interface_type,
                             const GInterfaceInfo *info)
{
  GumTypeSlot *slot = type_slot (instance_type);
  GumTypeSlot *iface = type_slot (interface_type);

  if (slot == NULL || iface == NULL || slot->n_ifaces >= GUM_RT_MAX_IFACES)
    oom ();
  slot->ifaces[slot->n_ifaces].iface = interface_type;
  slot->ifaces[slot->n_ifaces].init = info->interface_init;
  slot->ifaces[slot->n_ifaces].default_init = iface->class_init;
  slot->ifaces[slot->n_ifaces].vtable_size = iface->class_size;
  slot->n_ifaces++;
}

static void
type_ensure (GumTypeSlot *slot)
{
  GObjectClass *klass;
  guint i;

  if (slot == NULL)
    return;
  if (__atomic_load_n (&slot->ready, __ATOMIC_ACQUIRE) == 1)
    return;
  if (!g_once_init_enter (&slot->ready))
    return;

  klass = g_malloc0 (slot->class_size != 0 ? slot->class_size : sizeof (GObjectClass));
  klass->g_type_class.g_type = slot->id;
  slot->klass = klass;
  if (slot->class_init != NULL)
    slot->class_init (klass, NULL);

  for (i = 0; i != slot->n_ifaces; i++)
    {
      gpointer vt = g_malloc0 (slot->ifaces[i].vtable_size);
      GTypeInterface *header = vt;

      header->g_type = slot->ifaces[i].iface;
      header->g_instance_type = slot->id;
      if (slot->ifaces[i].default_init != NULL)
        slot->ifaces[i].default_init (vt, NULL);
      if (slot->ifaces[i].init != NULL)
        slot->ifaces[i].init (vt, NULL);
      klass->iface_type[i] = slot->ifaces[i].iface;
      klass->iface_vt[i] = vt;
    }

  g_once_init_leave (&slot->ready, 1);
}

gpointer
g_type_class_peek_parent (gpointer g_class)
{
  GObjectClass *klass = g_class;
  GumTypeSlot *slot = type_slot (klass->g_type_class.g_type);
  GumTypeSlot *parent;

  if (slot == NULL || slot->parent == G_TYPE_OBJECT || slot->parent == G_TYPE_INVALID)
    return &base_object_class;
  parent = type_slot (slot->parent);
  type_ensure (parent);
  return parent != NULL ? parent->klass : &base_object_class;
}

gpointer
gum_rt_iface (gpointer instance, GType iface_type)
{
  GObject *object = instance;
  GObjectClass *klass;
  guint i;

  if (object == NULL)
    return NULL;
  klass = G_OBJECT_GET_CLASS (object);
  for (i = 0; i != GUM_RT_MAX_IFACES; i++)
    {
      if (klass->iface_type[i] == iface_type)
        return klass->iface_vt[i];
    }
  return NULL;
}

static void
value_clear (GValue *value)
{
  if (value->kind == GUM_PROP_STRING)
    g_free (value->data.v_pointer);
  memset (value, 0, sizeof (*value));
}

gpointer
g_object_new (GType object_type, const gchar *first_property_name, ...)
{
  GumTypeSlot *slot = type_slot (object_type);
  GObject *object;
  GObjectClass *klass;
  va_list ap;
  const gchar *name;

  if (slot == NULL)
    oom ();
  type_ensure (slot);
  object = g_malloc0 (slot->instance_size);
  object->g_type_instance.g_class = (GTypeClass *) slot->klass;
  object->ref_count = 1;
  if (slot->instance_init != NULL)
    slot->instance_init ((GTypeInstance *) object, slot->klass);

  klass = slot->klass;
  va_start (ap, first_property_name);
  for (name = first_property_name; name != NULL; name = va_arg (ap, const gchar *))
    {
      guint i;
      GValue value;

      memset (&value, 0, sizeof (value));
      for (i = 0; i != klass->n_props; i++)
        {
          if (strcmp (klass->props[i].name, name) == 0)
            break;
        }
      if (i == klass->n_props)
        oom ();
      value.g_type = klass->props[i].value_type;
      value.kind = klass->props[i].kind;
      switch (klass->props[i].kind)
        {
        case GUM_PROP_ENUM:
          g_value_set_enum (&value, va_arg (ap, gint));
          break;
        case GUM_PROP_INT:
          g_value_set_int (&value, va_arg (ap, gint));
          break;
        case GUM_PROP_UINT:
          g_value_set_uint (&value, va_arg (ap, guint));
          break;
        case GUM_PROP_UINT64:
          g_value_set_uint64 (&value, va_arg (ap, guint64));
          break;
        case GUM_PROP_STRING:
          g_value_set_string (&value, va_arg (ap, const gchar *));
          break;
        case GUM_PROP_BOXED:
          g_value_set_boxed (&value, va_arg (ap, gpointer));
          break;
        }
      if (klass->set_property != NULL)
        klass->set_property (object, klass->props[i].id, &value,
            klass->props[i].pspec);
      value_clear (&value);
    }
  va_end (ap);
  return object;
}

gpointer
g_object_ref (gpointer object)
{
  if (object != NULL)
    __atomic_fetch_add (&((GObject *) object)->ref_count, 1, __ATOMIC_RELAXED);
  return object;
}

typedef struct _GumSide GumSide;
struct _GumSide {
  GObject *object;
  GumSide *next;
};

typedef struct {
  GumSide link;
  GWeakNotify notify;
  gpointer data;
} GumWeak;

typedef struct {
  GumSide link;
  gchar *key;
  gpointer data;
  GDestroyNotify destroy;
} GumQData;

static GumWeak *weak_refs;
static GumQData *qdata;
static pthread_mutex_t side_lock = PTHREAD_MUTEX_INITIALIZER;

static void
notify_weak (GObject *object)
{
  GumWeak *keep = NULL;
  GumWeak *hit = NULL;
  GumWeak *cur;

  pthread_mutex_lock (&side_lock);
  cur = weak_refs;
  weak_refs = NULL;
  while (cur != NULL)
    {
      GumWeak *next = (GumWeak *) cur->link.next;
      if (cur->link.object == object)
        {
          cur->link.next = (GumSide *) hit;
          hit = cur;
        }
      else
        {
          cur->link.next = (GumSide *) keep;
          keep = cur;
        }
      cur = next;
    }
  weak_refs = keep;
  pthread_mutex_unlock (&side_lock);

  while (hit != NULL)
    {
      GumWeak *next = (GumWeak *) hit->link.next;
      hit->notify (hit->data, object);
      g_free (hit);
      hit = next;
    }
}

static void
notify_qdata (GObject *object)
{
  GumQData *keep = NULL;
  GumQData *hit = NULL;
  GumQData *cur;

  pthread_mutex_lock (&side_lock);
  cur = qdata;
  qdata = NULL;
  while (cur != NULL)
    {
      GumQData *next = (GumQData *) cur->link.next;
      if (cur->link.object == object)
        {
          cur->link.next = (GumSide *) hit;
          hit = cur;
        }
      else
        {
          cur->link.next = (GumSide *) keep;
          keep = cur;
        }
      cur = next;
    }
  qdata = keep;
  pthread_mutex_unlock (&side_lock);

  while (hit != NULL)
    {
      GumQData *next = (GumQData *) hit->link.next;
      if (hit->destroy != NULL)
        hit->destroy (hit->data);
      g_free (hit->key);
      g_free (hit);
      hit = next;
    }
}

void
g_object_unref (gpointer object)
{
  GObject *obj = object;
  GObjectClass *klass;
  gint prev;
  gint left;

  if (obj == NULL)
    return;
  prev = __atomic_fetch_sub (&obj->ref_count, 1, __ATOMIC_ACQ_REL);
  if (prev != 1)
    return;

  __atomic_store_n (&obj->ref_count, GUM_RT_DISPOSE_GUARD, __ATOMIC_RELEASE);
  klass = G_OBJECT_GET_CLASS (obj);
  if (klass->dispose != NULL)
    klass->dispose (obj);
  left = __atomic_load_n (&obj->ref_count, __ATOMIC_ACQUIRE);
  if (left != GUM_RT_DISPOSE_GUARD)
    {
      __atomic_fetch_sub (&obj->ref_count, GUM_RT_DISPOSE_GUARD, __ATOMIC_ACQ_REL);
      return;
    }

  notify_weak (obj);
  notify_qdata (obj);
  if (klass->finalize != NULL)
    klass->finalize (obj);
  g_free (obj);
}

void
g_object_weak_ref (GObject *object, GWeakNotify notify, gpointer data)
{
  GumWeak *weak = g_new0 (GumWeak, 1);

  weak->link.object = object;
  weak->notify = notify;
  weak->data = data;
  pthread_mutex_lock (&side_lock);
  weak->link.next = (GumSide *) weak_refs;
  weak_refs = weak;
  pthread_mutex_unlock (&side_lock);
}

void
g_object_set_data_full (GObject *object, const gchar *key, gpointer data,
                        GDestroyNotify destroy)
{
  GumQData *cur;

  pthread_mutex_lock (&side_lock);
  for (cur = qdata; cur != NULL; cur = (GumQData *) cur->link.next)
    {
      if (cur->link.object == object && strcmp (cur->key, key) == 0)
        {
          gpointer old = cur->data;
          GDestroyNotify old_destroy = cur->destroy;
          cur->data = data;
          cur->destroy = destroy;
          pthread_mutex_unlock (&side_lock);
          if (old_destroy != NULL)
            old_destroy (old);
          return;
        }
    }
  cur = g_new0 (GumQData, 1);
  cur->link.object = object;
  cur->key = g_strdup (key);
  cur->data = data;
  cur->destroy = destroy;
  cur->link.next = (GumSide *) qdata;
  qdata = cur;
  pthread_mutex_unlock (&side_lock);
}

gpointer
g_object_get_data (GObject *object, const gchar *key)
{
  GumQData *cur;
  gpointer data = NULL;

  pthread_mutex_lock (&side_lock);
  for (cur = qdata; cur != NULL; cur = (GumQData *) cur->link.next)
    {
      if (cur->link.object == object && strcmp (cur->key, key) == 0)
        {
          data = cur->data;
          break;
        }
    }
  pthread_mutex_unlock (&side_lock);
  return data;
}

static GParamSpec *
param_new (const gchar *name, GumPropKind kind, GType value_type)
{
  GParamSpec *spec = g_new0 (GParamSpec, 1);

  spec->name = g_strdup (name);
  spec->kind = kind;
  spec->value_type = value_type;
  return spec;
}

void
g_object_class_install_property (GObjectClass *oclass, guint property_id,
                                 GParamSpec *pspec)
{
  guint i;

  if (oclass->n_props >= GUM_RT_MAX_PROPS)
    oom ();
  i = oclass->n_props++;
  oclass->props[i].id = property_id;
  oclass->props[i].name = g_strdup (pspec->name);
  oclass->props[i].kind = pspec->kind;
  oclass->props[i].value_type = pspec->value_type;
  oclass->props[i].pspec = pspec;
}

GParamSpec *
g_param_spec_enum (const gchar *name, const gchar *nick, const gchar *blurb,
                   GType enum_type, gint default_value, gint flags)
{
  (void) nick; (void) blurb; (void) default_value; (void) flags;
  return param_new (name, GUM_PROP_ENUM, enum_type);
}

GParamSpec *
g_param_spec_uint (const gchar *name, const gchar *nick, const gchar *blurb,
                   guint minimum, guint maximum, guint default_value, gint flags)
{
  (void) nick; (void) blurb; (void) minimum; (void) maximum;
  (void) default_value; (void) flags;
  return param_new (name, GUM_PROP_UINT, 0);
}

GParamSpec *
g_param_spec_int (const gchar *name, const gchar *nick, const gchar *blurb,
                  gint minimum, gint maximum, gint default_value, gint flags)
{
  (void) nick; (void) blurb; (void) minimum; (void) maximum;
  (void) default_value; (void) flags;
  return param_new (name, GUM_PROP_INT, 0);
}

GParamSpec *
g_param_spec_uint64 (const gchar *name, const gchar *nick, const gchar *blurb,
                     guint64 minimum, guint64 maximum, guint64 default_value,
                     gint flags)
{
  (void) nick; (void) blurb; (void) minimum; (void) maximum;
  (void) default_value; (void) flags;
  return param_new (name, GUM_PROP_UINT64, 0);
}

GParamSpec *
g_param_spec_string (const gchar *name, const gchar *nick, const gchar *blurb,
                     const gchar *default_value, gint flags)
{
  (void) nick; (void) blurb; (void) default_value; (void) flags;
  return param_new (name, GUM_PROP_STRING, G_TYPE_STRING);
}

GParamSpec *
g_param_spec_boxed (const gchar *name, const gchar *nick, const gchar *blurb,
                    GType boxed_type, gint flags)
{
  (void) nick; (void) blurb; (void) flags;
  return param_new (name, GUM_PROP_BOXED, boxed_type);
}

void
g_value_set_enum (GValue *value, gint v_enum)
{
  value->kind = GUM_PROP_ENUM;
  value->data.v_int = v_enum;
}

void
g_value_set_uint (GValue *value, guint v_uint)
{
  value->kind = GUM_PROP_UINT;
  value->data.v_uint = v_uint;
}

void
g_value_set_int (GValue *value, gint v_int)
{
  value->kind = GUM_PROP_INT;
  value->data.v_int = v_int;
}

void
g_value_set_uint64 (GValue *value, guint64 v_uint64)
{
  value->kind = GUM_PROP_UINT64;
  value->data.v_uint64 = v_uint64;
}

void
g_value_set_string (GValue *value, const gchar *v_string)
{
  g_free (value->data.v_pointer);
  value->kind = GUM_PROP_STRING;
  value->g_type = G_TYPE_STRING;
  value->data.v_pointer = g_strdup (v_string);
}

void
g_value_set_boxed (GValue *value, gconstpointer v_boxed)
{
  value->kind = GUM_PROP_BOXED;
  value->data.v_pointer = (gpointer) v_boxed;
}

gint
g_value_get_enum (const GValue *value)
{
  return value->data.v_int;
}

guint
g_value_get_uint (const GValue *value)
{
  return value->data.v_uint;
}

guint64
g_value_get_uint64 (const GValue *value)
{
  return value->data.v_uint64;
}

gchar *
g_value_dup_string (const GValue *value)
{
  return g_strdup (value->data.v_pointer);
}

gpointer
g_value_dup_boxed (const GValue *value)
{
  GumTypeSlot *slot;

  if (value->data.v_pointer == NULL)
    return NULL;
  if (value->g_type == G_TYPE_BYTES)
    return g_bytes_ref (value->data.v_pointer);
  slot = type_slot (value->g_type);
  if (slot != NULL && slot->boxed_copy != NULL)
    return slot->boxed_copy (value->data.v_pointer);
  return value->data.v_pointer;
}

gpointer
g_malloc (gsize n_bytes)
{
  gpointer mem;

  if (n_bytes == 0)
    return NULL;
  mem = malloc (n_bytes);
  if (mem == NULL)
    oom ();
  return mem;
}

gpointer
g_malloc0 (gsize n_bytes)
{
  gpointer mem = g_malloc (n_bytes);

  if (mem != NULL)
    memset (mem, 0, n_bytes);
  return mem;
}

gpointer
g_realloc (gpointer mem, gsize n_bytes)
{
  gpointer next;

  if (n_bytes == 0)
    {
      g_free (mem);
      return NULL;
    }
  next = realloc (mem, n_bytes);
  if (next == NULL)
    oom ();
  return next;
}

void
g_free (gpointer mem)
{
  free (mem);
}

gpointer
g_memdup (gconstpointer mem, guint byte_size)
{
  gpointer copy;

  if (mem == NULL || byte_size == 0)
    return NULL;
  copy = g_malloc (byte_size);
  memcpy (copy, mem, byte_size);
  return copy;
}

gpointer
g_slice_alloc (gsize block_size)
{
  return g_malloc (block_size);
}

gpointer
g_slice_alloc0 (gsize block_size)
{
  return g_malloc0 (block_size);
}

void
g_slice_free1 (gsize block_size, gpointer mem)
{
  (void) block_size;
  g_free (mem);
}

gchar *
g_strdup (const gchar *str)
{
  return str != NULL ? g_memdup (str, (guint) strlen (str) + 1) : NULL;
}

gchar *
g_strndup (const gchar *str, gsize n)
{
  gsize len;
  gchar *copy;

  if (str == NULL)
    return NULL;
  len = strlen (str);
  if (len > n)
    len = n;
  copy = g_malloc (len + 1);
  memcpy (copy, str, len);
  copy[len] = '\0';
  return copy;
}

gchar *
g_strdup_printf (const gchar *format, ...)
{
  gchar *str;
  va_list ap;

  va_start (ap, format);
  if (vasprintf (&str, format, ap) < 0)
    oom ();
  va_end (ap);
  return str;
}

gint
g_vsnprintf (gchar *string, gulong n, const gchar *format, va_list args)
{
  return vsnprintf (string, n, format, args);
}

gchar *
g_strchomp (gchar *str)
{
  gsize len;

  if (str == NULL)
    return NULL;
  len = strlen (str);
  while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' ||
      str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == '\f' ||
      str[len - 1] == '\v'))
    str[--len] = '\0';
  return str;
}

gchar **
g_strsplit (const gchar *string, const gchar *delimiter, gint max_tokens)
{
  gsize delim_len;
  guint count = 0;
  const gchar *p;
  gchar **out;
  guint i;

  if (string == NULL || delimiter == NULL || delimiter[0] == '\0')
    return NULL;
  delim_len = strlen (delimiter);
  p = string;
  for (;;)
    {
      count++;
      if (max_tokens > 0 && (gint) count >= max_tokens)
        break;
      p = strstr (p, delimiter);
      if (p == NULL)
        break;
      p += delim_len;
    }
  out = g_new0 (gchar *, count + 1);
  p = string;
  for (i = 0; i != count; i++)
    {
      const gchar *hit = NULL;
      gsize len;

      if (i + 1 != count)
        hit = strstr (p, delimiter);
      len = hit != NULL ? (gsize) (hit - p) : strlen (p);
      out[i] = g_strndup (p, len);
      p = hit != NULL ? hit + delim_len : p + len;
    }
  return out;
}

void
g_strfreev (gchar **str_array)
{
  guint i;

  if (str_array == NULL)
    return;
  for (i = 0; str_array[i] != NULL; i++)
    g_free (str_array[i]);
  g_free (str_array);
}

guint
g_strv_length (gchar **str_array)
{
  guint i = 0;

  if (str_array == NULL)
    return 0;
  while (str_array[i] != NULL)
    i++;
  return i;
}

gboolean
g_str_has_prefix (const gchar *str, const gchar *prefix)
{
  gsize n;

  if (str == NULL || prefix == NULL)
    return FALSE;
  n = strlen (prefix);
  return strncmp (str, prefix, n) == 0;
}

gboolean
g_str_has_suffix (const gchar *str, const gchar *suffix)
{
  gsize nstr, nsuf;

  if (str == NULL || suffix == NULL)
    return FALSE;
  nstr = strlen (str);
  nsuf = strlen (suffix);
  if (nsuf > nstr)
    return FALSE;
  return strcmp (str + nstr - nsuf, suffix) == 0;
}

guint
g_str_hash (gconstpointer v)
{
  const gchar *p = v;
  guint32 h = 5381;

  for (; *p != '\0'; p++)
    h = (h << 5) + h + (guchar) *p;
  return h;
}

gboolean
g_str_equal (gconstpointer v1, gconstpointer v2)
{
  return strcmp (v1, v2) == 0;
}

guint
g_direct_hash (gconstpointer v)
{
  return (guint) (guintptr) v;
}

gint
g_ascii_xdigit_value (gchar c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

gchar
g_ascii_toupper (gchar c)
{
  if (c >= 'a' && c <= 'z')
    return (gchar) (c - 'a' + 'A');
  return c;
}

const gchar *
g_strerror (gint errnum)
{
  return strerror (errnum);
}

void
g_logv (const gchar *log_domain, gint log_level, const gchar *format, va_list args)
{
  /* "GLib" */
  static const unsigned char enc_glib[] = {
    0xe0, 0xeb, 0xce, 0xc5, 0xa7
  };
  char fallback[sizeof (enc_glib)];
  const gchar *prefix = log_domain;

  (void) log_level;
  if (prefix == NULL)
  {
    sp_dec (fallback, enc_glib, sizeof (enc_glib));
    prefix = fallback;
  }
  fprintf (stderr, "%s: ", prefix);
  vfprintf (stderr, format, args);
  fputc ('\n', stderr);
}

void
g_error (const gchar *format, ...)
{
  /* "GLib" */
  static const unsigned char enc_glib[] = {
    0xe0, 0xeb, 0xce, 0xc5, 0xa7
  };
  char domain[sizeof (enc_glib)];
  va_list ap;

  sp_dec (domain, enc_glib, sizeof (enc_glib));
  va_start (ap, format);
  g_logv (domain, G_LOG_LEVEL_CRITICAL, format, ap);
  va_end (ap);
  abort ();
}

void
g_abort (void)
{
  abort ();
}

static void
array_terminate (GArray *array)
{
  if (array->zero_terminated && array->elt_size != 0 && array->data != NULL)
    memset (array->data + ((gsize) array->len * array->elt_size), 0, array->elt_size);
}

static void
array_ensure (GArray *array, guint extra)
{
  guint need;
  guint alloc;
  gsize bytes;

  need = array->len + extra + (array->zero_terminated ? 1 : 0);
  if (need <= array->alloc || array->elt_size == 0)
    return;
  alloc = array->alloc != 0 ? array->alloc : 8;
  while (alloc < need)
    alloc *= 2;
  bytes = (gsize) alloc * array->elt_size;
  array->data = g_realloc (array->data, bytes);
  if (array->clear)
    memset (array->data + ((gsize) array->alloc * array->elt_size), 0,
        (gsize) (alloc - array->alloc) * array->elt_size);
  array->alloc = alloc;
}

static void
array_clear_range (GArray *array, guint index, guint length)
{
  guint i;

  if (array->clear_func == NULL)
    return;
  for (i = 0; i != length; i++)
    array->clear_func (array->data + ((gsize) (index + i) * array->elt_size));
}

static void
array_dispose (GArray *array, gboolean free_segment)
{
  if (free_segment)
    {
      array_clear_range (array, 0, array->len);
      g_free (array->data);
    }
  g_free (array);
}

GArray *
g_array_sized_new (gboolean zero_terminated, gboolean clear_,
                   guint element_size, guint reserved_size)
{
  GArray *array = g_new0 (GArray, 1);

  array->elt_size = element_size;
  array->ref_count = 1;
  array->zero_terminated = zero_terminated ? 1 : 0;
  array->clear = clear_ ? 1 : 0;
  if (reserved_size != 0)
    {
      array->alloc = reserved_size + (zero_terminated ? 1 : 0);
      array->data = array->clear
          ? g_malloc0 ((gsize) array->alloc * element_size)
          : g_malloc ((gsize) array->alloc * element_size);
      array_terminate (array);
    }
  return array;
}

GArray *
g_array_new (gboolean zero_terminated, gboolean clear_, guint element_size)
{
  return g_array_sized_new (zero_terminated, clear_, element_size, 0);
}

gchar *
g_array_free (GArray *array, gboolean free_segment)
{
  gchar *data;

  if (array == NULL)
    return NULL;
  data = array->data;
  if (__atomic_sub_fetch (&array->ref_count, 1, __ATOMIC_ACQ_REL) != 0)
    return free_segment ? NULL : data;
  if (!free_segment)
    {
      g_free (array);
      return data;
    }
  array_dispose (array, TRUE);
  return NULL;
}

void
g_array_unref (GArray *array)
{
  g_array_free (array, TRUE);
}

GArray *
g_array_set_size (GArray *array, guint length)
{
  if (length < array->len)
    array_clear_range (array, length, array->len - length);
  else if (length > array->len)
    array_ensure (array, length - array->len);
  array->len = length;
  array_terminate (array);
  return array;
}

GArray *
g_array_append_vals (GArray *array, gconstpointer data, guint len)
{
  array_ensure (array, len);
  if (len != 0 && array->elt_size != 0)
    memcpy (array->data + ((gsize) array->len * array->elt_size), data,
        (gsize) len * array->elt_size);
  array->len += len;
  array_terminate (array);
  return array;
}

GArray *
g_array_insert_vals (GArray *array, guint index_, gconstpointer data, guint len)
{
  gsize elt = array->elt_size;

  if (index_ > array->len)
    index_ = array->len;
  array_ensure (array, len);
  if (index_ != array->len && elt != 0)
    memmove (array->data + ((gsize) (index_ + len) * elt),
        array->data + ((gsize) index_ * elt),
        (gsize) (array->len - index_) * elt);
  if (len != 0 && elt != 0)
    memcpy (array->data + ((gsize) index_ * elt), data, (gsize) len * elt);
  array->len += len;
  array_terminate (array);
  return array;
}

GArray *
g_array_remove_index (GArray *array, guint index_)
{
  gsize elt = array->elt_size;

  if (index_ >= array->len)
    return array;
  array_clear_range (array, index_, 1);
  if (index_ + 1 != array->len && elt != 0)
    memmove (array->data + ((gsize) index_ * elt),
        array->data + ((gsize) (index_ + 1) * elt),
        (gsize) (array->len - index_ - 1) * elt);
  array->len--;
  array_terminate (array);
  return array;
}

void
g_array_sort (GArray *array, GCompareFunc compare_func)
{
  if (array->len > 1 && array->elt_size != 0)
    qsort (array->data, array->len, array->elt_size, compare_func);
}

gboolean
g_array_binary_search (GArray *array, gconstpointer target,
                       GCompareFunc compare_func, guint *out_match_index)
{
  guint left;
  guint right;
  guint middle = 0;
  gboolean found = FALSE;

  if (array->len == 0 || array->elt_size == 0)
    return FALSE;
  left = 0;
  right = array->len - 1;
  while (left <= right)
    {
      gint cmp;

      middle = left + (right - left) / 2;
      cmp = compare_func (array->data + ((gsize) middle * array->elt_size), target);
      if (cmp == 0)
        {
          found = TRUE;
          break;
        }
      if (cmp < 0)
        left = middle + 1;
      else if (middle == 0)
        break;
      else
        right = middle - 1;
    }
  if (!found)
    return FALSE;
  while (middle > 0 &&
      compare_func (array->data + ((gsize) (middle - 1) * array->elt_size), target) == 0)
    middle--;
  if (out_match_index != NULL)
    *out_match_index = middle;
  return TRUE;
}

void
g_array_set_clear_func (GArray *array, GDestroyNotify clear_func)
{
  array->clear_func = clear_func;
}

static void
ptr_array_ensure (GPtrArray *array, guint extra)
{
  guint need = array->len + extra;
  guint alloc;

  if (need <= array->alloc)
    return;
  alloc = array->alloc != 0 ? array->alloc : 8;
  while (alloc < need)
    alloc *= 2;
  array->pdata = g_realloc (array->pdata, alloc * sizeof (gpointer));
  array->alloc = alloc;
}

GPtrArray *
g_ptr_array_new_full (guint reserved_size, GDestroyNotify element_free_func)
{
  GPtrArray *array = g_new0 (GPtrArray, 1);

  array->ref_count = 1;
  array->element_free_func = element_free_func;
  if (reserved_size != 0)
    {
      array->alloc = reserved_size;
      array->pdata = g_malloc0 (reserved_size * sizeof (gpointer));
    }
  return array;
}

GPtrArray *
g_ptr_array_new_with_free_func (GDestroyNotify element_free_func)
{
  return g_ptr_array_new_full (0, element_free_func);
}

static void
ptr_array_dispose (GPtrArray *array, gboolean free_segment)
{
  guint i;

  if (free_segment)
    {
      if (array->element_free_func != NULL)
        {
          for (i = 0; i != array->len; i++)
            array->element_free_func (array->pdata[i]);
        }
      g_free (array->pdata);
    }
  g_free (array);
}

gpointer *
g_ptr_array_free (GPtrArray *array, gboolean free_segment)
{
  gpointer *pdata;

  if (array == NULL)
    return NULL;
  pdata = array->pdata;
  if (__atomic_sub_fetch (&array->ref_count, 1, __ATOMIC_ACQ_REL) != 0)
    return free_segment ? NULL : pdata;
  if (!free_segment)
    {
      g_free (array);
      return pdata;
    }
  ptr_array_dispose (array, TRUE);
  return NULL;
}

void
g_ptr_array_unref (GPtrArray *array)
{
  g_ptr_array_free (array, TRUE);
}

void
g_ptr_array_add (GPtrArray *array, gpointer data)
{
  ptr_array_ensure (array, 1);
  array->pdata[array->len++] = data;
}

gpointer
g_ptr_array_remove_index (GPtrArray *array, guint index_)
{
  gpointer item;

  if (index_ >= array->len)
    return NULL;
  item = array->pdata[index_];
  if (index_ + 1 != array->len)
    memmove (&array->pdata[index_], &array->pdata[index_ + 1],
        (array->len - index_ - 1) * sizeof (gpointer));
  array->len--;
  return item;
}

void
g_ptr_array_set_size (GPtrArray *array, gint length)
{
  guint i;

  if (length < 0)
    length = 0;
  if ((guint) length < array->len && array->element_free_func != NULL)
    {
      for (i = (guint) length; i != array->len; i++)
        array->element_free_func (array->pdata[i]);
    }
  if ((guint) length > array->len)
    {
      ptr_array_ensure (array, (guint) length - array->len);
      memset (&array->pdata[array->len], 0,
          ((guint) length - array->len) * sizeof (gpointer));
    }
  array->len = (guint) length;
}

void
g_ptr_array_sort (GPtrArray *array, GCompareFunc compare_func)
{
  if (array->len > 1)
    qsort (array->pdata, array->len, sizeof (gpointer), compare_func);
}

GSList *
g_slist_prepend (GSList *list, gpointer data)
{
  GSList *node = g_new (GSList, 1);

  node->data = data;
  node->next = list;
  return node;
}

GSList *
g_slist_append (GSList *list, gpointer data)
{
  GSList *node = g_new (GSList, 1);
  GSList *last;

  node->data = data;
  node->next = NULL;
  if (list == NULL)
    return node;
  for (last = list; last->next != NULL; last = last->next)
    ;
  last->next = node;
  return list;
}

GSList *
g_slist_remove (GSList *list, gconstpointer data)
{
  GSList *prev = NULL;
  GSList *cur;

  for (cur = list; cur != NULL; cur = cur->next)
    {
      if (cur->data == data)
        {
          if (prev != NULL)
            prev->next = cur->next;
          else
            list = cur->next;
          g_free (cur);
          break;
        }
      prev = cur;
    }
  return list;
}

GSList *
g_slist_find (GSList *list, gconstpointer data)
{
  for (; list != NULL; list = list->next)
    {
      if (list->data == data)
        return list;
    }
  return NULL;
}

GSList *
g_slist_copy (GSList *list)
{
  GSList *copy = NULL;
  GSList **tail = &copy;

  for (; list != NULL; list = list->next)
    {
      *tail = g_slist_prepend (NULL, list->data);
      tail = &(*tail)->next;
    }
  return copy;
}

void
g_slist_free (GSList *list)
{
  while (list != NULL)
    {
      GSList *next = list->next;
      g_free (list);
      list = next;
    }
}

void
g_slist_free_1 (GSList *list)
{
  g_free (list);
}

void
g_slist_free_full (GSList *list, GDestroyNotify free_func)
{
  while (list != NULL)
    {
      GSList *next = list->next;
      if (free_func != NULL)
        free_func (list->data);
      g_free (list);
      list = next;
    }
}

GSList *
g_slist_delete_link (GSList *list, GSList *link_)
{
  GSList *prev = NULL;
  GSList *cur;

  for (cur = list; cur != NULL; cur = cur->next)
    {
      if (cur == link_)
        {
          if (prev != NULL)
            prev->next = cur->next;
          else
            list = cur->next;
          g_free (cur);
          break;
        }
      prev = cur;
    }
  return list;
}

void
g_slist_foreach (GSList *list, GFunc func, gpointer user_data)
{
  for (; list != NULL; list = list->next)
    func (list->data, user_data);
}

GList *
g_list_remove_link (GList *list, GList *llink)
{
  if (llink == NULL)
    return list;
  if (llink->prev != NULL)
    llink->prev->next = llink->next;
  else
    list = llink->next;
  if (llink->next != NULL)
    llink->next->prev = llink->prev;
  llink->next = NULL;
  llink->prev = NULL;
  return list;
}

void
g_list_free (GList *list)
{
  while (list != NULL)
    {
      GList *next = list->next;
      g_free (list);
      list = next;
    }
}

void
g_list_foreach (GList *list, GFunc func, gpointer user_data)
{
  for (; list != NULL; list = list->next)
    func (list->data, user_data);
}

GList *
g_list_sort (GList *list, GCompareFunc compare_func)
{
  GList *result = NULL;

  while (list != NULL)
    {
      GList *cur = list;
      GList *walk;

      list = list->next;
      if (result == NULL || compare_func (cur->data, result->data) < 0)
        {
          cur->prev = NULL;
          cur->next = result;
          if (result != NULL)
            result->prev = cur;
          result = cur;
          continue;
        }
      walk = result;
      while (walk->next != NULL &&
          compare_func (cur->data, walk->next->data) >= 0)
        walk = walk->next;
      cur->next = walk->next;
      cur->prev = walk;
      if (walk->next != NULL)
        walk->next->prev = cur;
      walk->next = cur;
    }
  return result;
}

GQueue *
g_queue_new (void)
{
  return g_new0 (GQueue, 1);
}

void
g_queue_free (GQueue *queue)
{
  if (queue == NULL)
    return;
  g_list_free (queue->head);
  g_free (queue);
}

void
g_queue_push_tail (GQueue *queue, gpointer data)
{
  GList *node = g_new (GList, 1);

  node->data = data;
  node->next = NULL;
  node->prev = queue->tail;
  if (queue->tail != NULL)
    queue->tail->next = node;
  else
    queue->head = node;
  queue->tail = node;
  queue->length++;
}

gpointer
g_queue_pop_head (GQueue *queue)
{
  GList *node;
  gpointer data;

  if (queue->head == NULL)
    return NULL;
  node = queue->head;
  data = node->data;
  queue->head = node->next;
  if (queue->head != NULL)
    queue->head->prev = NULL;
  else
    queue->tail = NULL;
  queue->length--;
  g_free (node);
  return data;
}

gpointer
g_queue_pop_tail (GQueue *queue)
{
  GList *node;
  gpointer data;

  if (queue->tail == NULL)
    return NULL;
  node = queue->tail;
  data = node->data;
  queue->tail = node->prev;
  if (queue->tail != NULL)
    queue->tail->next = NULL;
  else
    queue->head = NULL;
  queue->length--;
  g_free (node);
  return data;
}

gboolean
g_queue_is_empty (GQueue *queue)
{
  return queue->head == NULL;
}

static void
string_ensure (GString *string, gsize extra)
{
  gsize need = string->len + extra + 1;
  gsize alloc;

  if (need <= string->allocated_len)
    return;
  alloc = string->allocated_len != 0 ? string->allocated_len : 16;
  while (alloc < need)
    alloc *= 2;
  string->str = g_realloc (string->str, alloc);
  string->allocated_len = alloc;
}

GString *
g_string_sized_new (gsize dfl_size)
{
  GString *string = g_new0 (GString, 1);

  string->allocated_len = dfl_size != 0 ? dfl_size : 16;
  string->str = g_malloc0 (string->allocated_len);
  return string;
}

GString *
g_string_new (const gchar *init)
{
  GString *string = g_string_sized_new (init != NULL ? strlen (init) + 1 : 16);

  if (init != NULL && init[0] != '\0')
    g_string_append (string, init);
  return string;
}

gchar *
g_string_free (GString *string, gboolean free_segment)
{
  gchar *str;

  if (string == NULL)
    return NULL;
  str = string->str;
  if (free_segment)
    {
      g_free (str);
      str = NULL;
    }
  g_free (string);
  return str;
}

GString *
g_string_append (GString *string, const gchar *val)
{
  gsize n;

  if (val == NULL || val[0] == '\0')
    return string;
  n = strlen (val);
  string_ensure (string, n);
  memcpy (string->str + string->len, val, n + 1);
  string->len += n;
  return string;
}

GString *
g_string_append_c (GString *string, gchar c)
{
  string_ensure (string, 1);
  string->str[string->len++] = c;
  string->str[string->len] = '\0';
  return string;
}

void
g_string_append_printf (GString *string, const gchar *format, ...)
{
  va_list ap;
  gchar *piece;

  va_start (ap, format);
  if (vasprintf (&piece, format, ap) < 0)
    oom ();
  va_end (ap);
  g_string_append (string, piece);
  free (piece);
}

typedef struct _GumHashEntry GumHashEntry;
struct _GumHashEntry {
  gpointer key;
  gpointer value;
  GumHashEntry *next;
};

struct _GHashTable {
  GumHashEntry **buckets;
  guint n_buckets;
  guint n_nodes;
  guint ref_count;
  GHashFunc hash_func;
  GEqualFunc key_equal_func;
  GDestroyNotify key_destroy_func;
  GDestroyNotify value_destroy_func;
};

static gboolean
hash_equal (GHashTable *hash, gconstpointer a, gconstpointer b)
{
  if (hash->key_equal_func != NULL)
    return hash->key_equal_func (a, b);
  return a == b;
}

static guint
hash_key (GHashTable *hash, gconstpointer key)
{
  if (hash->hash_func != NULL)
    return hash->hash_func (key);
  return g_direct_hash (key);
}

static void
hash_rehash (GHashTable *hash, guint n_buckets)
{
  GumHashEntry **buckets = g_new0 (GumHashEntry *, n_buckets);
  guint i;

  for (i = 0; i != hash->n_buckets; i++)
    {
      GumHashEntry *entry = hash->buckets[i];
      while (entry != NULL)
        {
          GumHashEntry *next = entry->next;
          guint bucket = hash_key (hash, entry->key) % n_buckets;
          entry->next = buckets[bucket];
          buckets[bucket] = entry;
          entry = next;
        }
    }
  g_free (hash->buckets);
  hash->buckets = buckets;
  hash->n_buckets = n_buckets;
}

static GHashTable *
hash_new (GHashFunc hash_func, GEqualFunc key_equal_func,
          GDestroyNotify key_destroy_func, GDestroyNotify value_destroy_func)
{
  GHashTable *hash = g_new0 (GHashTable, 1);

  hash->n_buckets = 8;
  hash->buckets = g_new0 (GumHashEntry *, hash->n_buckets);
  hash->ref_count = 1;
  hash->hash_func = hash_func;
  hash->key_equal_func = key_equal_func;
  hash->key_destroy_func = key_destroy_func;
  hash->value_destroy_func = value_destroy_func;
  return hash;
}

GHashTable *
g_hash_table_new (GHashFunc hash_func, GEqualFunc key_equal_func)
{
  return hash_new (hash_func, key_equal_func, NULL, NULL);
}

GHashTable *
g_hash_table_new_full (GHashFunc hash_func, GEqualFunc key_equal_func,
                       GDestroyNotify key_destroy_func,
                       GDestroyNotify value_destroy_func)
{
  return hash_new (hash_func, key_equal_func, key_destroy_func, value_destroy_func);
}

static GumHashEntry *
hash_find (GHashTable *hash, gconstpointer key, guint *bucket_out)
{
  guint bucket;
  GumHashEntry *entry;

  if (hash->n_buckets == 0)
    return NULL;
  bucket = hash_key (hash, key) % hash->n_buckets;
  if (bucket_out != NULL)
    *bucket_out = bucket;
  for (entry = hash->buckets[bucket]; entry != NULL; entry = entry->next)
    {
      if (hash_equal (hash, entry->key, key))
        return entry;
    }
  return NULL;
}

static void
hash_destroy_entry (GHashTable *hash, GumHashEntry *entry)
{
  if (hash->key_destroy_func != NULL)
    hash->key_destroy_func (entry->key);
  if (hash->value_destroy_func != NULL &&
      !(entry->key == entry->value && hash->key_destroy_func != NULL))
    hash->value_destroy_func (entry->value);
  g_free (entry);
}

gboolean
g_hash_table_insert (GHashTable *hash_table, gpointer key, gpointer value)
{
  guint bucket = 0;
  GumHashEntry *entry = hash_find (hash_table, key, &bucket);

  if (entry != NULL)
    {
      if (key != entry->key && hash_table->key_destroy_func != NULL)
        hash_table->key_destroy_func (key);
      if (hash_table->value_destroy_func != NULL && value != entry->value)
        hash_table->value_destroy_func (entry->value);
      entry->value = value;
      return FALSE;
    }
  if (hash_table->n_nodes + 1 > hash_table->n_buckets * 2)
    {
      hash_rehash (hash_table, hash_table->n_buckets * 2);
      bucket = hash_key (hash_table, key) % hash_table->n_buckets;
    }
  entry = g_new (GumHashEntry, 1);
  entry->key = key;
  entry->value = value;
  entry->next = hash_table->buckets[bucket];
  hash_table->buckets[bucket] = entry;
  hash_table->n_nodes++;
  return TRUE;
}

gboolean
g_hash_table_add (GHashTable *hash_table, gpointer key)
{
  if (g_hash_table_contains (hash_table, key))
    return FALSE;
  g_hash_table_insert (hash_table, key, key);
  return TRUE;
}

gboolean
g_hash_table_remove (GHashTable *hash_table, gconstpointer key)
{
  guint bucket = 0;
  GumHashEntry *prev = NULL;
  GumHashEntry *entry;

  if (hash_table->n_buckets == 0)
    return FALSE;
  bucket = hash_key (hash_table, key) % hash_table->n_buckets;
  for (entry = hash_table->buckets[bucket]; entry != NULL; entry = entry->next)
    {
      if (hash_equal (hash_table, entry->key, key))
        {
          if (prev != NULL)
            prev->next = entry->next;
          else
            hash_table->buckets[bucket] = entry->next;
          hash_table->n_nodes--;
          hash_destroy_entry (hash_table, entry);
          return TRUE;
        }
      prev = entry;
    }
  return FALSE;
}

void
g_hash_table_remove_all (GHashTable *hash_table)
{
  guint i;

  for (i = 0; i != hash_table->n_buckets; i++)
    {
      GumHashEntry *entry = hash_table->buckets[i];
      hash_table->buckets[i] = NULL;
      while (entry != NULL)
        {
          GumHashEntry *next = entry->next;
          hash_destroy_entry (hash_table, entry);
          entry = next;
        }
    }
  hash_table->n_nodes = 0;
}

static void
hash_free (GHashTable *hash_table)
{
  g_hash_table_remove_all (hash_table);
  g_free (hash_table->buckets);
  g_free (hash_table);
}

void
g_hash_table_unref (GHashTable *hash_table)
{
  if (hash_table == NULL)
    return;
  if (__atomic_sub_fetch (&hash_table->ref_count, 1, __ATOMIC_ACQ_REL) == 0)
    hash_free (hash_table);
}

gpointer
g_hash_table_lookup (GHashTable *hash_table, gconstpointer key)
{
  GumHashEntry *entry = hash_find (hash_table, key, NULL);

  return entry != NULL ? entry->value : NULL;
}

gboolean
g_hash_table_contains (GHashTable *hash_table, gconstpointer key)
{
  return hash_find (hash_table, key, NULL) != NULL;
}

guint
g_hash_table_size (GHashTable *hash_table)
{
  return hash_table->n_nodes;
}

GList *
g_hash_table_get_keys (GHashTable *hash_table)
{
  GList *keys = NULL;
  guint i;

  for (i = 0; i != hash_table->n_buckets; i++)
    {
      GumHashEntry *entry;
      for (entry = hash_table->buckets[i]; entry != NULL; entry = entry->next)
        {
          GList *node = g_new (GList, 1);
          node->data = entry->key;
          node->prev = NULL;
          node->next = keys;
          if (keys != NULL)
            keys->prev = node;
          keys = node;
        }
    }
  return keys;
}

void
g_hash_table_iter_init (GHashTableIter *iter, GHashTable *hash_table)
{
  guint i;

  memset (iter, 0, sizeof (*iter));
  iter->p0 = hash_table;
  for (i = 0; i != hash_table->n_buckets; i++)
    {
      if (hash_table->buckets[i] != NULL)
        {
          iter->p2 = hash_table->buckets[i];
          iter->i0 = (int) i;
          return;
        }
    }
}

gboolean
g_hash_table_iter_next (GHashTableIter *iter, gpointer *key, gpointer *value)
{
  GHashTable *hash = iter->p0;
  GumHashEntry *entry = iter->p2;
  guint bucket;
  GumHashEntry *next;

  if (entry == NULL)
    return FALSE;
  iter->p1 = entry;
  next = entry->next;
  bucket = (guint) iter->i0;
  if (next == NULL)
    {
      bucket++;
      while (bucket < hash->n_buckets && hash->buckets[bucket] == NULL)
        bucket++;
      next = bucket < hash->n_buckets ? hash->buckets[bucket] : NULL;
    }
  iter->p2 = next;
  iter->i0 = (int) bucket;
  if (key != NULL)
    *key = entry->key;
  if (value != NULL)
    *value = entry->value;
  return TRUE;
}

void
g_hash_table_iter_remove (GHashTableIter *iter)
{
  GHashTable *hash = iter->p0;
  GumHashEntry *entry = iter->p1;
  guint bucket;
  GumHashEntry *prev = NULL;
  GumHashEntry *cur;

  if (entry == NULL || hash->n_buckets == 0)
    return;
  bucket = hash_key (hash, entry->key) % hash->n_buckets;
  for (cur = hash->buckets[bucket]; cur != NULL; cur = cur->next)
    {
      if (cur == entry)
        {
          if (prev != NULL)
            prev->next = cur->next;
          else
            hash->buckets[bucket] = cur->next;
          hash->n_nodes--;
          hash_destroy_entry (hash, cur);
          iter->p1 = NULL;
          return;
        }
      prev = cur;
    }
}

struct _GBytes {
  gconstpointer data;
  gsize size;
  volatile gint ref_count;
  GDestroyNotify notify;
  gpointer user_data;
};

static GBytes *
bytes_new (gconstpointer data, gsize size, GDestroyNotify notify, gpointer user_data)
{
  GBytes *bytes = g_new0 (GBytes, 1);

  bytes->data = data;
  bytes->size = size;
  bytes->ref_count = 1;
  bytes->notify = notify;
  bytes->user_data = user_data;
  return bytes;
}

GBytes *
g_bytes_new_static (gconstpointer data, gsize size)
{
  return bytes_new (data, size, NULL, NULL);
}

GBytes *
g_bytes_new_take (gpointer data, gsize size)
{
  return bytes_new (data, size, g_free, data);
}

GBytes *
g_bytes_ref (GBytes *bytes)
{
  if (bytes != NULL)
    __atomic_fetch_add (&bytes->ref_count, 1, __ATOMIC_RELAXED);
  return bytes;
}

void
g_bytes_unref (GBytes *bytes)
{
  if (bytes == NULL)
    return;
  if (__atomic_sub_fetch (&bytes->ref_count, 1, __ATOMIC_ACQ_REL) == 0)
    {
      if (bytes->notify != NULL)
        bytes->notify (bytes->user_data);
      g_free (bytes);
    }
}

gconstpointer
g_bytes_get_data (GBytes *bytes, gsize *size)
{
  if (size != NULL)
    *size = bytes != NULL ? bytes->size : 0;
  return bytes != NULL ? bytes->data : NULL;
}

struct _GMappedFile {
  gpointer data;
  gsize length;
  volatile gint ref_count;
};

GMappedFile *
g_mapped_file_new (const gchar *filename, gboolean writable, GError **error)
{
  FILE *file;
  int fd;
  struct stat st;
  GMappedFile *mapped;
  int prot = writable ? PROT_READ | PROT_WRITE : PROT_READ;

  file = fopen (filename, writable ? "rb+" : "rb");
  if (file == NULL)
    {
      g_set_error (error, G_FILE_ERROR,
          errno == ENOENT ? G_FILE_ERROR_NOENT :
          (errno == EACCES ? G_FILE_ERROR_ACCES : G_FILE_ERROR_FAILED),
          "%s", g_strerror (errno));
      return NULL;
    }
  fd = fileno (file);
  if (fstat (fd, &st) != 0 || st.st_size < 0)
    {
      fclose (file);
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "stat failed");
      return NULL;
    }
  mapped = g_new0 (GMappedFile, 1);
  mapped->length = (gsize) st.st_size;
  mapped->ref_count = 1;
  if (mapped->length == 0)
    {
      mapped->data = g_malloc (1);
      fclose (file);
      return mapped;
    }
  mapped->data = mmap (NULL, mapped->length, prot, MAP_PRIVATE, fd, 0);
  fclose (file);
  if (mapped->data == MAP_FAILED)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "mmap failed");
      g_free (mapped);
      return NULL;
    }
  return mapped;
}

static void
mapped_file_unref_notify (gpointer data)
{
  g_mapped_file_unref (data);
}

GBytes *
g_mapped_file_get_bytes (GMappedFile *file)
{
  __atomic_fetch_add (&file->ref_count, 1, __ATOMIC_RELAXED);
  return bytes_new (file->data, file->length, mapped_file_unref_notify, file);
}

void
g_mapped_file_unref (GMappedFile *file)
{
  if (file == NULL)
    return;
  if (__atomic_sub_fetch (&file->ref_count, 1, __ATOMIC_ACQ_REL) != 0)
    return;
  if (file->length == 0)
    g_free (file->data);
  else if (file->data != NULL && file->data != MAP_FAILED)
    munmap (file->data, file->length);
  g_free (file);
}

struct _GRegex {
  regex_t re;
  volatile gint ref_count;
  gboolean notempty;
};

struct _GMatchInfo {
  GRegex *regex;
  const gchar *string;
  gint length;
  gint start_pos;
  gint end_pos;
  gboolean matched;
  GRegexMatchFlags flags;
  gchar *span;
  gint span_base;
  gint span_len;
};

GRegex *
g_regex_new (const gchar *pattern, GRegexCompileFlags compile_options,
             GRegexMatchFlags match_options, GError **error)
{
  GRegex *regex;
  int cflags = REG_EXTENDED;

  (void) error;
  if (pattern == NULL)
    return NULL;
  regex = g_new0 (GRegex, 1);
  if (compile_options & G_REGEX_MULTILINE)
    cflags |= REG_NEWLINE;
  if (regcomp (&regex->re, pattern, cflags) != 0)
    {
      g_free (regex);
      return NULL;
    }
  regex->ref_count = 1;
  regex->notempty = (match_options & G_REGEX_MATCH_NOTEMPTY) != 0;
  return regex;
}

void
g_regex_unref (GRegex *regex)
{
  if (regex == NULL)
    return;
  if (__atomic_sub_fetch (&regex->ref_count, 1, __ATOMIC_ACQ_REL) == 0)
    {
      regfree (&regex->re);
      g_free (regex);
    }
}

static gboolean
regex_find (GMatchInfo *info, gint from)
{
  gboolean skip_empty;

  if (from < 0)
    from = 0;
  skip_empty = info->regex->notempty ||
      (info->flags & G_REGEX_MATCH_NOTEMPTY) != 0;
  while (from < info->length)
    {
      gint span_end = from;
      const gchar *subject;
      gint base;
      int eflags = 0;
      regmatch_t match;

      while (span_end < info->length && info->string[span_end] != '\0')
        span_end++;
      if (span_end == from)
        {
          from++;
          continue;
        }

      if (info->span == NULL || from < info->span_base ||
          from >= info->span_base + info->span_len)
        {
          g_free (info->span);
          info->span_base = from;
          info->span_len = span_end - from;
          info->span = g_malloc ((gsize) info->span_len + 1);
          memcpy (info->span, info->string + from, (gsize) info->span_len);
          info->span[info->span_len] = '\0';
        }

      base = info->span_base;
      subject = info->span + (from - base);
      if (from != 0)
        eflags |= REG_NOTBOL;
      if (base + info->span_len != info->length)
        eflags |= REG_NOTEOL;
      if (regexec (&info->regex->re, subject, 1, &match, eflags) == 0)
        {
          gint abs_start = from + (gint) match.rm_so;
          gint abs_end = from + (gint) match.rm_eo;

          if (skip_empty && abs_start == abs_end)
            {
              from = abs_start + 1;
              continue;
            }
          info->start_pos = abs_start;
          info->end_pos = abs_end;
          info->matched = TRUE;
          return TRUE;
        }
      from = base + info->span_len;
      if (from < info->length)
        from++;
    }
  info->matched = FALSE;
  return FALSE;
}

gboolean
g_regex_match_full (const GRegex *regex, const gchar *string, gssize string_len,
                    gint start_position, GRegexMatchFlags match_options,
                    GMatchInfo **match_info, GError **error)
{
  GMatchInfo *info;

  (void) error;
  if (string_len < 0)
    string_len = string != NULL ? (gssize) strlen (string) : 0;
  info = g_new0 (GMatchInfo, 1);
  info->regex = (GRegex *) regex;
  __atomic_fetch_add (&info->regex->ref_count, 1, __ATOMIC_RELAXED);
  info->string = string;
  info->length = (gint) string_len;
  info->flags = match_options;
  regex_find (info, start_position);
  if (match_info != NULL)
    *match_info = info;
  else
    {
      gboolean matched = info->matched;
      g_match_info_free (info);
      return matched;
    }
  return info->matched;
}

gboolean
g_regex_match (const GRegex *regex, const gchar *string,
               GRegexMatchFlags match_options, GMatchInfo **match_info)
{
  return g_regex_match_full (regex, string, -1, 0, match_options, match_info, NULL);
}

gboolean
g_match_info_matches (const GMatchInfo *match_info)
{
  return match_info != NULL && match_info->matched;
}

gboolean
g_match_info_next (GMatchInfo *match_info, GError **error)
{
  gint from;

  (void) error;
  if (match_info == NULL || !match_info->matched)
    return FALSE;
  from = match_info->end_pos;
  if (from == match_info->start_pos)
    from++;
  return regex_find (match_info, from);
}

gboolean
g_match_info_fetch_pos (const GMatchInfo *match_info, gint match_num,
                        gint *start_pos, gint *end_pos)
{
  if (match_info == NULL || !match_info->matched || match_num != 0)
    return FALSE;
  if (start_pos != NULL)
    *start_pos = match_info->start_pos;
  if (end_pos != NULL)
    *end_pos = match_info->end_pos;
  return TRUE;
}

void
g_match_info_free (GMatchInfo *match_info)
{
  if (match_info == NULL)
    return;
  g_regex_unref (match_info->regex);
  g_free (match_info->span);
  g_free (match_info);
}

gboolean
g_path_is_absolute (const gchar *file_name)
{
  return file_name != NULL && file_name[0] == '/';
}

gchar *
g_path_get_dirname (const gchar *file_name)
{
  gsize len;
  gchar *copy;
  gchar *slash;

  if (file_name == NULL || file_name[0] == '\0')
    return g_strdup (".");
  copy = g_strdup (file_name);
  len = strlen (copy);
  while (len > 1 && copy[len - 1] == '/')
    copy[--len] = '\0';
  slash = strrchr (copy, '/');
  if (slash == NULL)
    {
      g_free (copy);
      return g_strdup (".");
    }
  if (slash == copy)
    slash[1] = '\0';
  else
    *slash = '\0';
  return copy;
}

gchar *
g_canonicalize_filename (const gchar *filename, const gchar *relative_to)
{
  gchar *base = NULL;
  gchar *joined;
  gchar *normalized;
  gchar **parts;
  gchar **out;
  guint n = 0;
  guint i;
  GString *built;

  if (filename == NULL)
    return NULL;
  if (g_path_is_absolute (filename))
    {
      joined = g_strdup (filename);
    }
  else
    {
      if (relative_to != NULL && g_path_is_absolute (relative_to))
        base = g_strdup (relative_to);
      else
        {
          gchar cwd[PATH_MAX];
          if (getcwd (cwd, sizeof (cwd)) == NULL)
            strcpy (cwd, "/");
          if (relative_to != NULL && relative_to[0] != '\0')
            {
              base = g_strdup_printf ("%s/%s", cwd, relative_to);
            }
          else
            base = g_strdup (cwd);
        }
      joined = g_strdup_printf ("%s/%s", base, filename);
      g_free (base);
    }

  parts = g_strsplit (joined, "/", 0);
  g_free (joined);
  out = g_new0 (gchar *, g_strv_length (parts) + 1);
  for (i = 0; parts[i] != NULL; i++)
    {
      if (parts[i][0] == '\0' || strcmp (parts[i], ".") == 0)
        continue;
      if (strcmp (parts[i], "..") == 0)
        {
          if (n > 0)
            n--;
          continue;
        }
      out[n++] = parts[i];
    }
  built = g_string_new ("/");
  for (i = 0; i != n; i++)
    {
      if (i != 0)
        g_string_append_c (built, '/');
      g_string_append (built, out[i]);
    }
  g_strfreev (parts);
  g_free (out);
  normalized = g_string_free (built, FALSE);
  return normalized;
}

gboolean
g_file_test (const gchar *filename, GFileTest test)
{
  (void) test;
  return filename != NULL && access (filename, F_OK) == 0;
}

static gint
file_error_from_errno (gint err)
{
  if (err == ENOENT)
    return G_FILE_ERROR_NOENT;
  if (err == EACCES || err == EPERM)
    return G_FILE_ERROR_ACCES;
  return G_FILE_ERROR_FAILED;
}

gboolean
g_file_get_contents (const gchar *filename, gchar **contents, gsize *length,
                     GError **error)
{
  FILE *file;
  gchar *buf;
  gsize len = 0;
  gsize alloc = 4096;

  file = fopen (filename, "rb");
  if (file == NULL)
    {
      g_set_error (error, G_FILE_ERROR, file_error_from_errno (errno),
          "%s", g_strerror (errno));
      return FALSE;
    }
  buf = g_malloc (alloc);
  for (;;)
    {
      size_t n;

      if (len + 4096 + 1 > alloc)
        {
          alloc *= 2;
          buf = g_realloc (buf, alloc);
        }
      n = fread (buf + len, 1, 4096, file);
      len += n;
      if (n < 4096)
        {
          if (ferror (file))
            {
              fclose (file);
              g_free (buf);
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "read failed");
              return FALSE;
            }
          break;
        }
    }
  fclose (file);
  buf[len] = '\0';
  *contents = buf;
  if (length != NULL)
    *length = len;
  return TRUE;
}

gchar *
g_file_read_link (const gchar *filename, GError **error)
{
  gchar buf[PATH_MAX];
  ssize_t n;

  n = readlink (filename, buf, sizeof (buf) - 1);
  if (n < 0)
    {
      g_set_error (error, G_FILE_ERROR, file_error_from_errno (errno),
          "%s", g_strerror (errno));
      return NULL;
    }
  buf[n] = '\0';
  return g_strdup (buf);
}

struct _GDir {
  DIR *dir;
};

GDir *
g_dir_open (const gchar *path, guint flags, GError **error)
{
  GDir *dir;

  (void) flags;
  dir = g_new0 (GDir, 1);
  dir->dir = opendir (path);
  if (dir->dir == NULL)
    {
      g_set_error (error, G_FILE_ERROR, file_error_from_errno (errno),
          "%s", g_strerror (errno));
      g_free (dir);
      return NULL;
    }
  return dir;
}

const gchar *
g_dir_read_name (GDir *dir)
{
  struct dirent *ent;

  if (dir == NULL)
    return NULL;
  for (;;)
    {
      ent = readdir (dir->dir);
      if (ent == NULL)
        return NULL;
      if (strcmp (ent->d_name, ".") == 0 || strcmp (ent->d_name, "..") == 0)
        continue;
      return ent->d_name;
    }
}

void
g_dir_close (GDir *dir)
{
  if (dir == NULL)
    return;
  if (dir->dir != NULL)
    closedir (dir->dir);
  g_free (dir);
}

void
g_set_error (GError **err, GQuark domain, gint code, const gchar *format, ...)
{
  GError *error;
  va_list ap;

  if (err == NULL || *err != NULL)
    return;
  error = g_new0 (GError, 1);
  error->domain = domain;
  error->code = code;
  va_start (ap, format);
  if (vasprintf (&error->message, format, ap) < 0)
    oom ();
  va_end (ap);
  *err = error;
}

gboolean
g_error_matches (const GError *error, GQuark domain, gint code)
{
  return error != NULL && error->domain == domain && error->code == code;
}

void
g_error_free (GError *error)
{
  if (error == NULL)
    return;
  g_free (error->message);
  g_free (error);
}

gint64
g_get_monotonic_time (void)
{
  struct timespec ts;

  clock_gettime (CLOCK_MONOTONIC, &ts);
  return (gint64) ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

struct _GThread {
  pthread_t thread;
  GThreadFunc func;
  gpointer data;
  gpointer result;
  gchar name[16];
};

static void *
thread_main (void *arg)
{
  GThread *thread = arg;

  if (thread->name[0] != '\0')
    pthread_setname_np (thread->thread, thread->name);
  thread->result = thread->func (thread->data);
  return NULL;
}

GThread *
g_thread_new (const gchar *name, GThreadFunc func, gpointer data)
{
  GThread *thread = g_new0 (GThread, 1);

  thread->func = func;
  thread->data = data;
  if (name != NULL)
    {
      strncpy (thread->name, name, sizeof (thread->name) - 1);
      thread->name[sizeof (thread->name) - 1] = '\0';
    }
  if (pthread_create (&thread->thread, NULL, thread_main, thread) != 0)
    oom ();
  return thread;
}

gpointer
g_thread_join (GThread *thread)
{
  gpointer result;

  pthread_join (thread->thread, NULL);
  result = thread->result;
  g_free (thread);
  return result;
}

void
g_thread_yield (void)
{
  sched_yield ();
}

gpointer
g_once (GOnce *once, GThreadFunc func, gpointer arg)
{
  gint zero = 0;

  if (__atomic_load_n (&once->status, __ATOMIC_ACQUIRE) == 2)
    return once->retval;
  if (__atomic_compare_exchange_n ((gint *) &once->status, &zero, 1, 0,
          __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
    {
      once->retval = func (arg);
      __atomic_store_n (&once->status, 2, __ATOMIC_RELEASE);
      return once->retval;
    }
  while (__atomic_load_n (&once->status, __ATOMIC_ACQUIRE) != 2)
    ;
  return once->retval;
}

static void
private_ensure (GPrivate *key)
{
  if (g_once_init_enter (&key->ready))
    {
      pthread_key_create (&key->key, key->notify);
      g_once_init_leave (&key->ready, 1);
    }
}

gpointer
g_private_get (GPrivate *key)
{
  private_ensure (key);
  return pthread_getspecific (key->key);
}

void
g_private_set (GPrivate *key, gpointer value)
{
  gpointer old;

  private_ensure (key);
  old = pthread_getspecific (key->key);
  if (old != NULL && old != value && key->notify != NULL)
    key->notify (old);
  pthread_setspecific (key->key, value);
}

void
g_mutex_init (GMutex *mutex)
{
  pthread_mutex_init (mutex, NULL);
}

void
g_mutex_clear (GMutex *mutex)
{
  pthread_mutex_destroy (mutex);
}

void
g_mutex_lock (GMutex *mutex)
{
  pthread_mutex_lock (mutex);
}

void
g_mutex_unlock (GMutex *mutex)
{
  pthread_mutex_unlock (mutex);
}

void
g_rec_mutex_init (GRecMutex *rec_mutex)
{
  pthread_mutexattr_t attr;

  pthread_mutexattr_init (&attr);
  pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init (rec_mutex, &attr);
  pthread_mutexattr_destroy (&attr);
}

void
g_rec_mutex_clear (GRecMutex *rec_mutex)
{
  pthread_mutex_destroy (rec_mutex);
}

void
g_rec_mutex_lock (GRecMutex *rec_mutex)
{
  pthread_mutex_lock (rec_mutex);
}

gboolean
g_rec_mutex_trylock (GRecMutex *rec_mutex)
{
  return pthread_mutex_trylock (rec_mutex) == 0;
}

void
g_rec_mutex_unlock (GRecMutex *rec_mutex)
{
  pthread_mutex_unlock (rec_mutex);
}

void
g_cond_init (GCond *cond)
{
  pthread_cond_init (cond, NULL);
}

void
g_cond_clear (GCond *cond)
{
  pthread_cond_destroy (cond);
}

void
g_cond_wait (GCond *cond, GMutex *mutex)
{
  pthread_cond_wait (cond, mutex);
}

void
g_cond_signal (GCond *cond)
{
  pthread_cond_signal (cond);
}
