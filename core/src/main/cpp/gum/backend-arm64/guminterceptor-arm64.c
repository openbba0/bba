/*
 * Copyright (C) 2014-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2022 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "guminterceptor-priv.h"

#include "gumarm64reader.h"
#include "gumarm64relocator.h"
#include "gumarm64writer.h"
#include "gumcloak.h"
#include "gumlibc.h"
#include "gummemory.h"


#define GUM_ARM64_LOGICAL_PAGE_SIZE 4096

#define GUM_FRAME_OFFSET_CPU_CONTEXT 0
#define GUM_FRAME_OFFSET_NEXT_HOP \
    (GUM_FRAME_OFFSET_CPU_CONTEXT + sizeof (GumCpuContext))

#define GUM_FCDATA(context) \
    ((GumArm64FunctionContextData *) (context)->backend_data.storage)

typedef struct _GumArm64FunctionContextData GumArm64FunctionContextData;

struct _GumInterceptorBackend
{
  GRecMutex * mutex;
  GumCodeAllocator * allocator;

  GumArm64Writer writer;
  GumArm64Relocator relocator;

  gpointer thunks;
  gpointer enter_thunk;
  gpointer leave_thunk;
};

struct _GumArm64FunctionContextData
{
  guint redirect_code_size;
  arm64_reg scratch_reg;
};

G_STATIC_ASSERT (sizeof (GumArm64FunctionContextData)
    <= sizeof (GumFunctionContextBackendData));

static void gum_interceptor_backend_create_thunks (
    GumInterceptorBackend * self);
static void gum_interceptor_backend_destroy_thunks (
    GumInterceptorBackend * self);

static void gum_emit_thunks (gpointer mem, GumInterceptorBackend * self);
static void gum_emit_enter_thunk (GumArm64Writer * aw);
static void gum_emit_leave_thunk (GumArm64Writer * aw);

static void gum_emit_prolog (GumArm64Writer * aw);
static void gum_emit_epilog (GumArm64Writer * aw);

GumInterceptorBackend *
_gum_interceptor_backend_create (GRecMutex * mutex,
                                 GumCodeAllocator * allocator)
{
  GumInterceptorBackend * backend;

  backend = g_slice_new0 (GumInterceptorBackend);
  backend->mutex = mutex;
  backend->allocator = allocator;

  if (gum_process_get_code_signing_policy () == GUM_CODE_SIGNING_OPTIONAL)
  {
    gum_arm64_writer_init (&backend->writer, NULL);
    gum_arm64_relocator_init (&backend->relocator, NULL, &backend->writer);

    gum_interceptor_backend_create_thunks (backend);
  }

  return backend;
}

void
_gum_interceptor_backend_destroy (GumInterceptorBackend * backend)
{
  if (backend->thunks != NULL)
  {
    gum_interceptor_backend_destroy_thunks (backend);

    gum_arm64_relocator_clear (&backend->relocator);
    gum_arm64_writer_clear (&backend->writer);
  }

  g_slice_free (GumInterceptorBackend, backend);
}


gboolean
_gum_interceptor_backend_claim_grafted_trampoline (GumInterceptorBackend * self,
                                                   GumFunctionContext * ctx)
{
  return FALSE;
}


static gboolean
gum_interceptor_backend_prepare_trampoline (GumInterceptorBackend * self,
                                            GumFunctionContext * ctx,
                                            gboolean * need_deflector)
{
  GumArm64FunctionContextData * data = GUM_FCDATA (ctx);
  gpointer function_address = ctx->function_address;
  guint redirect_limit;

  *need_deflector = FALSE;

  if (gum_arm64_relocator_can_relocate (function_address, 16,
      GUM_SCENARIO_ONLINE, &redirect_limit, &data->scratch_reg))
  {
    data->redirect_code_size = 16;

    ctx->trampoline_slice = gum_code_allocator_alloc_slice (self->allocator);
  }
  else
  {
    GumAddressSpec spec;
    gsize alignment;

    if (redirect_limit >= 8)
    {
      data->redirect_code_size = 8;

      spec.near_address = GSIZE_TO_POINTER (
          GPOINTER_TO_SIZE (function_address) &
          ~((gsize) (GUM_ARM64_LOGICAL_PAGE_SIZE - 1)));
      spec.max_distance = GUM_ARM64_ADRP_MAX_DISTANCE;
      alignment = GUM_ARM64_LOGICAL_PAGE_SIZE;
    }
    else if (redirect_limit >= 4)
    {
      data->redirect_code_size = 4;

      spec.near_address = function_address;
      spec.max_distance = GUM_ARM64_B_MAX_DISTANCE;
      alignment = 0;
    }
    else
    {
      return FALSE;
    }

    ctx->trampoline_slice = gum_code_allocator_try_alloc_slice_near (
        self->allocator, &spec, alignment);
    if (ctx->trampoline_slice == NULL)
    {
      ctx->trampoline_slice = gum_code_allocator_alloc_slice (self->allocator);
      *need_deflector = TRUE;
    }
  }

  if (data->scratch_reg == ARM64_REG_INVALID)
    goto no_scratch_reg;

  return TRUE;

no_scratch_reg:
  {
    gum_code_slice_unref (ctx->trampoline_slice);
    ctx->trampoline_slice = NULL;
    return FALSE;
  }
}

gboolean
_gum_interceptor_backend_create_trampoline (GumInterceptorBackend * self,
                                            GumFunctionContext * ctx)
{
  GumArm64Writer * aw = &self->writer;
  GumArm64Relocator * ar = &self->relocator;
  gpointer function_address = ctx->function_address;
  GumArm64FunctionContextData * data = GUM_FCDATA (ctx);
  gboolean need_deflector;
  gpointer deflector_target;
  GString * signature;
  gboolean is_eligible_for_lr_rewriting;
  guint reloc_bytes;

  if (!gum_interceptor_backend_prepare_trampoline (self, ctx, &need_deflector))
    return FALSE;

  gum_arm64_writer_reset (aw, ctx->trampoline_slice->data);

  if (ctx->type == GUM_INTERCEPTOR_TYPE_FAST)
  {
    deflector_target = ctx->replacement_function;
  }
  else
  {
    ctx->on_enter_trampoline =
        gum_sign_code_pointer (gum_arm64_writer_cur (aw));
    deflector_target = ctx->on_enter_trampoline;
  }

  if (need_deflector)
  {
    GumAddressSpec caller;
    gpointer return_address;
    gboolean dedicated;

    caller.near_address =
        (guint8 *) function_address + data->redirect_code_size - 4;
    caller.max_distance = GUM_ARM64_B_MAX_DISTANCE;

    return_address = (guint8 *) function_address + data->redirect_code_size;

    dedicated = data->redirect_code_size == 4;

    ctx->trampoline_deflector = gum_code_allocator_alloc_deflector (
        self->allocator, &caller, return_address, deflector_target, dedicated);
    if (ctx->trampoline_deflector == NULL)
    {
      gum_code_slice_unref (ctx->trampoline_slice);
      ctx->trampoline_slice = NULL;
      return FALSE;
    }

    gum_arm64_writer_put_pop_reg_reg (aw, ARM64_REG_X0, ARM64_REG_LR);
  }

  if (ctx->type != GUM_INTERCEPTOR_TYPE_FAST)
  {
    gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X17, GUM_ADDRESS (ctx));
    gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X16,
        GUM_ADDRESS (gum_sign_code_pointer (self->enter_thunk)));
    gum_arm64_writer_put_br_reg (aw, ARM64_REG_X16);

    ctx->on_leave_trampoline = gum_arm64_writer_cur (aw);

    gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X17, GUM_ADDRESS (ctx));
    gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X16,
        GUM_ADDRESS (gum_sign_code_pointer (self->leave_thunk)));
    gum_arm64_writer_put_br_reg (aw, ARM64_REG_X16);

    gum_arm64_writer_flush (aw);
    g_assert (gum_arm64_writer_offset (aw) <= ctx->trampoline_slice->size);
  }

  ctx->on_invoke_trampoline = gum_sign_code_pointer (gum_arm64_writer_cur (aw));

  gum_arm64_relocator_reset (ar, function_address, aw);

  signature = g_string_sized_new (16);

  do
  {
    const cs_insn * insn;

    reloc_bytes = gum_arm64_relocator_read_one (ar, &insn);
    g_assert (reloc_bytes != 0);

    if (signature->len != 0)
      g_string_append_c (signature, ';');
    g_string_append (signature, insn->mnemonic);
  }
  while (reloc_bytes < data->redirect_code_size);

  /*
   * Try to deal with minimal thunks that determine their caller and pass
   * it along to some inner function. This is important to support hooking
   * dlopen() on Android, where the dynamic linker uses the caller address
   * to decide on namespace and whether to allow the particular library to
   * be used by a particular caller.
   *
   * Because we potentially replace LR in order to trap the return, we end
   * up breaking dlopen() in such cases. We work around this by detecting
   * LR being read, and replace that instruction with a load of the actual
   * caller.
   *
   * This is however a bit risky done blindly, so we try to limit the
   * scope to the bare minimum. A potentially better longer term solution
   * is to analyze the function and patch each point of return, so we don't
   * have to replace LR on entry. That is however a bit complex, so we
   * opt for this simpler solution for now.
   */
  is_eligible_for_lr_rewriting = strcmp (signature->str, "mov;b") == 0 ||
      g_str_has_prefix (signature->str, "stp;mov;mov;bl");

  g_string_free (signature, TRUE);

  if (is_eligible_for_lr_rewriting)
  {
    const cs_insn * insn;

    while ((insn = gum_arm64_relocator_peek_next_write_insn (ar)) != NULL)
    {
      if (insn->id == ARM64_INS_MOV &&
          insn->detail->arm64.operands[1].reg == ARM64_REG_LR)
      {
        arm64_reg dst_reg = insn->detail->arm64.operands[0].reg;
        const guint reg_size = sizeof (gpointer);
        const guint reg_pair_size = 2 * reg_size;
        guint dst_reg_index, dst_reg_slot_index, dst_reg_offset_in_frame;

        gum_arm64_writer_put_push_all_x_registers (aw);

        gum_arm64_writer_put_call_address_with_arguments (aw,
            GUM_ADDRESS (_gum_interceptor_translate_top_return_address), 1,
            GUM_ARG_REGISTER, ARM64_REG_LR);

        if (dst_reg >= ARM64_REG_X0 && dst_reg <= ARM64_REG_X28)
        {
          dst_reg_index = dst_reg - ARM64_REG_X0;
        }
        else
        {
          g_assert (dst_reg >= ARM64_REG_X29 && dst_reg <= ARM64_REG_X30);

          dst_reg_index = dst_reg - ARM64_REG_X29;
        }

        dst_reg_slot_index = (dst_reg_index * reg_size) / reg_pair_size;

        dst_reg_offset_in_frame = (15 - dst_reg_slot_index) * reg_pair_size;
        if (dst_reg_index % 2 != 0)
          dst_reg_offset_in_frame += reg_size;

        gum_arm64_writer_put_str_reg_reg_offset (aw, ARM64_REG_X0, ARM64_REG_SP,
            dst_reg_offset_in_frame);

        gum_arm64_writer_put_pop_all_x_registers (aw);

        gum_arm64_relocator_skip_one (ar);
      }
      else
      {
        gum_arm64_relocator_write_one (ar);
      }
    }
  }
  else
  {
    gum_arm64_relocator_write_all (ar);
  }

  if (!ar->eoi)
  {
    GumAddress resume_at;

    resume_at = gum_sign_code_address (
        GUM_ADDRESS (function_address) + reloc_bytes);
    gum_arm64_writer_put_ldr_reg_address (aw, data->scratch_reg, resume_at);
    gum_arm64_writer_put_br_reg (aw, data->scratch_reg);
  }

  gum_arm64_writer_flush (aw);
  g_assert (gum_arm64_writer_offset (aw) <= ctx->trampoline_slice->size);

  ctx->overwritten_prologue_len = reloc_bytes;
  gum_memcpy (ctx->overwritten_prologue, function_address, reloc_bytes);

  return TRUE;
}

void
_gum_interceptor_backend_destroy_trampoline (GumInterceptorBackend * self,
                                             GumFunctionContext * ctx)
{

  gum_code_slice_unref (ctx->trampoline_slice);
  gum_code_deflector_unref (ctx->trampoline_deflector);
  ctx->trampoline_slice = NULL;
  ctx->trampoline_deflector = NULL;
}

void
_gum_interceptor_backend_activate_trampoline (GumInterceptorBackend * self,
                                              GumFunctionContext * ctx,
                                              gpointer prologue)
{
  GumArm64Writer * aw = &self->writer;
  GumArm64FunctionContextData * data = GUM_FCDATA (ctx);
  GumAddress on_enter;

  if (ctx->type == GUM_INTERCEPTOR_TYPE_FAST)
    on_enter = GUM_ADDRESS (ctx->replacement_function);
  else
    on_enter = GUM_ADDRESS (ctx->on_enter_trampoline);


  gum_arm64_writer_reset (aw, prologue);
  aw->pc = GUM_ADDRESS (ctx->function_address);

  if (ctx->trampoline_deflector != NULL)
  {
    if (data->redirect_code_size == 8)
    {
      gum_arm64_writer_put_push_reg_reg (aw, ARM64_REG_X0, ARM64_REG_LR);
      gum_arm64_writer_put_bl_imm (aw,
          GUM_ADDRESS (ctx->trampoline_deflector->trampoline));
    }
    else
    {
      g_assert (data->redirect_code_size == 4);
      gum_arm64_writer_put_b_imm (aw,
          GUM_ADDRESS (ctx->trampoline_deflector->trampoline));
    }
  }
  else
  {
    switch (data->redirect_code_size)
    {
      case 4:
        gum_arm64_writer_put_b_imm (aw, on_enter);
        break;
      case 8:
        gum_arm64_writer_put_adrp_reg_address (aw, data->scratch_reg, on_enter);
        gum_arm64_writer_put_br_reg_no_auth (aw, data->scratch_reg);
        break;
      case 16:
        gum_arm64_writer_put_ldr_reg_address (aw, data->scratch_reg, on_enter);
        gum_arm64_writer_put_br_reg (aw, data->scratch_reg);
        break;
      default:
        g_assert_not_reached ();
    }
  }

  gum_arm64_writer_flush (aw);
  g_assert (gum_arm64_writer_offset (aw) <= data->redirect_code_size);
}

void
_gum_interceptor_backend_deactivate_trampoline (GumInterceptorBackend * self,
                                                GumFunctionContext * ctx,
                                                gpointer prologue)
{

  gum_memcpy (prologue, ctx->overwritten_prologue,
      ctx->overwritten_prologue_len);
}

gpointer
_gum_interceptor_backend_get_function_address (GumFunctionContext * ctx)
{
  return ctx->function_address;
}

gpointer
_gum_interceptor_backend_resolve_redirect (GumInterceptorBackend * self,
                                           gpointer address)
{
  return gum_arm64_reader_try_get_relative_jump_target (address);
}

static void
gum_interceptor_backend_create_thunks (GumInterceptorBackend * self)
{
  gsize page_size, code_size;
  GumMemoryRange range;

  page_size = gum_query_page_size ();
  code_size = page_size;

  self->thunks = gum_memory_allocate (NULL, code_size, page_size, GUM_PAGE_RW);

  range.base_address = GUM_ADDRESS (self->thunks);
  range.size = code_size;
  gum_cloak_add_range (&range);

  gum_memory_patch_code (self->thunks, 1024,
      (GumMemoryPatchApplyFunc) gum_emit_thunks, self);
}

static void
gum_interceptor_backend_destroy_thunks (GumInterceptorBackend * self)
{
  gum_memory_free (self->thunks, gum_query_page_size ());
}

static void
gum_emit_thunks (gpointer mem,
                 GumInterceptorBackend * self)
{
  GumArm64Writer * aw = &self->writer;

  self->enter_thunk = self->thunks;
  gum_arm64_writer_reset (aw, mem);
  aw->pc = GUM_ADDRESS (self->enter_thunk);
  gum_emit_enter_thunk (aw);
  gum_arm64_writer_flush (aw);

  self->leave_thunk =
      (guint8 *) self->enter_thunk + gum_arm64_writer_offset (aw);
  gum_emit_leave_thunk (aw);
  gum_arm64_writer_flush (aw);
}

static void
gum_emit_enter_thunk (GumArm64Writer * aw)
{
  gum_emit_prolog (aw);

  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X1, ARM64_REG_SP,
      GUM_FRAME_OFFSET_CPU_CONTEXT);
  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X2, ARM64_REG_SP,
      GUM_FRAME_OFFSET_CPU_CONTEXT + G_STRUCT_OFFSET (GumCpuContext, lr));
  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X3, ARM64_REG_SP,
      GUM_FRAME_OFFSET_NEXT_HOP);

  gum_arm64_writer_put_call_address_with_arguments (aw,
      GUM_ADDRESS (_gum_function_context_begin_invocation), 4,
      GUM_ARG_REGISTER, ARM64_REG_X17,
      GUM_ARG_REGISTER, ARM64_REG_X1,
      GUM_ARG_REGISTER, ARM64_REG_X2,
      GUM_ARG_REGISTER, ARM64_REG_X3);

  gum_emit_epilog (aw);
}

static void
gum_emit_leave_thunk (GumArm64Writer * aw)
{
  gum_emit_prolog (aw);

  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X1, ARM64_REG_SP,
      GUM_FRAME_OFFSET_CPU_CONTEXT);
  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X2, ARM64_REG_SP,
      GUM_FRAME_OFFSET_NEXT_HOP);

  gum_arm64_writer_put_call_address_with_arguments (aw,
      GUM_ADDRESS (_gum_function_context_end_invocation), 3,
      GUM_ARG_REGISTER, ARM64_REG_X17,
      GUM_ARG_REGISTER, ARM64_REG_X1,
      GUM_ARG_REGISTER, ARM64_REG_X2);

  gum_emit_epilog (aw);
}

static void
gum_emit_prolog (GumArm64Writer * aw)
{
  gint i;

  /*
   * Set up our stack frame:
   *
   * [in: frame pointer chain entry, out: next_hop]
   * [in/out: cpu_context]
   */

  /* Reserve space for next_hop */
  gum_arm64_writer_put_sub_reg_reg_imm (aw, ARM64_REG_SP, ARM64_REG_SP, 16);

  /* Store vector registers */
  for (i = 30; i != -2; i -= 2)
    gum_arm64_writer_put_push_reg_reg (aw, ARM64_REG_Q0 + i, ARM64_REG_Q1 + i);

  /* Store X1-X28, FP, and LR */
  gum_arm64_writer_put_push_reg_reg (aw, ARM64_REG_FP, ARM64_REG_LR);
  for (i = 27; i != -1; i -= 2)
    gum_arm64_writer_put_push_reg_reg (aw, ARM64_REG_X0 + i, ARM64_REG_X1 + i);

  /* Store NZCV and X0 */
  gum_arm64_writer_put_mov_reg_nzcv (aw, ARM64_REG_X1);
  gum_arm64_writer_put_push_reg_reg (aw, ARM64_REG_X1, ARM64_REG_X0);

  /* PC placeholder and SP */
  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X0,
      ARM64_REG_SP, sizeof (GumCpuContext) -
      G_STRUCT_OFFSET (GumCpuContext, nzcv) + 16);
  gum_arm64_writer_put_push_reg_reg (aw, ARM64_REG_XZR, ARM64_REG_X0);

  /* Frame pointer chain entry */
  gum_arm64_writer_put_str_reg_reg_offset (aw, ARM64_REG_LR, ARM64_REG_SP,
      sizeof (GumCpuContext) + 8);
  gum_arm64_writer_put_str_reg_reg_offset (aw, ARM64_REG_FP, ARM64_REG_SP,
      sizeof (GumCpuContext) + 0);
  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_FP, ARM64_REG_SP,
      sizeof (GumCpuContext));
}

static void
gum_emit_epilog (GumArm64Writer * aw)
{
  guint i;

  /* Skip PC and SP */
  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_SP, ARM64_REG_SP, 16);

  /* Restore NZCV and X0 */
  gum_arm64_writer_put_pop_reg_reg (aw, ARM64_REG_X1, ARM64_REG_X0);
  gum_arm64_writer_put_mov_nzcv_reg (aw, ARM64_REG_X1);

  /* Restore X1-X28, FP, and LR */
  for (i = 1; i != 29; i += 2)
    gum_arm64_writer_put_pop_reg_reg (aw, ARM64_REG_X0 + i, ARM64_REG_X1 + i);
  gum_arm64_writer_put_pop_reg_reg (aw, ARM64_REG_FP, ARM64_REG_LR);

  /* Restore vector registers */
  for (i = 0; i != 32; i += 2)
    gum_arm64_writer_put_pop_reg_reg (aw, ARM64_REG_Q0 + i, ARM64_REG_Q1 + i);

  gum_arm64_writer_put_pop_reg_reg (aw, ARM64_REG_X16, ARM64_REG_X17);
  gum_arm64_writer_put_ret_reg (aw, ARM64_REG_X16);
}
