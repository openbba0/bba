  .text
  .align 2

  .globl gum_stalker_follow_me
  .hidden gum_stalker_follow_me
  .type gum_stalker_follow_me, %function
gum_stalker_follow_me:
  stp x29, x30, [sp, -16]!
  mov x29, sp
  mov x3, x30
  bl _gum_stalker_do_follow_me
  ldp x29, x30, [sp], 16
  br x0

  .globl gum_stalker_activate
  .hidden gum_stalker_activate
  .type gum_stalker_activate, %function
gum_stalker_activate:
  stp x29, x30, [sp, -16]!
  mov x29, sp
  mov x2, x30
  bl _gum_stalker_do_activate
  ldp x29, x30, [sp], 16
  br x0

  .globl gum_stalker_deactivate
  .hidden gum_stalker_deactivate
  .type gum_stalker_deactivate, %function
gum_stalker_deactivate:
  stp x29, x30, [sp, -16]!
  mov x29, sp
  mov x1, x30
  bl _gum_stalker_do_deactivate
  ldp x29, x30, [sp], 16
  br x0
