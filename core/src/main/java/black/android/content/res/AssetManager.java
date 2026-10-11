package black.android.content.res;

import android.content.res.Configuration;
import android.util.DisplayMetrics;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BConstructor;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.content.res.AssetManager")
public interface AssetManager {
    @BConstructor
    android.content.res.AssetManager _new();

    @BMethod
    Integer addAssetPath(String String0);

    @BMethod
    Configuration getConfiguration();

    @BMethod
    DisplayMetrics getDisplayMetrics();
}
