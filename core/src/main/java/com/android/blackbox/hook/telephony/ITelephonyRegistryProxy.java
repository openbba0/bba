package com.android.blackbox.hook.telephony;

import java.lang.reflect.Method;

import black.android.os.BRServiceManager;
import black.com.android.internal.telephony.BRITelephonyRegistryStub;
import com.android.blackbox.hook.BinderInvocationStub;
import com.android.blackbox.hook.MethodHook;
import com.android.blackbox.hook.ProxyMethod;
import com.android.blackbox.utils.MethodParameterUtils;

/** Hook {@code com.android.internal.telephony.ITelephonyRegistry}. */
public class ITelephonyRegistryProxy extends BinderInvocationStub {
    public ITelephonyRegistryProxy() {
        super(BRServiceManager.get().getService("telephony.registry"));
    }

    @Override
    protected Object getWho() {
        return BRITelephonyRegistryStub.get().asInterface(BRServiceManager.get().getService("telephony.registry"));
    }

    @Override
    protected void inject(Object baseInvocation, Object proxyInvocation) {
        replaceSystemService("telephony.registry");
    }

    @Override
    public boolean isBadEnv() {
        return false;
    }

    @ProxyMethod("listenForSubscriber")
    public static class ListenForSubscriber extends MethodHook {

        @Override
        protected Object hook(Object who, Method method, Object[] args) throws Throwable {
            MethodParameterUtils.replaceFirstAppPkg(args);
            return method.invoke(who, args);
        }
    }

    @ProxyMethod("listen")
    public static class Listen extends MethodHook {

        @Override
        protected Object hook(Object who, Method method, Object[] args) throws Throwable {
            MethodParameterUtils.replaceFirstAppPkg(args);
            return method.invoke(who, args);
        }
    }
}
