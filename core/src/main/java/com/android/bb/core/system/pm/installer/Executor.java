package com.android.bb.core.system.pm.installer;

import com.android.bb.core.system.pm.BPackageSettings;
import com.android.bb.model.pm.InstallOption;


public interface Executor {
    public static final String TAG = "InstallExecutor";

    int exec(BPackageSettings ps, InstallOption option, int userId);
}
