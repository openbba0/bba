package black.android.app;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.app.Notification")
public interface NotificationO {
    @BField
    String mChannelId();

    @BField
    String mGroupKey();
}
