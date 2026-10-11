package com.android.bb.core.system.pm.installer;

import com.android.bb.core.env.BEnvironment;
import com.android.bb.core.system.pm.BPackageSettings;
import com.android.bb.model.pm.InstallOption;
import com.android.bb.utils.FileUtils;


public class RemoveUserExecutor implements Executor {

    @Override
    public int exec(BPackageSettings ps, InstallOption option, int userId) {
        String packageName = ps.pkg.packageName;
        
        FileUtils.deleteDir(BEnvironment.getDataDir(packageName, userId));
        FileUtils.deleteDir(BEnvironment.getDeDataDir(packageName, userId));
        FileUtils.deleteDir(BEnvironment.getExternalDataDir(packageName, userId));
        return 0;
    }
}
