package com.android.lppi;

import android.annotation.SuppressLint;
import android.os.Build;
import android.os.Process;


public class SandHookConfig {

    public volatile static int SDK_INT = Build.VERSION.SDK_INT;
    //Debug status of hook target process
    public volatile static boolean DEBUG = true;
    //Enable compile with jit
    public volatile static boolean compiler = SDK_INT < 29;
    public volatile static ClassLoader initClassLoader;
    public volatile static int curUser = 0;
    public volatile static boolean delayHook = false;//是否hook native

    public volatile static String libSandHookPath;

    /**
     * Native code is statically linked into {@code libbba.so}. Loading that
     * library registers SandHook JNI natives via {@code JNI_Load_Ex}.
     */
    public volatile static LibLoader libLoader = new LibLoader() {
        @SuppressLint("UnsafeDynamicallyLoadedCode")
        @Override
        public void loadLib() {
            if (SandHookConfig.libSandHookPath == null) {
                System.loadLibrary("bba");
            } else {
                System.load(SandHookConfig.libSandHookPath);
            }
        }
    };

    public static boolean is64bit() {
        try {
            Class.forName("dalvik.system.VMRuntime");
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return false;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return Process.is64Bit();
        }
        return false;//VMRuntime.is64Bit.call(VMRuntime.getRuntime.call());
    }

    public interface LibLoader {
        void loadLib();
    }
}
