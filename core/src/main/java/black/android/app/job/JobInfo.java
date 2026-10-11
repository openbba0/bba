package black.android.app.job;

import android.content.ComponentName;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.app.job.JobInfo")
public interface JobInfo {
    @BField
    long flexMillis();

    @BField
    long intervalMillis();

    @BField
    int jobId();

    @BField
    ComponentName service();
}
