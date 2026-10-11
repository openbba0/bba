package black.android.view;

import java.io.File;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.renderscript.RenderScript")
public interface RenderScript {
    @BStaticMethod
    void setupDiskCache(File File0);
}
