package black.android.app;

import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.app.AppOpsManager")
public interface AppOpsManager {
    @BField
    IInterface mService();
}
