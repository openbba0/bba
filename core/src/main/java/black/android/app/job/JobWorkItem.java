package black.android.app.job;

import android.content.Intent;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BConstructor;
import com.android.blackreflection.annotation.BField;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.app.job.JobWorkItem")
public interface JobWorkItem {
    @BConstructor
    JobWorkItem _new(Intent Intent0);

    @BField
    int mDeliveryCount();

    @BField
    Object mGrants();

    @BField
    int mWorkId();

    @BMethod
    Intent getIntent();
}
