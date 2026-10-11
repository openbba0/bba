package black.android.os.health;

import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.os.health.SystemHealthManager")
public interface SystemHealthManager {
    @BField
    IInterface mBatteryStats();
}
