package black.com.android.internal.telephony;

import android.os.IBinder;
import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("com.android.internal.telephony.ITelephonyRegistry")
public interface ITelephonyRegistry {
    @BClassName("com.android.internal.telephony.ITelephonyRegistry$Stub")
    interface Stub {
        @BStaticMethod
        IInterface asInterface(IBinder IBinder0);
    }
}
