package black.android.widget;

import android.content.pm.ApplicationInfo;

import java.util.ArrayList;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.widget.RemoteViews")
public interface RemoteViews {
    @BField
    ArrayList<Object> mActions();

    @BField
    ApplicationInfo mApplication();

    @BField
    String mPackage();
}
