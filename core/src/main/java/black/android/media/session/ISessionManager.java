package black.android.media.session;

import android.os.IBinder;
import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.media.session.ISessionManager")
public interface ISessionManager {
    @BClassName("android.media.session.ISessionManager$Stub")
    interface Stub {
        @BStaticMethod
        IInterface asInterface(IBinder IBinder0);
    }
}
