package com.android.blackbox.core.system.pm.installer;

import com.android.blackbox.core.env.BEnvironment;
import com.android.blackbox.core.system.pm.BPackageSettings;
import com.android.blackbox.model.pm.InstallOption;
import com.android.blackbox.utils.FileUtils;


public class CreatePackageExecutor implements Executor {

    @Override
    public int exec(BPackageSettings ps, InstallOption option, int userId) {
        FileUtils.deleteDir(BEnvironment.getAppDir(ps.pkg.packageName));

        
        FileUtils.mkdirs(BEnvironment.getAppDir(ps.pkg.packageName));
        FileUtils.mkdirs(BEnvironment.getAppLibDir(ps.pkg.packageName));
        return 0;
    }
}
