package black.android.hardware.biometrics;

import android.os.IBinder;
import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.hardware.biometrics.IAuthService")
public interface IAuthService {
    @BClassName("android.hardware.biometrics.IAuthService$Stub")
    interface Stub {
        @BStaticMethod
        IInterface asInterface(IBinder binder);
    }
}
