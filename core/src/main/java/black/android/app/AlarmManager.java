package black.android.app;

import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.app.AlarmManager")
public interface AlarmManager {
    @BField
    IInterface mService();

    @BField
    int mTargetSdkVersion();
}
