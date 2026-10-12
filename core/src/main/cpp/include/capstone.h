#pragma once
#include <capstone/capstone.h>

/*
 * AOSP libcapstone is Capstone 5 without the PAC branch mnemonics or
 * cs_arch_register_arm64(). Arm64 is already built into that library, so
 * registration is a no-op. The extra ids are never returned by the
 * disassembler; they only keep frida-gum's switches compiling.
 */
#ifndef ARM64_INS_BRAA
#define ARM64_INS_BRAA   (ARM64_INS_ENDING + 1)
#define ARM64_INS_BRAAZ  (ARM64_INS_ENDING + 2)
#define ARM64_INS_BRAB   (ARM64_INS_ENDING + 3)
#define ARM64_INS_BRABZ  (ARM64_INS_ENDING + 4)
#define ARM64_INS_BLRAA  (ARM64_INS_ENDING + 5)
#define ARM64_INS_BLRAAZ (ARM64_INS_ENDING + 6)
#define ARM64_INS_BLRAB  (ARM64_INS_ENDING + 7)
#define ARM64_INS_BLRABZ (ARM64_INS_ENDING + 8)
#define ARM64_INS_RETAA  (ARM64_INS_ENDING + 9)
#define ARM64_INS_RETAB  (ARM64_INS_ENDING + 10)
#endif

static inline void cs_arch_register_arm64(void) {}
