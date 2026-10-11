package black.android.app;

import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticField;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.app.NotificationManager")
public interface NotificationManager {
    @BStaticField
    IInterface sService();

    @BStaticMethod
    IInterface getService();
}
