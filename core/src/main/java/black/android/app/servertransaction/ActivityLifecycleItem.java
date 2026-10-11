package black.android.app.servertransaction;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.app.servertransaction.ActivityLifecycleItem")
public interface ActivityLifecycleItem {
    @BMethod
    Integer getTargetState();
}
