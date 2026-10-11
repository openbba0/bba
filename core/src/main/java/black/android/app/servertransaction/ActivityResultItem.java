package black.android.app.servertransaction;

import java.util.List;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.app.servertransaction.ActivityResultItem")
public interface ActivityResultItem {
    @BField
    List mResultInfoList();
}
