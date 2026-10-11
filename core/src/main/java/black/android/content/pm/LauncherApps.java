package black.android.content.pm;

import android.content.pm.PackageManager;
import android.os.IInterface;
import android.os.UserManager;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.content.pm.LauncherApps")
public interface LauncherApps {
    @BField
    PackageManager mPm();

    @BField
    IInterface mService();

    @BField
    UserManager mUserManager();
}
