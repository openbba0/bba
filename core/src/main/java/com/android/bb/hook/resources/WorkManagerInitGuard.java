package com.android.bb.hook.resources;

import android.app.Application;
import android.content.Context;

import com.android.bb.app.BActivityThread;
import com.android.bb.core.env.BEnvironment;
import com.android.bb.utils.Slog;
import com.android.lppi.XC_MethodHook;
import com.android.lppi.XposedHelpers;
import com.android.lppi.xposedcompat.XposedCompat;

import java.io.File;

/**
 * Guest apps that removed {@code WorkManagerInitializer} still get it from the host
 * class loader. Their later {@code WorkManager.initialize} then crashes startup.
 */
public final class WorkManagerInitGuard {

    private static final String TAG = "WorkManagerInitGuard";
    private static boolean sInstalled;

    private WorkManagerInitGuard() {
    }

    public static void install(String packageName, String processName, Application application) {
        if (sInstalled || application == null) {
            return;
        }
        ClassLoader loader = application.getClassLoader();
        Class<?> workManager = XposedHelpers.findClassIfExists("androidx.work.WorkManager", loader);
        Class<?> configuration = XposedHelpers.findClassIfExists("androidx.work.Configuration", loader);
        if (workManager == null || configuration == null) {
            return;
        }
        sInstalled = true;
        prepare(packageName, processName, application);
        try {
            XposedHelpers.findAndHookMethod(workManager, "initialize", Context.class, configuration,
                    new XC_MethodHook() {
                        @Override
                        protected void beforeHookedMethod(MethodHookParam param) {
                            if (alreadyInitialized(workManager)) {
                                Slog.i(TAG, "skip WorkManager.initialize, package=" + packageName);
                                param.setResult(null);
                            }
                        }
                    });
        } catch (Throwable t) {
            Slog.w(TAG, "hook WorkManager.initialize failed, package=" + packageName, t);
        }
    }

    private static boolean alreadyInitialized(Class<?> workManager) {
        try {
            workManager.getMethod("getInstance").invoke(null);
            return true;
        } catch (Throwable ignored) {
            return false;
        }
    }

    private static void prepare(String packageName, String processName, Application application) {
        XposedCompat.context = application;
        XposedCompat.packageName = packageName;
        XposedCompat.processName = processName;
        XposedCompat.classLoader = application.getClassLoader();
        XposedCompat.useInternalStub = false;
        XposedCompat.useNewCallBackup = true;
        File cache = new File(BEnvironment.getDataDir(packageName, BActivityThread.getUserId()), "cache/artx");
        if (!cache.exists() && !cache.mkdirs()) {
            Slog.w(TAG, "artx cache mkdir failed: " + cache);
        }
        XposedCompat.cacheDir = cache;
    }
}
