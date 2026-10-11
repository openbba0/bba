package black.android.app;


import android.app.ActivityThread;
import android.app.Application;
import android.content.Context;
import android.os.IBinder;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BMethod;
import com.android.blackreflection.annotation.BParamClass;

@BClassName("android.app.Service")
public interface Service {
    @BMethod
    void attach(Context context,
                @BParamClass(ActivityThread.class) Object thread, String className, IBinder token,
                Application application, Object activityManager);
}
