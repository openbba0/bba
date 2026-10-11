package black.android.service.notification;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.service.notification.StatusBarNotification")
public interface StatusBarNotification {
    @BField
    Integer id();

    @BField
    String opPkg();

    @BField
    String pkg();

    @BField
    String tag();
}
