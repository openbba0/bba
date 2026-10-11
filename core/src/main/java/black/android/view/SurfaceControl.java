package black.android.view;

import android.graphics.Bitmap;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.view.SurfaceControl")
public interface SurfaceControl {
    @BStaticMethod
    Bitmap screnshot(int int0, int int1);
}
