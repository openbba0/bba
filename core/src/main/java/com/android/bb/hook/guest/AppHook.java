package com.android.bb.hook.guest;

import android.app.Application;

import com.android.bb.utils.Slog;
import com.swift.sandhook.xposedcompat.XposedCompat;

import java.io.File;

/**
 * SandHook environment and package match for one guest app.
 * Subclasses only install that app's Java hooks.
 */
public abstract class AppHook {

    public abstract String packageName();

    /**
     * Subclasses return false to leave this guest process unhooked.
     */
    protected abstract boolean enabled();

    public final void install(String packageName, String processName, Application application) throws Throwable {
        if (!enabled()) {
            Slog.i(tag(), "hooks disabled, package=" + packageName + ", process=" + processName);
            return;
        }
        prepare(packageName, processName, application);
        onHook(application);
        Slog.i(tag(), "hooks installed, package=" + packageName + ", process=" + processName);
    }

    protected abstract void onHook(Application application) throws Throwable;

    protected String tag() {
        return getClass().getSimpleName();
    }

    private void prepare(String packageName, String processName, Application application) {
        XposedCompat.context = application;
        XposedCompat.packageName = packageName;
        XposedCompat.processName = processName;
        XposedCompat.classLoader = application.getClassLoader();
        XposedCompat.useInternalStub = false;
        XposedCompat.useNewCallBackup = true;
        File cache = new File(application.getCacheDir(), "sandhook");
        if (!cache.exists() && !cache.mkdirs()) {
            Slog.w(tag(), "sandhook cache mkdir failed: " + cache);
        }
        XposedCompat.cacheDir = cache;
    }
}
