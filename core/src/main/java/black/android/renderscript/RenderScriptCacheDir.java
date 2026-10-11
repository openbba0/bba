package black.android.renderscript;

import java.io.File;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.renderscript.RenderScriptCacheDir")
public interface RenderScriptCacheDir {
    @BStaticMethod
    void setupDiskCache(File File0);
}
