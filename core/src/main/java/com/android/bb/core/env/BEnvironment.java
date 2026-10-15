package com.android.bb.core.env;

import java.io.File;
import java.util.Locale;

import com.android.bb.BlackBoxCore;
import com.android.bb.utils.FileUtils;


public class BEnvironment {
    /** On-disk folder under the host data dir. Keep this out of common VA scanners. */
    public static final String VIRTUAL_DIR = "v0";
    public static final String VIRTUAL_DIR_MARK = "/" + VIRTUAL_DIR + "/";
    private static final String LEGACY_VIRTUAL_DIR = "blackbox";

    private static final File sVirtualRoot = new File(BlackBoxCore.getContext().getCacheDir().getParent(), VIRTUAL_DIR);
    private static final File sExternalVirtualRoot = BlackBoxCore.getContext().getExternalFilesDir(VIRTUAL_DIR);

    public static File JUNIT_JAR = new File(getCacheDir(), "junit.apk");

    public static void load() {
        migrateLegacyRoot();
        FileUtils.mkdirs(sVirtualRoot);
        FileUtils.mkdirs(sExternalVirtualRoot);
        FileUtils.mkdirs(getSystemDir());
        FileUtils.mkdirs(getCacheDir());
        FileUtils.mkdirs(getProcDir());
    }

    private static void migrateLegacyRoot() {
        File parent = BlackBoxCore.getContext().getCacheDir().getParentFile();
        if (parent == null) {
            return;
        }
        File legacy = new File(parent, LEGACY_VIRTUAL_DIR);
        if (legacy.exists() && !sVirtualRoot.exists()) {
            if (!FileUtils.renameTo(legacy, sVirtualRoot)) {
                FileUtils.mkdirs(sVirtualRoot);
            }
        }
        File externalParent = BlackBoxCore.getContext().getExternalFilesDir(null);
        if (externalParent != null) {
            File legacyExt = new File(externalParent, LEGACY_VIRTUAL_DIR);
            if (legacyExt.exists() && sExternalVirtualRoot != null && !sExternalVirtualRoot.exists()) {
                if (!FileUtils.renameTo(legacyExt, sExternalVirtualRoot)) {
                    FileUtils.mkdirs(sExternalVirtualRoot);
                }
            }
        }
    }

    /** True when the path is already under the on-disk virtual tree. */
    public static boolean isVirtualPath(String path) {
        return path != null && (path.contains(VIRTUAL_DIR_MARK)
                || path.contains("/" + LEGACY_VIRTUAL_DIR + "/"));
    }

    public static File getVirtualRoot() {
        return sVirtualRoot;
    }

    public static File getExternalVirtualRoot() {
        return sExternalVirtualRoot;
    }

    public static File getSystemDir() {
        return new File(sVirtualRoot, "system");
    }

    public static File getProcDir() {
        return new File(sVirtualRoot, "proc");
    }

    public static File getCacheDir() {
        return new File(sVirtualRoot, "cache");
    }

    public static File getUserInfoConf() {
        return new File(getSystemDir(), "user.conf");
    }

    public static File getAccountsConf() {
        return new File(getSystemDir(), "accounts.conf");
    }

    public static File getUidConf() {
        return new File(getSystemDir(), "uid.conf");
    }

    public static File getSharedUserConf() {
        return new File(getSystemDir(), "shared-user.conf");
    }

    public static File getXPModuleConf() {
        return new File(getSystemDir(), "xposed-module.conf");
    }

    public static File getFakeLocationConf() {
        return new File(getSystemDir(), "fake-location.conf");
    }

    public static File getPackageConf(String packageName) {
        return new File(getAppDir(packageName), "package.conf");
    }

    public static File getExternalUserDir(int userId) {
        return new File(sExternalVirtualRoot, String.format(Locale.CHINA, "storage/emulated/%d/", userId));
    }

    public static File getUserDir(int userId) {
        return new File(sVirtualRoot, String.format(Locale.CHINA, "data/user/%d", userId));
    }

    public static File getExternalObbDir(String packageName, int userId) {
        return new File(getExternalUserDir(userId), String.format(Locale.CHINA, "Android/obb/%s", packageName));
    }


    public static File getDeDataDir(String packageName, int userId) {
        return new File(sVirtualRoot, String.format(Locale.CHINA, "data/user_de/%d/%s", userId, packageName));
    }

    public static File getExternalDataDir(String packageName, int userId) {
        return new File(getExternalUserDir(userId), String.format(Locale.CHINA, "Android/data/%s", packageName));
    }


    public static File getDataDir(String packageName, int userId) {
        return new File(sVirtualRoot, String.format(Locale.CHINA, "data/user/%d/%s", userId, packageName));
    }

    /** Path apps should see for their private data directory. */
    public static String getVisibleDataDir(String packageName, int userId) {
        return String.format(Locale.US, "/data/user/%d/%s", userId, packageName);
    }

    public static String getVisibleDeDataDir(String packageName, int userId) {
        return String.format(Locale.US, "/data/user_de/%d/%s", userId, packageName);
    }

    public static String getVisibleSourceDir(String packageName) {
        return String.format(Locale.US, "/data/app/%s-1/base.apk", packageName);
    }

    public static String getVisibleNativeLibraryDir(String packageName) {
        return String.format(Locale.US, "/data/app/%s-1/lib/%s", packageName, abiFolder());
    }

    private static String abiFolder() {
        String abi = android.os.Build.SUPPORTED_ABIS.length > 0
                ? android.os.Build.SUPPORTED_ABIS[0]
                : android.os.Build.CPU_ABI;
        if (abi != null && abi.startsWith("arm64")) {
            return "arm64";
        }
        if (abi != null && abi.startsWith("armeabi")) {
            return "arm";
        }
        if (abi != null && abi.startsWith("x86_64")) {
            return "x86_64";
        }
        if (abi != null && abi.startsWith("x86")) {
            return "x86";
        }
        return "arm64";
    }

    public static File getProcDir(int pid) {
        File file = new File(getProcDir(), String.format(Locale.CHINA, "%d", pid));
        FileUtils.mkdirs(file);
        return file;
    }

    public static File getExternalDataFilesDir(String packageName, int userId) {
        return new File(getExternalDataDir(packageName, userId), "files");
    }

    public static File getDataFilesDir(String packageName, int userId) {
        return new File(getDataDir(packageName, userId), "files");
    }

    public static File getExternalDataCacheDir(String packageName, int userId) {
        return new File(getExternalDataDir(packageName, userId), "cache");
    }

    public static File getDataCacheDir(String packageName, int userId) {
        return new File(getDataDir(packageName, userId), "cache");
    }

    public static File getDataLibDir(String packageName, int userId) {
        return new File(getDataDir(packageName, userId), "lib");
    }

    public static File getDataDatabasesDir(String packageName, int userId) {
        return new File(getDataDir(packageName, userId), "databases");
    }

    public static File getAppRootDir() {
        return getAppDir("");
    }

    public static File getAppDir(String packageName) {
        return new File(sVirtualRoot, "data/app/" + packageName);
    }

    public static File getBaseApkDir(String packageName) {
        return new File(sVirtualRoot, "data/app/" + packageName + "/base.apk");
    }

    public static File getAppLibDir(String packageName) {
        return new File(getAppDir(packageName), "lib");
    }

    public static File getXSharedPreferences(String packageName, String prefFileName) {
       return new File(BEnvironment.getDataDir(packageName, BlackBoxCore.getUserId()), "shared_prefs/" + prefFileName + ".xml");
    }
}
