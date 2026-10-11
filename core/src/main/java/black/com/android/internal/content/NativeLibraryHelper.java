package black.com.android.internal.content;

import java.io.File;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;
import com.android.blackreflection.annotation.BParamClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("com.android.internal.content.NativeLibraryHelper")
public interface NativeLibraryHelper {
    @BStaticMethod
    Integer copyNativeBinaries(Handle Handle0, File File1, String String2);

    @BStaticMethod
    Integer findSupportedAbi(Handle Handle0, @BParamClassName("[Ljava.lang.String;") String[] strings);

    @BClassName("com.android.internal.content.NativeLibraryHelper$Handle")
    interface Handle {
        @BField
        boolean extractNativeLibs();

        @BStaticMethod
        Object create(File File0);
    }
}
