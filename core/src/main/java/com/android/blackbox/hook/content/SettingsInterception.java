package com.android.blackbox.hook.content;

import android.os.Bundle;

import com.android.blackbox.utils.Slog;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * After a {@code settings} provider {@code call} returns, rewrite the Bundle {@code value}
 * for known method + setting pairs. Invoked only when the authority is {@code settings}.
 */
public final class SettingsInterception {
    public static final String TAG = "SettingsInterception";

    private static final String BUNDLE_VALUE_KEY = "value";

    private static final String M_GET_GLOBAL = "GET_global";
    private static final String M_GET_SECURE = "GET_secure";
    private static final String M_GET_SYSTEM = "GET_system";

    private static final Map<String, Map<String, String>> OVERRIDES;

    static {
        Map<String, Map<String, String>> root = new HashMap<>();
        put(root, M_GET_GLOBAL, "adb_enabled", "0");
        put(root, M_GET_GLOBAL, "development_settings_enabled", "0");
        put(root, M_GET_SECURE, "accessibility_enabled", "0");
        put(root, M_GET_SECURE, "airplane_mode_on", "0");
        put(root, M_GET_SECURE, "android_id", "0000000000000000");
        put(root, M_GET_SECURE, "location_mode", "3");
        put(root, M_GET_SYSTEM, "screen_brightness", "89");
        for (Map.Entry<String, Map<String, String>> e : root.entrySet()) {
            e.setValue(Collections.unmodifiableMap(e.getValue()));
        }
        OVERRIDES = Collections.unmodifiableMap(root);
    }

    private static void put(Map<String, Map<String, String>> root, String method, String settingKey, String value) {
        Map<String, String> byKey = root.get(method);
        if (byKey == null) {
            byKey = new HashMap<>();
            root.put(method, byKey);
        }
        byKey.put(settingKey, value);
    }

    private SettingsInterception() {
    }

    /**
     * Parse {@code IContentProvider#call} when the layout is
     * {@code (AttributionSource, authority, method, arg, extras)} and rewrite a matching value.
     */
    public static void intercept(Object[] args, Bundle result) {
        if (result == null) {
            return;
        }
        String authority = null;
        String method = null;
        String arg = null;
        if (args != null && args.length >= 5) {
            if (args[1] instanceof String) {
                authority = (String) args[1];
            }
            if (args[2] instanceof String) {
                method = (String) args[2];
            }
            if (args[3] instanceof String) {
                arg = (String) args[3];
            }
        }
        if (method == null || arg == null) {
            return;
        }
        Map<String, String> byKey = OVERRIDES.get(method);
        String value = byKey != null ? byKey.get(arg) : null;
        if (value != null) {
            String original = result.getString(BUNDLE_VALUE_KEY);
            result.putString(BUNDLE_VALUE_KEY, value);
            Slog.d(TAG, "authority=" + authority + ", method=" + method + ", arg=" + arg
                    + " from= " + original + " to= " + value);
        }
    }
}
