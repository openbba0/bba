package black.android.app.servertransaction;

import android.os.IBinder;

import java.util.List;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.app.servertransaction.ClientTransaction")
public interface ClientTransaction {
    @BField
    List<Object> mActivityCallbacks();

    @BField
    IBinder mActivityToken();

    @BField
    Object mLifecycleStateRequest();
}
