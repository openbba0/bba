package com.android.bb.hook.network;

import android.content.Context;
import android.os.IBinder;

import black.android.net.wifi.BRIWifiManagerStub;
import black.android.os.BRServiceManager;
import com.android.bb.hook.BinderInvocationStub;


public class IWifiScannerProxy extends BinderInvocationStub {

    public IWifiScannerProxy() {
        super(BRServiceManager.get().getService("wifiscanner"));
    }

    @Override
    protected Object getWho() {
        return BRIWifiManagerStub.get().asInterface(BRServiceManager.get().getService("wifiscanner"));
    }

    @Override
    protected void inject(Object baseInvocation, Object proxyInvocation) {
        replaceSystemService("wifiscanner");
    }

    @Override
    public boolean isBadEnv() {
        return false;
    }
}
