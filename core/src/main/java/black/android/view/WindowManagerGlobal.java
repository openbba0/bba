package black.android.view;

import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticField;

@BClassName("android.view.WindowManagerGlobal")
public interface WindowManagerGlobal {
    @BStaticField
    int ADD_PERMISSION_DENIED();

    @BStaticField
    IInterface sWindowManagerService();
}
