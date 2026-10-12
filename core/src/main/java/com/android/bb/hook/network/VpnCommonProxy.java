package com.android.bb.hook.network;

import java.lang.reflect.Method;
import java.util.List;

import black.com.android.internal.net.BRVpnConfig;
import black.com.android.internal.net.VpnConfigContext;
import com.android.bb.BlackBoxCore;
import com.android.bb.app.BActivityThread;
import com.android.bb.hook.MethodHook;
import com.android.bb.hook.ProxyMethod;
import com.android.bb.utils.MethodParameterUtils;


public class VpnCommonProxy {
    @ProxyMethod("setVpnPackageAuthorization")
    public static class setVpnPackageAuthorization extends MethodHook {

        @Override
        protected Object hook(Object who, Method method, Object[] args) throws Throwable {
            MethodParameterUtils.replaceFirstAppPkg(args);
            return method.invoke(who, args);
        }
    }

    @ProxyMethod("prepareVpn")
    public static class PrepareVpn extends MethodHook {

        @Override
        protected Object hook(Object who, Method method, Object[] args) throws Throwable {
            MethodParameterUtils.replaceFirstAppPkg(args);
            return method.invoke(who, args);
        }
    }

    @ProxyMethod("establishVpn")
    public static class establishVpn extends MethodHook {

        @Override
        protected Object hook(Object who, Method method, Object[] args) throws Throwable {
            VpnConfigContext vpnConfigContext = BRVpnConfig.get(args[0]);
            vpnConfigContext._set_user(BlackBoxCore.getHostPkg());

            handlePackage(vpnConfigContext.allowedApplications());
            handlePackage(vpnConfigContext.disallowedApplications());
            return method.invoke(who, args);
        }

        private void handlePackage(List<String> applications) {
            if (applications == null)
                return;
            if (applications.contains(BActivityThread.getAppPackageName())) {
                applications.add(BlackBoxCore.getHostPkg());
            }
        }
    }

}
