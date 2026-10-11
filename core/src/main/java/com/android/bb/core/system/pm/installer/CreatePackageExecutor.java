package com.android.bb.core.system.pm.installer;

import com.android.bb.core.env.BEnvironment;
import com.android.bb.core.system.pm.BPackageSettings;
import com.android.bb.model.pm.InstallOption;
import com.android.bb.utils.FileUtils;


public class CreatePackageExecutor implements Executor {

    @Override
    public int exec(BPackageSettings ps, InstallOption option, int userId) {
        FileUtils.deleteDir(BEnvironment.getAppDir(ps.pkg.packageName));

        
        FileUtils.mkdirs(BEnvironment.getAppDir(ps.pkg.packageName));
        FileUtils.mkdirs(BEnvironment.getAppLibDir(ps.pkg.packageName));
        return 0;
    }
}
