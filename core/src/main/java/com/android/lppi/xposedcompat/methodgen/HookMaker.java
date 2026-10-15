package com.android.lppi.xposedcompat.methodgen;

import com.android.lppi.XposedBridge;

import java.lang.reflect.Member;
import java.lang.reflect.Method;


public interface HookMaker {
    void start(Member member, XposedBridge.AdditionalHookInfo hookInfo,
                      ClassLoader appClassLoader, String dexDirPath) throws Exception;
    Method getHookMethod();
    Method getBackupMethod();
    Method getCallBackupMethod();
}
