package black.android.app;

import android.graphics.drawable.Icon;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.app.Notification")
public interface NotificationM {
    @BField
    Icon mLargeIcon();

    @BField
    Icon mSmallIcon();
}
