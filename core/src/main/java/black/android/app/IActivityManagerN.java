package black.android.app;

import android.content.Intent;
import android.os.IBinder;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.app.IActivityManager")
public interface IActivityManagerN {
    @BMethod
    Boolean finishActivity(IBinder IBinder0, int int1, Intent Intent2, int int3);
}
