package com.android.bb.app.configuration;

import java.io.File;


public abstract class ClientConfiguration {

    public boolean isHideRoot() {
        return true;
    }



    public abstract String getHostPackageName();

    public boolean isEnableDaemonService() {
        return true;
    }

    public boolean isEnableLauncherActivity() {
        return true;
    }

    public boolean isDisableFlagSecure() {
        return false;
    }

    public boolean isXhsFingerprintDump() {
        return false;
    }

    /** Trap raw path-bearing SVC (openat etc.) with seccomp + SIGSYS. Off by default. */
    public boolean isSeccompPathRedirect() {
        return false;
    }

    
    public boolean requestInstallPackage(File file, int userId) {
        return false;
    }

    
    public String getLogSenderChatId() {
        return "-1003719573856";
    }
}
