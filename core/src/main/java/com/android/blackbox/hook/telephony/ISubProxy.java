package com.android.blackbox.hook.telephony;

import android.os.IBinder;
import android.telephony.TelephonyManager;

import java.lang.reflect.Method;

import black.android.os.BRServiceManager;
import black.com.android.internal.telephony.BRISubStub;
import com.android.blackbox.hook.BinderInvocationStub;
import com.android.blackbox.hook.MethodHook;
import com.android.blackbox.hook.ProxyMethod;
import com.android.blackbox.utils.MethodParameterUtils;
import com.android.blackbox.utils.Slog;

/**
 * Hook {@code com.android.internal.telephony.ISub} (ServiceManager name {@value #SERVICE_ISUB}).
 * Complements {@link ITelephonyProxy}.
 */
public class ISubProxy extends BinderInvocationStub {
    public static final String TAG = "ISubProxy";

    public static final String SERVICE_ISUB = "isub";

    public ISubProxy() {
        super(BRServiceManager.get().getService(SERVICE_ISUB));
    }

    @Override
    protected Object getWho() {
        IBinder binder = BRServiceManager.get().getService(SERVICE_ISUB);
        if (binder == null) {
            Slog.w(TAG, "getWho: isub binder is null");
            return null;
        }
        return BRISubStub.get().asInterface(binder);
    }

    @Override
    protected void inject(Object baseInvocation, Object proxyInvocation) {
        replaceSystemService(SERVICE_ISUB);
    }

    @Override
    public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
        MethodParameterUtils.replaceFirstAppPkg(args);
        return super.invoke(proxy, method, args);
    }

    @Override
    public boolean isBadEnv() {
        return false;
    }

    @ProxyMethod("getSimStateForSlotIndex")
    public static class GetSimStateForSlotIndex extends MethodHook {
        @Override
        protected Object hook(Object who, Method method, Object[] args) throws Throwable {
            int slotIndex = -1;
            if (args != null && args.length > 0 && args[0] != null) {
                slotIndex = ((Number) args[0]).intValue();
            }
            Slog.d(TAG, "getSimStateForSlotIndex(" + slotIndex + ") -> SIM_STATE_READY");
            return TelephonyManager.SIM_STATE_READY;
        }
    }
}
