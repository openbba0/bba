package com.android.blackbox.hook.device;

import android.os.IBinder;

import java.lang.reflect.Method;

import black.android.hardware.biometrics.BRIAuthServiceStub;
import black.android.os.BRServiceManager;
import com.android.blackbox.hook.BinderInvocationStub;
import com.android.blackbox.hook.MethodHook;
import com.android.blackbox.hook.ProxyMethod;
import com.android.blackbox.utils.Slog;

/**
 * Hook {@code android.hardware.biometrics.IAuthService}.
 * Service name matches {@code Context.AUTH_SERVICE} ({@code "auth"}, API 29+).
 */
public class IAuthServiceProxy extends BinderInvocationStub {
    public static final String TAG = "IAuthServiceProxy";

    private static final String SERVICE_NAME = "auth";

    public IAuthServiceProxy() {
        super(BRServiceManager.get().getService(SERVICE_NAME));
    }

    @Override
    protected Object getWho() {
        IBinder binder = BRServiceManager.get().getService(SERVICE_NAME);
        if (binder == null) {
            Slog.w(TAG, "getWho: auth binder is null");
            return null;
        }
        return BRIAuthServiceStub.get().asInterface(binder);
    }

    @Override
    protected void inject(Object baseInvocation, Object proxyInvocation) {
        replaceSystemService(SERVICE_NAME);
    }

    @Override
    public boolean isBadEnv() {
        return false;
    }

    /**
     * int canAuthenticate(String opPackageName, int userId, int authenticators)
     * 0 is BiometricManager.BIOMETRIC_SUCCESS.
     */
    @ProxyMethod("canAuthenticate")
    public static class CanAuthenticate extends MethodHook {
        @Override
        protected Object hook(Object who, Method method, Object[] args) throws Throwable {
            Object original = method.invoke(who, args);
            Slog.d(TAG, "canAuthenticate: result=" + original + " -> 0");
            return 0;
        }
    }
}
