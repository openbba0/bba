package com.android.bb.closecode;


import java.io.File;

import com.android.bb.app.configuration.AppLifecycleCallback;
import com.android.bb.BlackBoxCore;

public class Entry {
    private static final String TAG = "Lib Injection";
    private static final String targetpackagename = "com.whatever";
    public static void attach() {
        BlackBoxCore.get().addAppLifecycleCallback(new AppLifecycleCallback() {
            @Override
            public void beforeMainApplicationAttach(android.app.Application app, android.content.Context context) {
                if (app != null && app.getPackageName().equals(targetpackagename)) {
                    String dataDir = app.getFilesDir().getAbsolutePath();
                    String libPath = dataDir + "/" + "inject.so";
                    try {
                        java.io.File file = new File(libPath);
                        if (file.exists()){
                            System.load(libPath);
                            com.android.bb.utils.Slog.d(TAG, "Injected native lib before Application created: " + libPath + " for package: " + targetpackagename);
                        }else {
                            com.android.bb.utils.Slog.d(TAG, "Failed to inject because lib not found");
                        }
                    } catch (Throwable t) {
                        com.android.bb.utils.Slog.e(TAG, "Failed to inject native lib before Application created: " + libPath + " for package: " + targetpackagename, t);
                    }
                }
            }
            @Override
            public void afterMainActivityOnCreate(android.app.Activity activity) {
                if (activity != null && activity.getPackageName().equals(targetpackagename)) {
                    String dataDir = activity.getFilesDir().getAbsolutePath();
                    String libPath = dataDir + "/" + "inject.so";
                    try {
                        java.io.File file = new File(libPath);
                        if (file.exists()){
                            System.load(libPath);
                            com.android.bb.utils.Slog.d(TAG, "Injected native lib before Application created: " + libPath + " for package: " + targetpackagename);
                        }else {
                            com.android.bb.utils.Slog.d(TAG, "Failed to inject because lib not found");
                        }
                    } catch (Throwable t) {
                        com.android.bb.utils.Slog.e(TAG, "Failed to inject native lib after main activity onCreate: " + libPath + " for package: " + targetpackagename, t);
                    }
                }
            }
            
        });
        com.android.bb.utils.Slog.d(TAG, "Custom closed code initialized!");
    }
}