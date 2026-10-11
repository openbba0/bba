package black.android.view;

import java.io.File;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.view.HardwareRenderer")
public interface HardwareRenderer {
    @BStaticMethod
    void setupDiskCache(File File0);
}
