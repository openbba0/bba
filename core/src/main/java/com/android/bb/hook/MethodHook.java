package com.android.bb.hook;

import java.lang.reflect.Method;

import com.android.bb.BlackBoxCore;


public abstract class MethodHook {
    protected String getMethodName() {
        return null;
    }

    protected Object afterHook(Object result) throws Throwable {
        return result;
    }

    protected Object beforeHook(Object who, Method method, Object[] args) throws Throwable {
        return null;
    }

    protected abstract Object hook(Object who, Method method, Object[] args) throws Throwable;

    protected boolean isEnable() {
        return BlackBoxCore.get().isBlackProcess();
    }
}
