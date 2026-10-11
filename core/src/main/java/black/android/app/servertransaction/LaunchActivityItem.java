package black.android.app.servertransaction;

import android.content.Intent;
import android.content.pm.ActivityInfo;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.app.servertransaction.LaunchActivityItem")
public interface LaunchActivityItem {
    @BField
    ActivityInfo mInfo();

    @BField
    Intent mIntent();
}
