package com.android.bb.core.system.pm.installer;

import java.io.File;

import com.android.bb.core.env.BEnvironment;
import com.android.bb.core.system.pm.BPackageSettings;
import com.android.bb.model.pm.InstallOption;
import com.android.bb.utils.FileUtils;


public class CreatePackageExecutor implements Executor {

    @Override
    public int exec(BPackageSettings ps, InstallOption option, int userId) {
        File apkDir = BEnvironment.getAppDir(ps.pkg.packageName);
        FileUtils.deleteDir(apkDir);
        // On-disk directory behind the preset path /data/app/<package>-1/base.apk.
        FileUtils.mkdirs(apkDir);
        FileUtils.mkdirs(BEnvironment.getAppLibDir(ps.pkg.packageName));
        return 0;
    }
}
