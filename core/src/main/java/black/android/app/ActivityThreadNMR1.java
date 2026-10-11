package black.android.app;

import android.os.IBinder;

import java.util.List;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.app.ActivityThread")
public interface ActivityThreadNMR1 {
    @BMethod
    void performNewIntents(IBinder IBinder0, List List1, boolean boolean2);
}
