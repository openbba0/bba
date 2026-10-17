package com.android.bb.core;


import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Process;
import android.util.Log;

import java.nio.charset.StandardCharsets;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import androidx.annotation.Keep;

import java.io.File;
import java.util.Map;

import com.android.bb.BlackBoxCore;
import com.android.bb.app.BActivityThread;
import com.android.bb.core.system.ServiceManager;
import com.android.bb.core.system.spindle.BSpindleService;
import com.android.bb.core.system.spindle.ISpindleClient;
import com.android.bb.core.system.spindle.ISpindleService;

import com.android.bb.core.env.BEnvironment;
import com.android.bb.utils.Slog;


public class NativeCore {
    public static final String TAG = "NativeCore";

    static {

        System.loadLibrary("bba");
    }

    public static native void init(int apiLevel);

    public static native void setupIO(Map<String, String> rules);

    public static native boolean disableResourceLoading();

    /** Trap raw path SVC via seccomp and rewrite with IO rules. Optional; off by default. */
    public static native boolean enableSeccompIo();

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

    /** Start the TCP listener. Only the :black process actually binds the port. */
    public static native void spindleListen();

    public static native int spindleAdd(String name, int pid, int uid);

    public static native void spindleDrop(int id);

    /**
     * Register this process with the spindle service in :black.
     * The service records pid and uid from the Binder caller.
     * The Binder call runs on a background thread. bindApplication invokes this
     * on the main thread while ActivityThread is still handling the launch
     * transaction, and a synchronous register there never returns.
     */
    public static void spindleAgent(String name) {
        if (!BSpindleService.ENABLED || name == null || name.isEmpty() || BlackBoxCore.get().isServerProcess()) {
            return;
        }
        Thread thread = new Thread(() -> registerSpindle(name), "spindle-agent");
        thread.setDaemon(true);
        thread.start();
    }

    private static void registerSpindle(String name) {
        try {
            IBinder binder = BlackBoxCore.get().getService(ServiceManager.SPINDLE);
            ISpindleService service = ISpindleService.Stub.asInterface(binder);
            if (service == null) {
                Slog.w(TAG, "spindle service missing");
                return;
            }
            if (!service.register(name, new SpindleClient(BlackBoxCore.get().isMainProcess()))) {
                Slog.w(TAG, "spindle register rejected: " + name);
            }
        } catch (Throwable t) {
            Slog.w(TAG, "spindle register " + name + ": " + t.getMessage());
        }
    }

    private static final class SpindleClient extends ISpindleClient.Stub {
        private final boolean mCanLaunch;

        SpindleClient(boolean canLaunch) {
            mCanLaunch = canLaunch;
        }

        @Override
        public byte[] onFrame(byte[] frame) {
            if (frame == null || frame.length == 0) {
                return null;
            }
            if (frameEquals(frame, "UNLOCK")) {
                return new byte[0];
            }
            if (frameStarts(frame, new byte[]{'R', 'P', 'C', 0})) {
                byte[] json = new byte[frame.length - 4];
                System.arraycopy(frame, 4, json, 0, json.length);
                return spindleRpc(json);
            }
            try {
                onSpindleScript(frame);
            } catch (Throwable t) {
                Slog.w(TAG, "spindle script: " + t.getMessage());
            }
            // Empty reply keeps the host session. null is reserved for a dead agent.
            return new byte[0];
        }

        @Override
        public boolean launch(String packageName) {
            if (!mCanLaunch) {
                return false;
            }
            return launchForSpindle(packageName);
        }
    }

    private static boolean frameEquals(byte[] frame, String text) {
        byte[] raw = text.getBytes(StandardCharsets.US_ASCII);
        if (frame.length != raw.length) {
            return false;
        }
        for (int i = 0; i < raw.length; i++) {
            if (frame[i] != raw[i]) {
                return false;
            }
        }
        return true;
    }

    private static boolean frameStarts(byte[] frame, byte[] prefix) {
        if (frame.length < prefix.length) {
            return false;
        }
        for (int i = 0; i < prefix.length; i++) {
            if (frame[i] != prefix[i]) {
                return false;
            }
        }
        return true;
    }

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
        spindleRunJs(source);
    }

    /** console.log from the QuickJS runtime. Forwards one line to the attached host. */
    @Keep
    public static void onSpindleLog(String text) {
        if (text == null || text.isEmpty()) {
            return;
        }
        try {
            IBinder binder = BlackBoxCore.get().getService(ServiceManager.SPINDLE);
            ISpindleService service = ISpindleService.Stub.asInterface(binder);
            if (service != null) {
                service.emitLog(text);
            }
        } catch (Throwable t) {
            Slog.w(TAG, "spindle log: " + t.getMessage());
        }
    }

    public static native int spindleRunJs(byte[] source);

    public static native byte[] spindleRpc(byte[] json);

    public static native void spindleHostLog(int pid, String text);


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
}
