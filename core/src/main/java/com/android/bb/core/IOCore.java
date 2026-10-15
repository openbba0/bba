package com.android.bb.core;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Process;
import android.text.TextUtils;

import java.io.File;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;

import com.android.bb.BlackBoxCore;

import com.android.bb.core.env.BEnvironment;
import com.android.bb.utils.FileUtils;
import com.android.bb.utils.TrieTree;


@SuppressLint("SdCardPath")
public class IOCore {
    public static final String TAG = "IOCore";

    private static final IOCore sIOCore = new IOCore();
    private static final TrieTree mTrieTree = new TrieTree();
    private static final TrieTree sBlackTree = new TrieTree();
    private final Map<String, String> mRedirectMap = new LinkedHashMap<>();

    private static final Map<String, Map<String, String>> sCachePackageRedirect = new HashMap<>();

    public static IOCore get() {
        return sIOCore;
    }

    
    public void addRedirect(String origPath, String redirectPath) {
        if (TextUtils.isEmpty(origPath) || TextUtils.isEmpty(redirectPath) || mRedirectMap.get(origPath) != null)
            return;
        
        mTrieTree.add(origPath);
        mRedirectMap.put(origPath, redirectPath);
        File redirectFile = new File(redirectPath);
        if (!redirectFile.exists()) {
            FileUtils.mkdirs(redirectPath);
        }
        NativeCore.addIORule(origPath, redirectPath);
    }

    public void addBlackRedirect(String path) {
        if (TextUtils.isEmpty(path))
            return;
        sBlackTree.add(path);
    }

    public String redirectPath(String path) {
        if (TextUtils.isEmpty(path))
            return path;
        if (BEnvironment.isVirtualPath(path)) {
            return path;
        }
        String search = sBlackTree.search(path);
        if (!TextUtils.isEmpty(search))
            return search;

        
        String key = mTrieTree.search(path);
        if (!TextUtils.isEmpty(key)) {
            String dest = mRedirectMap.get(key);
            if (dest != null && path.startsWith(key)) {
                return dest + path.substring(key.length());
            }
        }

        return path;
    }

    public File redirectPath(File path) {
        if (path == null)
            return null;
        String pathStr = path.getAbsolutePath();
        return new File(redirectPath(pathStr));
    }

    public String redirectPath(String path, Map<String, String> rule) {
        if (TextUtils.isEmpty(path))
            return path;

        
        String key = mTrieTree.search(path);
        if (!TextUtils.isEmpty(key)) {
            String dest = rule.get(key);
            if (dest != null && path.startsWith(key)) {
                return dest + path.substring(key.length());
            }
        }

        return path;
    }

    public File redirectPath(File path, Map<String, String> rule) {
        if (path == null)
            return null;
        String pathStr = path.getAbsolutePath();
        return new File(redirectPath(pathStr, rule));
    }

    

    public void enableRedirect(Context context) {
        Map<String, String> rule = new LinkedHashMap<>();
        Set<String> blackRule = new HashSet<>();
        String packageName = context.getPackageName();

        try {
            int systemUserId = BlackBoxCore.getHostUserId();
            int userId = BlackBoxCore.getUserId();
            String physicalData = BEnvironment.getDataDir(packageName, userId).getAbsolutePath();
            String physicalLib = BEnvironment.getAppLibDir(packageName).getAbsolutePath();
            String physicalDe = BEnvironment.getDeDataDir(packageName, userId).getAbsolutePath();

            rule.put(String.format("/data/data/%s/lib", packageName), physicalLib);
            rule.put(String.format("/data/user/%d/%s/lib", systemUserId, packageName), physicalLib);

            rule.put(String.format("/data/data/%s", packageName), physicalData);
            rule.put(String.format("/data/user/%d/%s", systemUserId, packageName), physicalData);
            FileUtils.mkdirs(physicalDe);
            rule.put(BEnvironment.getVisibleDeDataDir(packageName, systemUserId), physicalDe);

            
            File profilesRoot = new File(BEnvironment.getVirtualRoot(), "profiles");
            FileUtils.mkdirs(profilesRoot.getAbsolutePath());
            
            rule.put("/data/misc/profiles", profilesRoot.getAbsolutePath());

            File profilesCurDir = new File(profilesRoot, String.format("cur/%d/%s", userId, packageName));
            File profilesRefDir = new File(profilesRoot, String.format("ref/%d/%s", userId, packageName));
            FileUtils.mkdirs(profilesCurDir.getAbsolutePath());
            FileUtils.mkdirs(profilesRefDir.getAbsolutePath());
            rule.put(String.format("/data/misc/profiles/cur/%d/%s", userId, packageName), profilesCurDir.getAbsolutePath());
            rule.put(String.format("/data/misc/profiles/ref/%d/%s", userId, packageName), profilesRefDir.getAbsolutePath());

            if (BlackBoxCore.getContext().getExternalCacheDir() != null && context.getExternalCacheDir() != null) {
                File external = BEnvironment.getExternalUserDir(userId);

                
                rule.put("/sdcard", external.getAbsolutePath());
                rule.put(String.format("/storage/emulated/%d", systemUserId), external.getAbsolutePath());

                blackRule.add("/sdcard/Pictures");
                blackRule.add(String.format("/storage/emulated/%d/Pictures", systemUserId));
            }
            if (BlackBoxCore.get().isHideRoot()) {
                hideRoot(rule);
            }
            proc(rule);
        } catch (Exception e) {
            e.printStackTrace();
        }
        for (String key : rule.keySet()) {
            get().addRedirect(key, rule.get(key));
        }
        for (String s : blackRule) {
            get().addBlackRedirect(s);
        }
        NativeCore.enableIO();
    }

    private void hideRoot(Map<String, String> rule) {
        rule.put("/system/app/Superuser.apk", "/system/app/Superuser.apk-fake");
        rule.put("/sbin/su", "/sbin/su-fake");
        rule.put("/system/bin/su", "/system/bin/su-fake");
        rule.put("/system/xbin/su", "/system/xbin/su-fake");
        rule.put("/data/local/xbin/su", "/data/local/xbin/su-fake");
        rule.put("/data/local/bin/su", "/data/local/bin/su-fake");
        rule.put("/system/sd/xbin/su", "/system/sd/xbin/su-fake");
        rule.put("/system/bin/failsafe/su", "/system/bin/failsafe/su-fake");
        rule.put("/data/local/su", "/data/local/su-fake");
        rule.put("/su/bin/su", "/su/bin/su-fake");
    }

    private void proc(Map<String, String> rule) {
        int appPid = BlackBoxCore.getAppPid();
        int pid = Process.myPid();
        String selfProc = "/proc/self/";
        String proc = "/proc/" + pid + "/";

        File procDir = BEnvironment.getProcDir(appPid);
        String cmdline = new File(procDir, "cmdline").getAbsolutePath();
        rule.put(proc + "cmdline", cmdline);
        rule.put(selfProc + "cmdline", cmdline);

        File mapsOut = new File(procDir, "maps");
        if (writeFilteredMaps(mapsOut)) {
            String maps = mapsOut.getAbsolutePath();
            rule.put(proc + "maps", maps);
            rule.put(selfProc + "maps", maps);
        }

        // Guest processes are children of the host, not zygote. Spoof PPid so VA
        // scanners that walk parent cmdline see zygote instead of the host package.
        int zygotePid = findZygotePid();
        if (zygotePid > 0) {
            File statusOut = new File(procDir, "status");
            if (writeSpoofedStatus(statusOut, zygotePid)) {
                String status = statusOut.getAbsolutePath();
                rule.put(proc + "status", status);
                rule.put(selfProc + "status", status);
            }
        }
    }

    private static int findZygotePid() {
        File[] procs = new File("/proc").listFiles();
        if (procs == null) {
            return -1;
        }
        int zygote = -1;
        int zygote64 = -1;
        for (File f : procs) {
            String name = f.getName();
            if (name == null || name.isEmpty() || !Character.isDigit(name.charAt(0))) {
                continue;
            }
            java.io.BufferedReader reader = null;
            try {
                reader = new java.io.BufferedReader(new java.io.FileReader(new File(f, "cmdline")));
                String cmd = reader.readLine();
                if (cmd == null) {
                    continue;
                }
                int nul = cmd.indexOf('\0');
                if (nul >= 0) {
                    cmd = cmd.substring(0, nul);
                }
                cmd = cmd.trim();
                if ("zygote64".equals(cmd)) {
                    zygote64 = Integer.parseInt(name);
                } else if ("zygote".equals(cmd)) {
                    zygote = Integer.parseInt(name);
                }
            } catch (Throwable ignored) {
            } finally {
                try {
                    if (reader != null) reader.close();
                } catch (Throwable ignored) {
                }
            }
        }
        return zygote64 > 0 ? zygote64 : zygote;
    }

    private boolean writeSpoofedStatus(File out, int fakePpid) {
        java.io.BufferedReader reader = null;
        java.io.BufferedWriter writer = null;
        try {
            reader = new java.io.BufferedReader(new java.io.FileReader("/proc/self/status"));
            writer = new java.io.BufferedWriter(new java.io.FileWriter(out));
            String line;
            while ((line = reader.readLine()) != null) {
                if (line.startsWith("PPid:")) {
                    writer.write("PPid:\t" + fakePpid);
                } else {
                    writer.write(line);
                }
                writer.newLine();
            }
            writer.flush();
            return true;
        } catch (Throwable e) {
            return false;
        } finally {
            try {
                if (reader != null) reader.close();
            } catch (Throwable ignored) {
            }
            try {
                if (writer != null) writer.close();
            } catch (Throwable ignored) {
            }
        }
    }

    private boolean writeFilteredMaps(File out) {
        java.io.BufferedReader reader = null;
        java.io.BufferedWriter writer = null;
        try {
            reader = new java.io.BufferedReader(new java.io.FileReader("/proc/self/maps"));
            writer = new java.io.BufferedWriter(new java.io.FileWriter(out));
            String line;
            while ((line = reader.readLine()) != null) {
                String lower = line.toLowerCase();
                if (lower.contains("sandhook") || lower.contains("blackbox")) {
                    continue;
                }
                writer.write(line);
                writer.newLine();
            }
            writer.flush();
            return true;
        } catch (Throwable e) {
            return false;
        } finally {
            try {
                if (reader != null) reader.close();
            } catch (Throwable ignored) {
            }
            try {
                if (writer != null) writer.close();
            } catch (Throwable ignored) {
            }
        }
    }
}
