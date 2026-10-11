package black.android.app;

import java.io.File;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.app.ContextImpl")
public interface ContextImplICS {
    @BField
    File mExternalCacheDir();

    @BField
    File mExternalFilesDir();
}
