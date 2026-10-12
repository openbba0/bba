package com.android.bb.core;


import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.util.Log;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import androidx.annotation.Keep;

import java.io.File;
import java.util.List;

import dalvik.system.DexFile;
import com.android.bb.BlackBoxCore;
import com.android.bb.app.BActivityThread;
import com.android.bb.utils.compat.DexFileCompat;

import com.android.bb.core.env.BEnvironment;
import com.android.bb.core.system.JarManager;
import com.android.bb.utils.Slog;


public class NativeCore {
    public static final String TAG = "NativeCore";

    static {

        System.loadLibrary("blackbox");
    }

    public static native void init(int apiLevel);

    public static native void enableIO();

    public static native void addIORule(String targetPath, String relocatePath);

    public static native void hideXposed();

    public static native boolean disableHiddenApi();
    
    public static native boolean disableResourceLoading();

    /** Frida inline hook. Returns the original entry, or 0 if the replace failed. */
    public static native long gumReplace(long target, long replacement);

    public static native void gumRevert(long target);

    /**
     * Follow a thread with Stalker. {@code threadId} 0 follows the caller.
     * {@code eventMask} bits: call=1, ret=2, exec=4, block=8, compile=16.
     */
    public static native boolean gumStalkerFollow(long threadId, int eventMask);

    public static native void gumStalkerUnfollow(long threadId);

    public static native void gumStalkerExclude(long base, long size);

    public static native int gumStalkerEventCount();

    /** Register this guest process so spindle_server can deliver scripts to it. */
    public static native void spindleAgent(String name);

    /** Stop and launch a cloned app on user 0. Called from the bba main process. */
    @Keep
    public static boolean launchForSpindle(String packageName) {
        if (packageName == null || packageName.isEmpty()) {
            return false;
        }
        if (Looper.myLooper() == Looper.getMainLooper()) {
            return launchForSpindleOnMain(packageName);
        }
        final boolean[] ok = {false};
        CountDownLatch latch = new CountDownLatch(1);
        new Handler(Looper.getMainLooper()).post(() -> {
            ok[0] = launchForSpindleOnMain(packageName);
            latch.countDown();
        });
        try {
            // startActivity can block until the guest finishes bindApplication.
            // The guest waits for UNLOCK on that same path, so do not wait forever.
            if (!latch.await(3, TimeUnit.SECONDS)) {
                return true;
            }
        } catch (InterruptedException e) {
            return false;
        }
        return ok[0];
    }

    private static boolean launchForSpindleOnMain(String packageName) {
        try {
            BlackBoxCore core = BlackBoxCore.get();
            core.stopPackage(packageName, 0);
            return core.launchApk(packageName, 0);
        } catch (Throwable t) {
            Slog.w(TAG, "spindle launch " + packageName + ": " + t.getMessage());
            return false;
        }
    }

    public interface ScriptHandler {
        void onScript(String source);
    }

    private static volatile ScriptHandler sScriptHandler;

    public static void setScriptHandler(ScriptHandler handler) {
        sScriptHandler = handler;
    }

    /** Script bytes from the dev machine, already inside this client process. */
    @Keep
    public static void onSpindleScript(byte[] source) {
        if (source == null) {
            return;
        }
        String js = new String(source, java.nio.charset.StandardCharsets.UTF_8);
        Slog.i(TAG, "spindle script " + source.length + " bytes");
        ScriptHandler handler = sScriptHandler;
        if (handler != null) {
            handler.onScript(js);
        }
    }


    @Keep
    public static int getCallingUid(int origCallingUid) {
        try {
            
            if (origCallingUid > 0 && origCallingUid < Process.FIRST_APPLICATION_UID)
                return origCallingUid;
            
            if (origCallingUid > Process.LAST_APPLICATION_UID)
                return origCallingUid;

            if (origCallingUid == BlackBoxCore.getHostUid()) {
                
                String appPackageName = BlackBoxCore.getAppPackageName();
                if (appPackageName != null && appPackageName.equals("com.google.android.gms")){
                    
                }
                
                
                
                if (appPackageName != null && appPackageName.equals("com.google.android.webview")){
                    return Process.myUid();
                }
                
                
                try {
                    int callingBUid = BlackBoxCore.getCallingBUid();
                    if (callingBUid > 0 && callingBUid < Process.LAST_APPLICATION_UID) {
                        return callingBUid;
                    }
                } catch (Exception e) {
                    Log.w(TAG, "Error getting calling BUid: " + e.getMessage());
                }
                
                
                try {
                    StackTraceElement[] stackTrace = Thread.currentThread().getStackTrace();
                    for (StackTraceElement element : stackTrace) {
                        String className = element.getClassName();
                        String methodName = element.getMethodName();
                        
                        
                        if ((className.contains("Settings") || className.contains("FeatureFlag")) &&
                            (methodName.contains("getString") || methodName.contains("getInt") || 
                             methodName.contains("getLong") || methodName.contains("getFloat"))) {
                            
                            Log.d(TAG, "System settings access detected, using system UID to prevent mismatch");
                            
                            return Process.SYSTEM_UID; 
                        }
                    }
                } catch (Exception e) {
                    Log.w(TAG, "Error analyzing stack trace for UID resolution: " + e.getMessage());
                }
                
                
                return BlackBoxCore.getHostUid();
            }
            return origCallingUid;
        } catch (Exception e) {
            Log.e(TAG, "Error in getCallingUid: " + e.getMessage());
            
            return Process.myUid();
        }
    }

    @Keep
    public static String redirectPath(String path) {
        return IOCore.get().redirectPath(path);
    }

    @Keep
    public static File redirectPath(File path) {
        return IOCore.get().redirectPath(path);
    }

    @Keep
    public static String getAppPackageName() {
        return BlackBoxCore.getAppPackageName();
    }

    @Keep
    public static boolean isXhsFingerprintDumpEnabled() {
        try {
            return BlackBoxCore.get().isXhsFingerprintDump();
        } catch (Throwable t) {
            Log.w(TAG, "isXhsFingerprintDumpEnabled failed: " + t.getMessage());
            return false;
        }
    }

    @Keep
    public static String getFingerprintDumpDir() {
        try {
            String packageName = BlackBoxCore.getAppPackageName();
            if (packageName == null) {
                return null;
            }
            File dir = BEnvironment.getExternalDataDir(packageName, BlackBoxCore.getUserId());
            if (!dir.exists() && !dir.mkdirs()) {
                Log.w(TAG, "getFingerprintDumpDir mkdir failed: " + dir);
            }
            return dir.getAbsolutePath();
        } catch (Throwable t) {
            Log.w(TAG, "getFingerprintDumpDir failed: " + t.getMessage());
            return null;
        }
    }

    @Keep
    public static long[] loadEmptyDex() {
        try {
            File emptyJar = JarManager.getInstance().getEmptyJar();
            if (emptyJar == null) {
                Log.w(TAG, "Empty JAR not available, attempting sync initialization");
                JarManager.getInstance().initializeSync();
                emptyJar = JarManager.getInstance().getEmptyJar();
            }
            
            if (emptyJar == null || !emptyJar.exists()) {
                Log.e(TAG, "Empty JAR file not found or invalid");
                return new long[]{};
            }
            
            DexFile dexFile = new DexFile(emptyJar);
            List<Long> cookies = DexFileCompat.getCookies(dexFile);
            long[] longs = new long[cookies.size()];
            for (int i = 0; i < cookies.size(); i++) {
                longs[i] = cookies.get(i);
            }
            Log.d(TAG, "Successfully loaded empty DEX with " + cookies.size() + " cookies");
            return longs;
        } catch (Exception e) {
            Log.e(TAG, "Failed to load empty DEX", e);
        }
        return new long[]{};
    }
    
    
    private static long[] createFallbackEmptyDex() {
        try {
            Slog.d(TAG, "Creating fallback empty DEX");
            
            
            
            byte[] emptyDexBytes = createMinimalDexBytes();
            
            
            File tempDexFile = File.createTempFile("fallback_empty", ".dex");
            tempDexFile.deleteOnExit();
            
            java.io.FileOutputStream fos = new java.io.FileOutputStream(tempDexFile);
            fos.write(emptyDexBytes);
            fos.close();
            
            
            DexFile dexFile = new DexFile(tempDexFile);
            List<Long> cookies = DexFileCompat.getCookies(dexFile);
            
            if (cookies != null && !cookies.isEmpty()) {
                long[] longs = new long[cookies.size()];
                for (int i = 0; i < cookies.size(); i++) {
                    longs[i] = cookies.get(i);
                }
                
                Slog.d(TAG, "Successfully created fallback empty DEX with " + cookies.size() + " cookies");
                return longs;
            }
            
        } catch (Exception e) {
            Slog.e(TAG, "Error creating fallback empty DEX: " + e.getMessage());
        }
        
        
        Slog.w(TAG, "Returning empty DEX array as last resort");
        return new long[]{};
    }
    
    
    private static byte[] createMinimalDexBytes() {
        
        
        
        
        
        byte[] dexHeader = {
            'd', 'e', 'x', '\n',  
            0x30, 0x33, 0x35, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x70, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00,  
            0x00, 0x00, 0x00, 0x00   
        };
        
        return dexHeader;
    }
}
