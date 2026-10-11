package black.android.os;

import android.os.IBinder;
import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.os.IDeviceIdentifiersPolicyService")
public interface IDeviceIdentifiersPolicyService {
    @BClassName("android.os.IDeviceIdentifiersPolicyService$Stub")
    interface Stub {
        @BStaticMethod
        IInterface asInterface(IBinder IBinder0);
    }
}
