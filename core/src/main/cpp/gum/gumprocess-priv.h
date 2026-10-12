/*
 * Copyright (C) 2017-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2024 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_PROCESS_PRIV_H__
#define __GUM_PROCESS_PRIV_H__

#include "gumprocess.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL void _gum_process_enumerate_threads (GumFoundThreadFunc func,
    gpointer user_data);
G_GNUC_INTERNAL gboolean _gum_process_collect_main_module (GumModule * module,
    gpointer user_data);
G_GNUC_INTERNAL void _gum_process_enumerate_modules (GumFoundModuleFunc func,
    gpointer user_data);
G_GNUC_INTERNAL void _gum_process_enumerate_ranges (GumPageProtection prot,
    GumFoundRangeFunc func, gpointer user_data);

typedef guint64 GumArm64CtrlReg;
G_GNUC_INTERNAL void _gum_arm64_set_breakpoint (GumArm64CtrlReg * bcr,
    guint64 * bvr, guint breakpoint_id, GumAddress address);
G_GNUC_INTERNAL void _gum_arm64_unset_breakpoint (GumArm64CtrlReg * bcr,
    guint64 * bvr, guint breakpoint_id);
G_GNUC_INTERNAL void _gum_arm64_set_watchpoint (GumArm64CtrlReg * wcr,
    guint64 * wvr, guint watchpoint_id, GumAddress address, gsize size,
    GumWatchConditions conditions);
G_GNUC_INTERNAL void _gum_arm64_unset_watchpoint (GumArm64CtrlReg * wcr,
    guint64 * wvr, guint watchpoint_id);

G_END_DECLS

#endif
