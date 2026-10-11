package com.android.blackbox.hook.system;

import android.content.Context;
import android.os.IBinder;

import java.lang.reflect.Method;

import black.android.os.BRServiceManager;
import com.android.blackbox.hook.BinderInvocationStub;
import com.android.blackbox.hook.MethodHook;
import com.android.blackbox.hook.ProxyMethod;
import com.android.blackbox.hook.ScanClass;


@ScanClass(IInputMethodManagerProxy.class)
public class IInputMethodManagerProxy extends BinderInvocationStub {
    public static final String TAG = "IInputMethodManagerProxy";

    public IInputMethodManagerProxy() {
        super(BRServiceManager.get().getService(Context.INPUT_METHOD_SERVICE));
    }

    @Override
    protected Object getWho() {
        return BRServiceManager.get().getService(Context.INPUT_METHOD_SERVICE);
    }

    @Override
    protected void inject(Object baseInvocation, Object proxyInvocation) {
        replaceSystemService(Context.INPUT_METHOD_SERVICE);
    }

    @Override
    public boolean isBadEnv() {
        return false;
    }

    @ProxyMethod("startInputOrWindowGainedFocus")
    public static class StartInputOrWindowGainedFocus extends MethodHook {
        @Override
        protected Object hook(Object who, Method method, Object[] args) throws Throwable {
            return method.invoke(who, args);
        }
    }
}
