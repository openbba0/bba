package black.android.app;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.app.NotificationChannel")
public interface NotificationChannel {
    @BField
    String mId();
}
