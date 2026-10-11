package com.android.blackbox.beer;

import android.app.Application;

import com.android.blackbox.utils.Slog;
import com.scorpion.IHook.XC_MethodHook;
import com.scorpion.IHook.XposedHelpers;

public final class XhsHook extends AppHook {

    private static final String PACKAGE = "com.xingin.xhs";

    @Override
    public String packageName() {
        return PACKAGE;
    }

    @Override
    protected void onHook(Application application) {
        ClassLoader loader = application.getClassLoader();
        Class<?> builder = XposedHelpers.findClassIfExists("okhttp3.Request$Builder", loader);
        if (builder == null) {
            Slog.w(tag(), "okhttp3.Request$Builder not found");
            return;
        }
        hookUrl(builder, String.class, "Builder.url String");
        Class<?> httpUrl = XposedHelpers.findClassIfExists("okhttp3.HttpUrl", loader);
        if (httpUrl != null) {
            hookUrl(builder, httpUrl, "Builder.url HttpUrl");
        }
        Slog.i(tag(), "Request.Builder.url attached");
    }

    private void hookUrl(Class<?> builder, Class<?> argType, String stackTag) {
        try {
            XposedHelpers.findAndHookMethod(builder, "url", argType, new XC_MethodHook() {
                @Override
                protected void beforeHookedMethod(MethodHookParam param) {
                    Slog.i(tag(), "[url] set url = " + param.args[0]);
                    logStack(stackTag);
                }
            });
        } catch (Throwable t) {
            Slog.e(tag(), "hook url(" + argType.getName() + ") failed", t);
        }
    }

    private void logStack(String message) {
        String text = android.util.Log.getStackTraceString(new Throwable("stack"));
        String[] lines = text.split("\n");
        Slog.i(tag(), message + " stack");
        int count = Math.min(lines.length, 40);
        for (int i = 0; i < count; i++) {
            if (lines[i] != null && lines[i].length() > 0) {
                Slog.i(tag(), lines[i]);
            }
        }
    }
}
