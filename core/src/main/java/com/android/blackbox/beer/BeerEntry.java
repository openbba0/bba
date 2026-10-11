package com.android.blackbox.beer;

import android.app.Application;

import com.android.blackbox.utils.Slog;

import java.util.HashMap;
import java.util.Map;

/**
 * Dispatches a guest process to the {@link AppHook} for its package.
 * Subclasses of {@link AppHook} are registered at compile time.
 */
public final class BeerEntry {

    private static final String TAG = "BeerEntry";

    private static final Map<String, AppHook> HOOKS = new HashMap<>();

    static {
        AppHookRegistry.register(HOOKS);
    }

    private static boolean sInstalled;

    private BeerEntry() {
    }

    public static void install(String packageName, String processName, Application application) {
        if (sInstalled || application == null || packageName == null) {
            return;
        }
        AppHook hook = HOOKS.get(packageName);
        if (hook == null) {
            return;
        }
        sInstalled = true;
        try {
            hook.install(packageName, processName, application);
        } catch (Throwable t) {
            Slog.e(TAG, "hook install failed, package=" + packageName, t);
        }
    }
}
