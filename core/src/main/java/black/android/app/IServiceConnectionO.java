package black.android.app;

import android.content.ComponentName;
import android.os.IBinder;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.app.IServiceConnection")
public interface IServiceConnectionO {
    @BMethod
    void connected(ComponentName ComponentName0, IBinder IBinder1, boolean boolean2);
}
