package black.android.app;

import android.os.IBinder;
import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.app.ApplicationThreadNative")
public interface ApplicationThreadNative {
    @BStaticMethod
    IInterface asInterface(IBinder IBinder0);
}
