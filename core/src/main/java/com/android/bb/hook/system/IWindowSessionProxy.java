package com.android.bb.hook.system;

import android.os.IInterface;
import android.view.WindowManager;

import java.lang.reflect.Method;

import com.android.bb.BlackBoxCore;
import com.android.bb.hook.BinderInvocationStub;
import com.android.bb.hook.MethodHook;
import com.android.bb.hook.ProxyMethod;



public class IWindowSessionProxy extends BinderInvocationStub {
    public static final String TAG = "WindowSessionStub";

    private IInterface mSession;

    public IWindowSessionProxy(IInterface session) {
        super(session.asBinder());
        mSession = session;
    }

    @Override
    protected Object getWho() {
        return mSession;
    }

    @Override
    protected void inject(Object baseInvocation, Object proxyInvocation) {

    }

    @Override
    public boolean isBadEnv() {
        return false;
    }

    @Override
    public Object getProxyInvocation() {
        return super.getProxyInvocation();
    }

    @ProxyMethod("addToDisplay")
    public static class AddToDisplay extends MethodHook {
        @Override
        protected Object hook(Object who, Method method, Object[] args) throws Throwable {
            for (Object arg : args) {
                if (arg == null) {
                    continue;
                }
                if (arg instanceof WindowManager.LayoutParams) {
                    WindowManager.LayoutParams lp = (WindowManager.LayoutParams) arg;
                    lp.packageName = BlackBoxCore.getHostPkg();
                    if (BlackBoxCore.get().isDisableFlagSecure()) {
                        lp.flags &= ~WindowManager.LayoutParams.FLAG_SECURE;
                    }
                }
            }
            return method.invoke(who, args);
        }
    }

    @ProxyMethod("addToDisplayAsUser")
    public static class AddToDisplayAsUser extends AddToDisplay {
    }

    @ProxyMethod("relayout")
    public static class Relayout extends MethodHook {
        @Override
        protected Object hook(Object who, Method method, Object[] args) throws Throwable {
            for (Object arg : args) {
                if (arg == null) {
                    continue;
                }
                if (arg instanceof WindowManager.LayoutParams) {
                    WindowManager.LayoutParams lp = (WindowManager.LayoutParams) arg;
                    if (BlackBoxCore.get().isDisableFlagSecure()) {
                        lp.flags &= ~WindowManager.LayoutParams.FLAG_SECURE;
                    }
                }
            }
            return method.invoke(who, args);
        }
    }
}
