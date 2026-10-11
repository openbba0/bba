package black.android.app;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;
import com.android.blackreflection.annotation.BStaticField;

@BClassName("android.app.WallpaperManager")
public interface WallpaperManager {
    @BStaticField
    Object sGlobals();

    @BClassName("android.app.WallpaperManager$Globals")
    interface Globals {
        @BField
        Object mService();
    }
}
