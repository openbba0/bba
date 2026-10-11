package black.android.app;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticField;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.app.ActivityTaskManager")
public interface ActivityTaskManager {

    @BStaticMethod
    Object getService();

    @BStaticField
    Object IActivityTaskManagerSingleton();
}
