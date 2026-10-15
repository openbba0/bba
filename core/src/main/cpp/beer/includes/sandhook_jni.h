#ifndef SANDHOOK_JNI_H
#define SANDHOOK_JNI_H

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Register SandHook natives into an already-loaded shared library (libbba). */
bool JNI_Load_Ex(JNIEnv *env, jclass classSandHook, jclass classNeverCall);

#ifdef __cplusplus
}
#endif

#endif
