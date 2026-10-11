package black.android.app;

import java.io.File;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.app.ContextImpl")
public interface ContextImplKitkat {
    @BField
    Object mDisplayAdjustments();

    @BField
    File[] mExternalCacheDirs();

    @BField
    File[] mExternalFilesDirs();

    @BField
    String mOpPackageName();
}
