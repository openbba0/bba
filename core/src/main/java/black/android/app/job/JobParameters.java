package black.android.app.job;

import android.os.IBinder;
import android.os.PersistableBundle;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.app.job.JobParameters")
public interface JobParameters {
    @BField
    IBinder callback();

    @BField
    PersistableBundle extras();

    @BField
    int jobId();
}
