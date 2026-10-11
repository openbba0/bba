package black.android.app;

import android.os.IBinder;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.app.ClientTransactionHandler")
public interface ClientTransactionHandler {
    @BMethod
    Object getActivityClient(IBinder IBinder0);
}
