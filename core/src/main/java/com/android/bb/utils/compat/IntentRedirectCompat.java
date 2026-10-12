package com.android.bb.utils.compat;

import android.content.Intent;

import java.lang.reflect.Method;

/**
 * Android 16 / HyperOS rejects a proxy intent whose extras contain another
 * Intent unless {@code collectExtraIntentKeys()} ran first. The framework
 * method is hidden and missing on older releases.
 */
public final class IntentRedirectCompat {
    private static final Method COLLECT_EXTRA_INTENT_KEYS;

    static {
        Method collect = null;
        try {
            collect = Intent.class.getMethod("collectExtraIntentKeys");
        } catch (Throwable ignored) {
        }
        COLLECT_EXTRA_INTENT_KEYS = collect;
    }

    private IntentRedirectCompat() {
    }

    public static void prepare(Intent intent) {
        if (intent == null || COLLECT_EXTRA_INTENT_KEYS == null) {
            return;
        }
        try {
            COLLECT_EXTRA_INTENT_KEYS.invoke(intent);
        } catch (Throwable ignored) {
        }
    }
}
