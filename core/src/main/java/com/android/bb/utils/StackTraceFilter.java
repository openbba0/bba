package com.android.bb.utils;

public class StackTraceFilter {
    static {
        install();
    }

    public static void install() {
        try {
            Thread.setDefaultUncaughtExceptionHandler((t, e) -> {
                StackTraceElement[] original = e.getStackTrace();
                e.setStackTrace(filterStackTrace(original));
            });
        } catch (Throwable ignored) {}
    }

    private static StackTraceElement[] filterStackTrace(StackTraceElement[] stack) {
        return java.util.Arrays.stream(stack)
            .filter(element -> !isSuspicious(element.getClassName()))
            .toArray(StackTraceElement[]::new);
    }

    private static boolean isSuspicious(String className) {
        String lower = className.toLowerCase();
        return lower.contains("xposed") ||
               lower.contains("epic") ||
               lower.contains("virtual") ||
               lower.contains("blackbox") ||
               lower.contains("sandhook") ||
               lower.contains("com.android.bb.") ||
               lower.contains("hook");
    }
}
