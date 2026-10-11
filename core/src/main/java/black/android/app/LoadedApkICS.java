package black.android.app;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.app.LoadedApk")
public interface LoadedApkICS {
    @BField
    Object mCompatibilityInfo();
}
