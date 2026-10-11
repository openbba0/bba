package black.android.app;

import java.util.List;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.app.NotificationChannelGroup")
public interface NotificationChannelGroup {
    @BField
    List<android.app.NotificationChannel> mChannels();

    @BField
    String mId();
}
