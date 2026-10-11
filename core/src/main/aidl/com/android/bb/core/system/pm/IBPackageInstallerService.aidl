// IBPackageInstallerService.aidl
package com.android.bb.core.system.pm;

import com.android.bb.core.system.pm.BPackageSettings;
import com.android.bb.model.pm.InstallOption;

// Declare any non-default types here with import statements

interface IBPackageInstallerService {
    int installPackageAsUser(in BPackageSettings ps, int userId);
    int uninstallPackageAsUser(in BPackageSettings ps, boolean removeApp, int userId);
    int clearPackage(in BPackageSettings ps, int userId);
    int updatePackage(in BPackageSettings ps);
}
