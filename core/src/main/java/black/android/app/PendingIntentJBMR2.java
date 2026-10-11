package black.android.app;

import android.content.Intent;
import android.os.IBinder;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BConstructor;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.app.PendingIntent")
public interface PendingIntentJBMR2 {
    @BConstructor
    PendingIntentJBMR2 _new(IBinder IBinder0);

    @BMethod
    Intent getIntent();
}
