package black.android.view;

import java.io.File;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.view.ThreadedRenderer")
public interface ThreadedRenderer {
    @BStaticMethod
    void setupDiskCache(File File0);
}
