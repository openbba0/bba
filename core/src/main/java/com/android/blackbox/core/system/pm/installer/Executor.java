package com.android.blackbox.core.system.pm.installer;

import com.android.blackbox.core.system.pm.BPackageSettings;
import com.android.blackbox.model.pm.InstallOption;


public interface Executor {
    public static final String TAG = "InstallExecutor";

    int exec(BPackageSettings ps, InstallOption option, int userId);
}
