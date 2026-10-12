/*
 * Small GLib/GObject subset used by the ARM64 Gum sources.
 * Type-name strings passed to registration are unused, so -O2 drops them.
 */
#ifndef __GLIB_H__
#define __GLIB_H__

#include <alloca.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifdef __cplusplus
# define G_BEGIN_DECLS extern "C" {
# define G_END_DECLS }
#else
# define G_BEGIN_DECLS
# define G_END_DECLS
#endif

G_BEGIN_DECLS

typedef int gboolean;
typedef char gchar;
typedef unsigned char guchar;
typedef uint8_t guint8;
typedef int gint;
typedef unsigned int guint;
typedef int16_t gint16;
typedef uint16_t guint16;
typedef int32_t gint32;
typedef uint32_t guint32;
typedef int64_t gint64;
typedef uint64_t guint64;
typedef float gfloat;
typedef double gdouble;
typedef size_t gsize;
typedef ssize_t gssize;
typedef intptr_t gintptr;
typedef uintptr_t guintptr;
typedef gint64 goffset;
typedef unsigned long gulong;
typedef guint32 gunichar;
typedef void *gpointer;
typedef const void *gconstpointer;
typedef gsize GType;
typedef guint GQuark;

typedef struct _GError GError;
typedef struct _GArray GArray;
typedef struct _GPtrArray GPtrArray;
typedef struct _GList GList;
typedef struct _GSList GSList;
typedef struct _GQueue GQueue;
typedef struct _GString GString;
typedef struct _GHashTable GHashTable;
typedef struct _GHashTableIter GHashTableIter;
typedef struct _GBytes GBytes;
typedef struct _GMappedFile GMappedFile;
typedef struct _GRegex GRegex;
typedef struct _GMatchInfo GMatchInfo;
typedef struct _GDir GDir;
typedef struct _GThread GThread;
typedef struct _GObject GObject;
typedef struct _GObjectClass GObjectClass;
typedef struct _GTypeClass GTypeClass;
typedef struct _GTypeInstance GTypeInstance;
typedef struct _GTypeInterface GTypeInterface;
typedef struct _GValue GValue;
typedef struct _GParamSpec GParamSpec;

typedef gint (*GCompareFunc) (gconstpointer a, gconstpointer b);
typedef gint (*GCompareDataFunc) (gconstpointer a, gconstpointer b, gpointer user_data);
typedef guint (*GHashFunc) (gconstpointer key);
typedef gboolean (*GEqualFunc) (gconstpointer a, gconstpointer b);
typedef void (*GDestroyNotify) (gpointer data);
typedef void (*GFunc) (gpointer data, gpointer user_data);
typedef void (*GHFunc) (gpointer key, gpointer value, gpointer user_data);
typedef gboolean (*GHRFunc) (gpointer key, gpointer value, gpointer user_data);
typedef gpointer (*GThreadFunc) (gpointer data);
typedef void (*GWeakNotify) (gpointer data, GObject *where_the_object_was);
typedef void (*GClassInitFunc) (gpointer g_class, gpointer class_data);
typedef void (*GInstanceInitFunc) (GTypeInstance *instance, gpointer g_class);
typedef void (*GInterfaceInitFunc) (gpointer g_iface, gpointer iface_data);
typedef gpointer (*GBoxedCopyFunc) (gpointer boxed);
typedef void (*GBoxedFreeFunc) (gpointer boxed);
typedef void (*GCallback) (void);

typedef enum {
  G_TYPE_FLAG_NONE = 0
} GTypeFlags;

typedef struct {
  GInterfaceInitFunc interface_init;
  void (*interface_finalize) (gpointer g_iface, gpointer iface_data);
  gpointer interface_data;
} GInterfaceInfo;

typedef struct {
  gint value;
  const gchar *value_name;
  const gchar *value_nick;
} GEnumValue;

struct _GError {
  GQuark domain;
  gint code;
  gchar *message;
};

struct _GArray {
  gchar *data;
  guint len;
  guint alloc;
  guint elt_size;
  guint ref_count;
  guint zero_terminated : 1;
  guint clear : 1;
  GDestroyNotify clear_func;
};

struct _GPtrArray {
  gpointer *pdata;
  guint len;
  guint alloc;
  guint ref_count;
  GDestroyNotify element_free_func;
};

struct _GSList {
  gpointer data;
  GSList *next;
};

struct _GList {
  gpointer data;
  GList *next;
  GList *prev;
};

struct _GQueue {
  GList *head;
  GList *tail;
  guint length;
};

struct _GString {
  gchar *str;
  gsize len;
  gsize allocated_len;
};

struct _GHashTableIter {
  gpointer p0;
  gpointer p1;
  gpointer p2;
  int i0;
  int i1;
};

struct _GTypeClass {
  GType g_type;
};

struct _GTypeInstance {
  GTypeClass *g_class;
};

struct _GTypeInterface {
  GType g_type;
  GType g_instance_type;
};

struct _GObject {
  GTypeInstance g_type_instance;
  volatile gint ref_count;
};

typedef enum {
  GUM_PROP_INT = 1,
  GUM_PROP_UINT,
  GUM_PROP_ENUM,
  GUM_PROP_UINT64,
  GUM_PROP_STRING,
  GUM_PROP_BOXED
} GumPropKind;

#define GUM_RT_MAX_PROPS 24
#define GUM_RT_MAX_IFACES 8

struct _GObjectClass {
  GTypeClass g_type_class;
  void (*dispose) (GObject *object);
  void (*finalize) (GObject *object);
  void (*set_property) (GObject *object, guint property_id, const GValue *value,
      GParamSpec *pspec);
  void (*get_property) (GObject *object, guint property_id, GValue *value,
      GParamSpec *pspec);

  GType iface_type[GUM_RT_MAX_IFACES];
  gpointer iface_vt[GUM_RT_MAX_IFACES];
  struct {
    guint id;
    gchar *name;
    GumPropKind kind;
    GType value_type;
    GParamSpec *pspec;
  } props[GUM_RT_MAX_PROPS];
  guint n_props;
};

struct _GParamSpec {
  gchar *name;
  GumPropKind kind;
  GType value_type;
};

struct _GValue {
  GType g_type;
  GumPropKind kind;
  union {
    gint v_int;
    guint v_uint;
    guint64 v_uint64;
    gpointer v_pointer;
  } data;
};

typedef pthread_mutex_t GMutex;
typedef pthread_mutex_t GRecMutex;
typedef pthread_cond_t GCond;

typedef struct {
  pthread_key_t key;
  GDestroyNotify notify;
  gsize ready;
} GPrivate;

typedef struct {
  volatile gint status;
  gpointer retval;
} GOnce;

#define TRUE 1
#define FALSE 0

#define G_GNUC_CONST __attribute__ ((__const__))
#define G_GNUC_UNUSED __attribute__ ((__unused__))
#define G_GNUC_NO_INLINE __attribute__ ((__noinline__))
#define G_GNUC_INTERNAL __attribute__ ((visibility ("hidden")))
#define G_NORETURN __attribute__ ((__noreturn__))
#define G_ANALYZER_NORETURN
#define G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#define G_GNUC_END_IGNORE_DEPRECATIONS

#define G_LIKELY(expr) (__builtin_expect (!!(expr), 1))
#define G_UNLIKELY(expr) (__builtin_expect (!!(expr), 0))
#define MIN(a, b) \
  ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
#define MAX(a, b) \
  ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#define ABS(a) \
  ({ __typeof__ (a) _a = (a); _a < 0 ? -_a : _a; })
#if defined (__LP64__) || defined (_WIN64)
# define GLIB_SIZEOF_VOID_P 8
#else
# define GLIB_SIZEOF_VOID_P 4
#endif

#define G_STMT_START do
#define G_STMT_END while (0)
#define G_PASTE_INTERNAL(a, b) a##b
#define G_PASTE(a, b) G_PASTE_INTERNAL (a, b)
#define G_STATIC_ASSERT(expr) _Static_assert ((expr), "static assert")
#define G_N_ELEMENTS(arr) (sizeof (arr) / sizeof ((arr)[0]))
#define G_STRUCT_OFFSET(struct_type, member) offsetof (struct_type, member)

#define G_LITTLE_ENDIAN 1234
#define G_BIG_ENDIAN 4321
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# define G_BYTE_ORDER G_BIG_ENDIAN
#else
# define G_BYTE_ORDER G_LITTLE_ENDIAN
#endif

#define GUINT16_SWAP_LE_BE(val) ((guint16) __builtin_bswap16 ((guint16) (val)))
#define GUINT32_SWAP_LE_BE(val) ((guint32) __builtin_bswap32 ((guint32) (val)))
#define GUINT64_SWAP_LE_BE(val) ((guint64) __builtin_bswap64 ((guint64) (val)))
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
# define GUINT16_TO_LE(val) ((guint16) (val))
# define GUINT32_TO_LE(val) ((guint32) (val))
# define GUINT64_TO_LE(val) ((guint64) (val))
# define GUINT16_TO_BE(val) (GUINT16_SWAP_LE_BE (val))
# define GUINT32_TO_BE(val) (GUINT32_SWAP_LE_BE (val))
# define GUINT64_TO_BE(val) (GUINT64_SWAP_LE_BE (val))
#else
# define GUINT16_TO_LE(val) (GUINT16_SWAP_LE_BE (val))
# define GUINT32_TO_LE(val) (GUINT32_SWAP_LE_BE (val))
# define GUINT64_TO_LE(val) (GUINT64_SWAP_LE_BE (val))
# define GUINT16_TO_BE(val) ((guint16) (val))
# define GUINT32_TO_BE(val) ((guint32) (val))
# define GUINT64_TO_BE(val) ((guint64) (val))
#endif
#define GUINT16_FROM_LE(val) (GUINT16_TO_LE (val))
#define GUINT32_FROM_LE(val) (GUINT32_TO_LE (val))
#define GUINT64_FROM_LE(val) (GUINT64_TO_LE (val))
#define GUINT16_FROM_BE(val) (GUINT16_TO_BE (val))
#define GUINT32_FROM_BE(val) (GUINT32_TO_BE (val))
#define GUINT64_FROM_BE(val) (GUINT64_TO_BE (val))
#define GINT16_TO_LE(val) ((gint16) GUINT16_TO_LE (val))
#define GINT32_TO_LE(val) ((gint32) GUINT32_TO_LE (val))
#define GINT64_TO_LE(val) ((gint64) GUINT64_TO_LE (val))
#define GINT16_TO_BE(val) ((gint16) GUINT16_TO_BE (val))
#define GINT32_TO_BE(val) ((gint32) GUINT32_TO_BE (val))
#define GINT64_TO_BE(val) ((gint64) GUINT64_TO_BE (val))
#define GINT16_FROM_LE(val) (GINT16_TO_LE (val))
#define GINT32_FROM_LE(val) (GINT32_TO_LE (val))
#define GINT64_FROM_LE(val) (GINT64_TO_LE (val))
#define GINT16_FROM_BE(val) (GINT16_TO_BE (val))
#define GINT32_FROM_BE(val) (GINT32_TO_BE (val))
#define GINT64_FROM_BE(val) (GINT64_TO_BE (val))

#define G_MININT32 ((gint32) 0x80000000)
#define G_MAXINT32 ((gint32) 0x7fffffff)
#define G_MAXUINT8 ((guint8) 0xff)
#define G_MAXUINT16 ((guint16) 0xffff)
#define G_MAXUINT32 ((guint32) 0xffffffff)
#define G_MAXUINT64 ((guint64) 0xffffffffffffffffULL)
#define G_MAXUINT ((guint) ~((guint) 0))
#define G_MAXSIZE ((gsize) ~((gsize) 0))

#define G_GINT64_CONSTANT(val) (G_PASTE (val, LL))
#define G_GUINT64_CONSTANT(val) (G_PASTE (val, ULL))
#if defined(__LP64__)
# define G_GINT64_MODIFIER "l"
#else
# define G_GINT64_MODIFIER "ll"
#endif
#define G_GSIZE_MODIFIER "z"
#define G_GSIZE_FORMAT "zu"

#define GINT_TO_POINTER(i) ((gpointer) (gintptr) (gint) (i))
#define GUINT_TO_POINTER(u) ((gpointer) (guintptr) (guint) (u))
#define GPOINTER_TO_INT(p) ((gint) (gintptr) (p))
#define GPOINTER_TO_UINT(p) ((guint) (guintptr) (p))
#define GPOINTER_TO_SIZE(p) ((gsize) (guintptr) (p))
#define GSIZE_TO_POINTER(s) ((gpointer) (guintptr) (s))

#define g_new(struct_type, n) ((struct_type *) g_malloc (sizeof (struct_type) * (gsize) (n)))
#define g_new0(struct_type, n) ((struct_type *) g_malloc0 (sizeof (struct_type) * (gsize) (n)))
#define g_newa(struct_type, n) ((struct_type *) alloca (sizeof (struct_type) * (gsize) (n)))
#define g_alloca(size) alloca (size)
#define g_slice_new(struct_type) ((struct_type *) g_slice_alloc (sizeof (struct_type)))
#define g_slice_new0(struct_type) ((struct_type *) g_slice_alloc0 (sizeof (struct_type)))
#define g_slice_free(struct_type, mem) g_slice_free1 (sizeof (struct_type), (mem))
#define g_slice_dup(struct_type, mem) ((struct_type *) g_memdup ((mem), sizeof (struct_type)))

#define g_steal_pointer(pp) \
  ((__typeof__ (*(pp))) gum_rt_steal_pointer ((pp)))
#define g_clear_pointer(pp, destroy) \
  G_STMT_START { \
    __typeof__ ((pp)) _pp = (pp); \
    __typeof__ (*_pp) _p = *_pp; \
    if (_p != NULL) { \
      *_pp = NULL; \
      (destroy) (_p); \
    } \
  } G_STMT_END
#define g_clear_object(opp) g_clear_pointer ((opp), g_object_unref)
#define g_clear_error(err) g_clear_pointer ((err), g_error_free)

#define g_return_if_fail(expr) \
  G_STMT_START { if (!(expr)) return; } G_STMT_END
#define g_return_val_if_fail(expr, val) \
  G_STMT_START { if (!(expr)) return (val); } G_STMT_END
#define g_assert(expr) \
  G_STMT_START { if (!(expr)) abort (); } G_STMT_END
#define g_assert_not_reached() abort ()

#define g_atomic_int_get(atomic) \
  __atomic_load_n ((atomic), __ATOMIC_ACQUIRE)
#define g_atomic_int_set(atomic, newval) \
  __atomic_store_n ((atomic), (newval), __ATOMIC_RELEASE)
#define g_atomic_int_inc(atomic) \
  ((void) __atomic_add_fetch ((atomic), 1, __ATOMIC_ACQ_REL))
#define g_atomic_int_add(atomic, val) \
  __atomic_fetch_add ((atomic), (val), __ATOMIC_ACQ_REL)
#define g_atomic_int_dec_and_test(atomic) \
  (__atomic_sub_fetch ((atomic), 1, __ATOMIC_ACQ_REL) == 0)
#define g_atomic_pointer_get(atomic) \
  __atomic_load_n ((atomic), __ATOMIC_ACQUIRE)
#define g_atomic_pointer_set(atomic, newval) \
  __atomic_store_n ((atomic), (newval), __ATOMIC_RELEASE)

static inline gboolean
g_atomic_int_compare_and_exchange (volatile gint *atomic, gint oldval, gint newval)
{
  return __atomic_compare_exchange_n ((gint *) atomic, &oldval, newval, 0,
      __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

#define GUM_ONCE_SENTINEL ((gsize) 1 << (sizeof (gsize) * 8 - 1))

static inline gboolean
g_once_init_enter (volatile gsize *location)
{
  gsize zero = 0;

  if (__atomic_compare_exchange_n ((gsize *) location, &zero, GUM_ONCE_SENTINEL,
          0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
    return TRUE;
  for (;;)
    {
      gsize v = __atomic_load_n ((gsize *) location, __ATOMIC_ACQUIRE);
      if (v != GUM_ONCE_SENTINEL)
        return FALSE;
    }
}

static inline void
g_once_init_leave (volatile gsize *location, gsize result)
{
  __atomic_store_n ((gsize *) location, result, __ATOMIC_RELEASE);
}

#define G_ONCE_INIT { 0, NULL }
#define G_PRIVATE_INIT(notify) { 0, (notify), 0 }
#define G_QUEUE_INIT { NULL, NULL, 0 }

#define G_LOCK_DEFINE_STATIC(name) \
  static GMutex G_PASTE (g__, G_PASTE (name, _lock))
#define G_LOCK(name) g_mutex_lock (&G_PASTE (g__, G_PASTE (name, _lock)))
#define G_UNLOCK(name) g_mutex_unlock (&G_PASTE (g__, G_PASTE (name, _lock)))

#define G_TYPE_INVALID ((GType) 0)
#define G_TYPE_INTERFACE ((GType) 8)
#define G_TYPE_ENUM ((GType) 12)
#define G_TYPE_STRING ((GType) 16)
#define G_TYPE_BOXED ((GType) 18)
#define G_TYPE_OBJECT ((GType) 80)
#define G_TYPE_BYTES ((GType) 21)

#define G_OBJECT(object) ((GObject *) (object))
#define G_OBJECT_CLASS(klass) ((GObjectClass *) (klass))
#define G_OBJECT_GET_CLASS(object) \
  (G_OBJECT_CLASS (((GTypeInstance *) (object))->g_class))
#define G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec) \
  G_STMT_START { (void) (object); (void) (property_id); (void) (pspec); } G_STMT_END

#define G_PARAM_READABLE (1 << 0)
#define G_PARAM_WRITABLE (1 << 1)
#define G_PARAM_READWRITE (G_PARAM_READABLE | G_PARAM_WRITABLE)
#define G_PARAM_CONSTRUCT_ONLY (1 << 2)
#define G_PARAM_STATIC_STRINGS (1 << 3)

typedef enum {
  G_FILE_ERROR_EXIST,
  G_FILE_ERROR_ISDIR,
  G_FILE_ERROR_ACCES,
  G_FILE_ERROR_NAMETOOLONG,
  G_FILE_ERROR_NOENT,
  G_FILE_ERROR_NOTDIR,
  G_FILE_ERROR_NXIO,
  G_FILE_ERROR_NODEV,
  G_FILE_ERROR_ROFS,
  G_FILE_ERROR_TXTBSY,
  G_FILE_ERROR_FAULT,
  G_FILE_ERROR_LOOP,
  G_FILE_ERROR_NOSPC,
  G_FILE_ERROR_NOMEM,
  G_FILE_ERROR_MFILE,
  G_FILE_ERROR_NFILE,
  G_FILE_ERROR_BADF,
  G_FILE_ERROR_INVAL,
  G_FILE_ERROR_PIPE,
  G_FILE_ERROR_AGAIN,
  G_FILE_ERROR_INTR,
  G_FILE_ERROR_IO,
  G_FILE_ERROR_PERM,
  G_FILE_ERROR_NOSYS,
  G_FILE_ERROR_FAILED
} GFileError;

typedef enum {
  G_FILE_TEST_EXISTS = 1 << 0
} GFileTest;

#define G_FILE_ERROR (gum_rt_file_error_quark ())

typedef enum {
  G_REGEX_OPTIMIZE = 1 << 0,
  G_REGEX_RAW = 1 << 1,
  G_REGEX_MULTILINE = 1 << 2
} GRegexCompileFlags;

typedef enum {
  G_REGEX_MATCH_NOTEMPTY = 1 << 0
} GRegexMatchFlags;

#define G_LOG_LEVEL_CRITICAL (1 << 3)

gpointer gum_rt_steal_pointer (gpointer pp);
GQuark gum_rt_file_error_quark (void);
GQuark gum_rt_alloc_quark (void);
GType gum_rt_alloc_type_id (void);
GType gum_rt_register_class (GType parent_type, guint class_size,
    GClassInitFunc class_init, guint instance_size, GInstanceInitFunc instance_init);
GType gum_rt_register_interface (guint class_size, GClassInitFunc class_init);
GType gum_rt_register_boxed (GBoxedCopyFunc copy_func, GBoxedFreeFunc free_func);
gpointer gum_rt_iface (gpointer instance, GType iface_type);

static inline const gchar *
g_intern_static_string (const gchar *string G_GNUC_UNUSED)
{
  return "";
}

static inline GQuark
g_quark_from_static_string (const gchar *string G_GNUC_UNUSED)
{
  return gum_rt_alloc_quark ();
}

static inline GType
g_enum_register_static (const gchar *name G_GNUC_UNUSED,
                        const GEnumValue *values G_GNUC_UNUSED)
{
  return gum_rt_alloc_type_id ();
}

static inline GType
g_type_register_static_simple (GType parent_type,
                               const gchar *type_name G_GNUC_UNUSED,
                               guint class_size,
                               GClassInitFunc class_init,
                               guint instance_size,
                               GInstanceInitFunc instance_init,
                               GTypeFlags flags G_GNUC_UNUSED)
{
  if (parent_type == G_TYPE_INTERFACE)
    return gum_rt_register_interface (class_size, class_init);
  return gum_rt_register_class (parent_type, class_size, class_init,
      instance_size, instance_init);
}

static inline GType
g_boxed_type_register_static (const gchar *name G_GNUC_UNUSED,
                              GBoxedCopyFunc copy_func,
                              GBoxedFreeFunc free_func)
{
  return gum_rt_register_boxed (copy_func, free_func);
}

#define G_DEFINE_QUARK(QN, q_n) \
GQuark q_n##_quark (void) \
{ \
  static GQuark q; \
  if (G_UNLIKELY (q == 0)) \
    q = g_quark_from_static_string (#QN); \
  return q; \
}

#define G_DEFINE_TYPE(TN, t_n, T_P) G_DEFINE_TYPE_EXTENDED (TN, t_n, T_P, 0, {})
#define G_DEFINE_TYPE_EXTENDED(TN, t_n, T_P, _f_, _C_) \
  _GUM_DEFINE_TYPE_BEGIN (TN, t_n, T_P, _f_) { _C_; } _GUM_DEFINE_TYPE_END ()

#define _GUM_DEFINE_TYPE_BEGIN(TypeName, type_name, TYPE_PARENT, flags) \
static void type_name##_init (TypeName *self); \
static void type_name##_class_init (TypeName##Class *klass); \
static gpointer type_name##_parent_class = NULL; \
static void type_name##_class_intern_init (gpointer klass) \
{ \
  type_name##_parent_class = g_type_class_peek_parent (klass); \
  type_name##_class_init ((TypeName##Class *) klass); \
} \
GType type_name##_get_type (void) \
{ \
  static gsize static_g_define_type_id = 0; \
  if (g_once_init_enter (&static_g_define_type_id)) \
    { \
      GType g_define_type_id = \
        g_type_register_static_simple (TYPE_PARENT, \
            g_intern_static_string (#TypeName), \
            sizeof (TypeName##Class), \
            (GClassInitFunc) (void (*) (void)) type_name##_class_intern_init, \
            sizeof (TypeName), \
            (GInstanceInitFunc) (void (*) (void)) type_name##_init, \
            (GTypeFlags) (flags)); \
      {

#define _GUM_DEFINE_TYPE_END() \
      } \
      g_once_init_leave (&static_g_define_type_id, g_define_type_id); \
    } \
  return static_g_define_type_id; \
}

#define G_IMPLEMENT_INTERFACE(TYPE_IFACE, iface_init) { \
  const GInterfaceInfo g_implement_interface_info = { \
    (GInterfaceInitFunc) (void (*) (void)) iface_init, NULL, NULL \
  }; \
  g_type_add_interface_static (g_define_type_id, TYPE_IFACE, \
      &g_implement_interface_info); \
}

#define G_DEFINE_INTERFACE(TN, t_n, T_P) \
static void t_n##_default_init (TN##Interface *klass); \
GType t_n##_get_type (void) \
{ \
  static gsize static_g_define_type_id = 0; \
  if (g_once_init_enter (&static_g_define_type_id)) \
    { \
      GType g_define_type_id = \
        g_type_register_static_simple (G_TYPE_INTERFACE, \
            g_intern_static_string (#TN), \
            sizeof (TN##Interface), \
            (GClassInitFunc) (void (*) (void)) t_n##_default_init, \
            0, NULL, 0); \
      if ((T_P) != G_TYPE_INVALID) \
        g_type_interface_add_prerequisite (g_define_type_id, (T_P)); \
      g_once_init_leave (&static_g_define_type_id, g_define_type_id); \
    } \
  return static_g_define_type_id; \
}

#define G_DEFINE_BOXED_TYPE(TypeName, type_name, copy_func, free_func) \
GType type_name##_get_type (void) \
{ \
  static gsize static_g_define_type_id = 0; \
  if (g_once_init_enter (&static_g_define_type_id)) \
    { \
      GType g_define_type_id = \
        g_boxed_type_register_static (g_intern_static_string (#TypeName), \
            (GBoxedCopyFunc) (copy_func), (GBoxedFreeFunc) (free_func)); \
      g_once_init_leave (&static_g_define_type_id, g_define_type_id); \
    } \
  return static_g_define_type_id; \
}

#define G_DECLARE_FINAL_TYPE(ModuleObjName, module_obj_name, MODULE, OBJ_NAME, ParentName) \
  GType module_obj_name##_get_type (void); \
  typedef struct _##ModuleObjName ModuleObjName; \
  typedef struct { ParentName##Class parent_class; } ModuleObjName##Class; \
  G_GNUC_UNUSED static inline ModuleObjName * MODULE##_##OBJ_NAME (gpointer ptr) { \
    return (ModuleObjName *) ptr; } \
  G_GNUC_UNUSED static inline gboolean MODULE##_IS_##OBJ_NAME (gpointer ptr) { \
    (void) ptr; return TRUE; } \
  G_GNUC_UNUSED static inline ModuleObjName##Class * MODULE##_##OBJ_NAME##_CLASS (gpointer ptr) { \
    return (ModuleObjName##Class *) ptr; } \
  G_GNUC_UNUSED static inline gboolean MODULE##_IS_##OBJ_NAME##_CLASS (gpointer ptr) { \
    (void) ptr; return TRUE; } \
  G_GNUC_UNUSED static inline ModuleObjName##Class * MODULE##_##OBJ_NAME##_GET_CLASS (gpointer ptr) { \
    return (ModuleObjName##Class *) ((GTypeInstance *) ptr)->g_class; }

#define G_DECLARE_INTERFACE(ModuleObjName, module_obj_name, MODULE, OBJ_NAME, PrerequisiteName) \
  GType module_obj_name##_get_type (void); \
  typedef struct _##ModuleObjName ModuleObjName; \
  typedef struct _##ModuleObjName##Interface ModuleObjName##Interface; \
  G_GNUC_UNUSED static inline ModuleObjName * MODULE##_##OBJ_NAME (gpointer ptr) { \
    return (ModuleObjName *) ptr; } \
  G_GNUC_UNUSED static inline gboolean MODULE##_IS_##OBJ_NAME (gpointer ptr) { \
    (void) ptr; return TRUE; } \
  G_GNUC_UNUSED static inline ModuleObjName##Interface * MODULE##_##OBJ_NAME##_GET_IFACE (gpointer ptr) { \
    return (ModuleObjName##Interface *) gum_rt_iface (ptr, module_obj_name##_get_type ()); }

#define g_array_append_val(a, v) g_array_append_vals ((a), &(v), 1)
#define g_array_insert_val(a, i, v) g_array_insert_vals ((a), (i), &(v), 1)
#define g_array_index(a, t, i) (((t *) (void *) (a)->data)[(i)])
#define g_ptr_array_index(array, index_) ((array)->pdata[(index_)])

gpointer g_malloc (gsize n_bytes);
gpointer g_malloc0 (gsize n_bytes);
gpointer g_realloc (gpointer mem, gsize n_bytes);
void g_free (gpointer mem);
gpointer g_memdup (gconstpointer mem, guint byte_size);
gpointer g_slice_alloc (gsize block_size);
gpointer g_slice_alloc0 (gsize block_size);
void g_slice_free1 (gsize block_size, gpointer mem);

gchar *g_strdup (const gchar *str);
gchar *g_strndup (const gchar *str, gsize n);
gchar *g_strdup_printf (const gchar *format, ...) __attribute__ ((format (printf, 1, 2)));
gint g_vsnprintf (gchar *string, gulong n, const gchar *format, va_list args);
gchar *g_strchomp (gchar *str);
gchar **g_strsplit (const gchar *string, const gchar *delimiter, gint max_tokens);
void g_strfreev (gchar **str_array);
guint g_strv_length (gchar **str_array);
gboolean g_str_has_prefix (const gchar *str, const gchar *prefix);
gboolean g_str_has_suffix (const gchar *str, const gchar *suffix);
guint g_str_hash (gconstpointer v);
gboolean g_str_equal (gconstpointer v1, gconstpointer v2);
guint g_direct_hash (gconstpointer v);
gint g_ascii_xdigit_value (gchar c);
gchar g_ascii_toupper (gchar c);
const gchar *g_strerror (gint errnum);

void g_logv (const gchar *log_domain, gint log_level, const gchar *format, va_list args);
void g_error (const gchar *format, ...) G_NORETURN;
void g_abort (void) G_NORETURN;

GArray *g_array_new (gboolean zero_terminated, gboolean clear_, guint element_size);
GArray *g_array_sized_new (gboolean zero_terminated, gboolean clear_,
    guint element_size, guint reserved_size);
gchar *g_array_free (GArray *array, gboolean free_segment);
void g_array_unref (GArray *array);
GArray *g_array_set_size (GArray *array, guint length);
GArray *g_array_append_vals (GArray *array, gconstpointer data, guint len);
GArray *g_array_insert_vals (GArray *array, guint index_, gconstpointer data, guint len);
GArray *g_array_remove_index (GArray *array, guint index_);
void g_array_sort (GArray *array, GCompareFunc compare_func);
gboolean g_array_binary_search (GArray *array, gconstpointer target,
    GCompareFunc compare_func, guint *out_match_index);
void g_array_set_clear_func (GArray *array, GDestroyNotify clear_func);

GPtrArray *g_ptr_array_new_full (guint reserved_size, GDestroyNotify element_free_func);
GPtrArray *g_ptr_array_new_with_free_func (GDestroyNotify element_free_func);
void g_ptr_array_unref (GPtrArray *array);
gpointer *g_ptr_array_free (GPtrArray *array, gboolean free_segment);
void g_ptr_array_add (GPtrArray *array, gpointer data);
gpointer g_ptr_array_remove_index (GPtrArray *array, guint index_);
void g_ptr_array_set_size (GPtrArray *array, gint length);
void g_ptr_array_sort (GPtrArray *array, GCompareFunc compare_func);

GSList *g_slist_prepend (GSList *list, gpointer data);
GSList *g_slist_append (GSList *list, gpointer data);
GSList *g_slist_remove (GSList *list, gconstpointer data);
GSList *g_slist_find (GSList *list, gconstpointer data);
GSList *g_slist_copy (GSList *list);
void g_slist_free (GSList *list);
void g_slist_free_1 (GSList *list);
void g_slist_free_full (GSList *list, GDestroyNotify free_func);
GSList *g_slist_delete_link (GSList *list, GSList *link_);
void g_slist_foreach (GSList *list, GFunc func, gpointer user_data);

GList *g_list_remove_link (GList *list, GList *llink);
void g_list_free (GList *list);
void g_list_foreach (GList *list, GFunc func, gpointer user_data);
GList *g_list_sort (GList *list, GCompareFunc compare_func);

GQueue *g_queue_new (void);
void g_queue_free (GQueue *queue);
void g_queue_push_tail (GQueue *queue, gpointer data);
gpointer g_queue_pop_head (GQueue *queue);
gpointer g_queue_pop_tail (GQueue *queue);
gboolean g_queue_is_empty (GQueue *queue);

GString *g_string_new (const gchar *init);
GString *g_string_sized_new (gsize dfl_size);
gchar *g_string_free (GString *string, gboolean free_segment);
GString *g_string_append (GString *string, const gchar *val);
GString *g_string_append_c (GString *string, gchar c);
void g_string_append_printf (GString *string, const gchar *format, ...)
    __attribute__ ((format (printf, 2, 3)));

GHashTable *g_hash_table_new (GHashFunc hash_func, GEqualFunc key_equal_func);
GHashTable *g_hash_table_new_full (GHashFunc hash_func, GEqualFunc key_equal_func,
    GDestroyNotify key_destroy_func, GDestroyNotify value_destroy_func);
void g_hash_table_unref (GHashTable *hash_table);
gboolean g_hash_table_insert (GHashTable *hash_table, gpointer key, gpointer value);
gboolean g_hash_table_add (GHashTable *hash_table, gpointer key);
gboolean g_hash_table_remove (GHashTable *hash_table, gconstpointer key);
void g_hash_table_remove_all (GHashTable *hash_table);
gpointer g_hash_table_lookup (GHashTable *hash_table, gconstpointer key);
gboolean g_hash_table_contains (GHashTable *hash_table, gconstpointer key);
guint g_hash_table_size (GHashTable *hash_table);
GList *g_hash_table_get_keys (GHashTable *hash_table);
void g_hash_table_iter_init (GHashTableIter *iter, GHashTable *hash_table);
gboolean g_hash_table_iter_next (GHashTableIter *iter, gpointer *key, gpointer *value);
void g_hash_table_iter_remove (GHashTableIter *iter);

GBytes *g_bytes_new_static (gconstpointer data, gsize size);
GBytes *g_bytes_new_take (gpointer data, gsize size);
GBytes *g_bytes_ref (GBytes *bytes);
void g_bytes_unref (GBytes *bytes);
gconstpointer g_bytes_get_data (GBytes *bytes, gsize *size);

GMappedFile *g_mapped_file_new (const gchar *filename, gboolean writable, GError **error);
GBytes *g_mapped_file_get_bytes (GMappedFile *file);
void g_mapped_file_unref (GMappedFile *file);

GRegex *g_regex_new (const gchar *pattern, GRegexCompileFlags compile_options,
    GRegexMatchFlags match_options, GError **error);
void g_regex_unref (GRegex *regex);
gboolean g_regex_match (const GRegex *regex, const gchar *string,
    GRegexMatchFlags match_options, GMatchInfo **match_info);
gboolean g_regex_match_full (const GRegex *regex, const gchar *string,
    gssize string_len, gint start_position, GRegexMatchFlags match_options,
    GMatchInfo **match_info, GError **error);
gboolean g_match_info_matches (const GMatchInfo *match_info);
gboolean g_match_info_next (GMatchInfo *match_info, GError **error);
gboolean g_match_info_fetch_pos (const GMatchInfo *match_info, gint match_num,
    gint *start_pos, gint *end_pos);
void g_match_info_free (GMatchInfo *match_info);

gboolean g_path_is_absolute (const gchar *file_name);
gchar *g_path_get_dirname (const gchar *file_name);
gchar *g_canonicalize_filename (const gchar *filename, const gchar *relative_to);
gboolean g_file_test (const gchar *filename, GFileTest test);
gboolean g_file_get_contents (const gchar *filename, gchar **contents,
    gsize *length, GError **error);
gchar *g_file_read_link (const gchar *filename, GError **error);
GDir *g_dir_open (const gchar *path, guint flags, GError **error);
const gchar *g_dir_read_name (GDir *dir);
void g_dir_close (GDir *dir);

void g_set_error (GError **err, GQuark domain, gint code, const gchar *format, ...)
    __attribute__ ((format (printf, 4, 5)));
gboolean g_error_matches (const GError *error, GQuark domain, gint code);
void g_error_free (GError *error);

gint64 g_get_monotonic_time (void);
GThread *g_thread_new (const gchar *name, GThreadFunc func, gpointer data);
gpointer g_thread_join (GThread *thread);
void g_thread_yield (void);
gpointer g_once (GOnce *once, GThreadFunc func, gpointer arg);
gpointer g_private_get (GPrivate *key);
void g_private_set (GPrivate *key, gpointer value);
void g_mutex_init (GMutex *mutex);
void g_mutex_clear (GMutex *mutex);
void g_mutex_lock (GMutex *mutex);
void g_mutex_unlock (GMutex *mutex);
void g_rec_mutex_init (GRecMutex *rec_mutex);
void g_rec_mutex_clear (GRecMutex *rec_mutex);
void g_rec_mutex_lock (GRecMutex *rec_mutex);
gboolean g_rec_mutex_trylock (GRecMutex *rec_mutex);
void g_rec_mutex_unlock (GRecMutex *rec_mutex);
void g_cond_init (GCond *cond);
void g_cond_clear (GCond *cond);
void g_cond_wait (GCond *cond, GMutex *mutex);
void g_cond_signal (GCond *cond);

gpointer g_type_class_peek_parent (gpointer g_class);
void g_type_add_interface_static (GType instance_type, GType interface_type,
    const GInterfaceInfo *info);
void g_type_interface_add_prerequisite (GType interface_type, GType prerequisite_type);

gpointer g_object_new (GType object_type, const gchar *first_property_name, ...);
gpointer g_object_ref (gpointer object);
void g_object_unref (gpointer object);
void g_object_weak_ref (GObject *object, GWeakNotify notify, gpointer data);
void g_object_set_data_full (GObject *object, const gchar *key, gpointer data,
    GDestroyNotify destroy);
gpointer g_object_get_data (GObject *object, const gchar *key);
void g_object_class_install_property (GObjectClass *oclass, guint property_id,
    GParamSpec *pspec);

GParamSpec *g_param_spec_enum (const gchar *name, const gchar *nick,
    const gchar *blurb, GType enum_type, gint default_value, gint flags);
GParamSpec *g_param_spec_uint (const gchar *name, const gchar *nick,
    const gchar *blurb, guint minimum, guint maximum, guint default_value, gint flags);
GParamSpec *g_param_spec_int (const gchar *name, const gchar *nick,
    const gchar *blurb, gint minimum, gint maximum, gint default_value, gint flags);
GParamSpec *g_param_spec_uint64 (const gchar *name, const gchar *nick,
    const gchar *blurb, guint64 minimum, guint64 maximum, guint64 default_value, gint flags);
GParamSpec *g_param_spec_string (const gchar *name, const gchar *nick,
    const gchar *blurb, const gchar *default_value, gint flags);
GParamSpec *g_param_spec_boxed (const gchar *name, const gchar *nick,
    const gchar *blurb, GType boxed_type, gint flags);

void g_value_set_enum (GValue *value, gint v_enum);
void g_value_set_uint (GValue *value, guint v_uint);
void g_value_set_int (GValue *value, gint v_int);
void g_value_set_uint64 (GValue *value, guint64 v_uint64);
void g_value_set_string (GValue *value, const gchar *v_string);
void g_value_set_boxed (GValue *value, gconstpointer v_boxed);
gint g_value_get_enum (const GValue *value);
guint g_value_get_uint (const GValue *value);
guint64 g_value_get_uint64 (const GValue *value);
gchar *g_value_dup_string (const GValue *value);
gpointer g_value_dup_boxed (const GValue *value);

G_END_DECLS

#endif
