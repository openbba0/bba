package com.swift.sandhook;

import com.scorpion.IHook.XC_MethodHook;
import com.scorpion.IHook.XposedBridge;
import com.swift.sandhook.wrapper.HookWrapper;
import com.swift.sandhook.xposedcompat.XposedCompat;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Entry used by the native Java.hook path. Hook installation goes through the
 * local beer module (SandHook / XposedBridge) instead of javax.security.Beer.
 */
public final class ArtHookBridge {
    private static final Map<Member, XC_MethodHook> CALLBACKS = new ConcurrentHashMap<>();

    private ArtHookBridge() {
    }

    public static boolean init() {
        try {
            XposedCompat.useInternalStub = false;
            SandHook.class.getName();
            return true;
        } catch (Throwable t) {
            android.util.Log.e("SPDL", "sandhook init", t);
            return false;
        }
    }

    public static Method beer(Object hookObj, Member target, Method callback) throws Throwable {
        if (hookObj == null || target == null || callback == null) {
            throw new IllegalArgumentException("null hook");
        }
        final boolean isStatic = Modifier.isStatic(target.getModifiers());
        XC_MethodHook xc = new XC_MethodHook() {
            @Override
            protected void beforeHookedMethod(MethodHookParam param) throws Throwable {
                Object[] src = param.args == null ? new Object[0] : param.args;
                Object[] packed;
                if (isStatic) {
                    packed = src;
                } else {
                    packed = new Object[src.length + 1];
                    packed[0] = param.thisObject;
                    System.arraycopy(src, 0, packed, 1, src.length);
                }
                try {
                    Object result = callback.invoke(hookObj, new Object[]{packed});
                    param.setResult(result);
                } catch (InvocationTargetException e) {
                    Throwable cause = e.getCause() != null ? e.getCause() : e;
                    param.setThrowable(cause);
                }
            }
        };
        XposedBridge.hookMethod(target, xc);
        HookWrapper.HookEntity entity = SandHook.globalHookEntityMap.get(target);
        if (entity == null || entity.backup == null) {
            throw new IllegalStateException("sandhook backup missing for " + target);
        }
        CALLBACKS.put(target, xc);
        return entity.backup;
    }

    public static boolean unbeer(Member target) {
        XC_MethodHook xc = CALLBACKS.remove(target);
        if (xc == null) {
            return false;
        }
        XposedBridge.unhookMethod(target, xc);
        return true;
    }

    /** Original method. Instance backups are static and take this as the first argument. */
    public static Object callOrigin(Member target, Object thiz, Object[] args) throws Throwable {
        HookWrapper.HookEntity entity = SandHook.globalHookEntityMap.get(target);
        if (entity == null || entity.backup == null) {
            throw new IllegalStateException("no backup for " + target);
        }
        if (Modifier.isStatic(target.getModifiers())) {
            return entity.backup.invoke(null, args);
        }
        int n = args == null ? 0 : args.length;
        Object[] full = new Object[n + 1];
        full[0] = thiz;
        if (n > 0) {
            System.arraycopy(args, 0, full, 1, n);
        }
        return entity.backup.invoke(null, full);
    }

    public static final class Hook {
        private final long cookie;

        public Hook(long cookie) {
            this.cookie = cookie;
        }

        public Object dispatch(Object[] args) {
            return nativeDispatch(cookie, args);
        }

        private static native Object nativeDispatch(long cookie, Object[] args);
    }
}
