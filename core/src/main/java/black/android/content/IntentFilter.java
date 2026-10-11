package black.android.content;

import java.util.List;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.content.IntentFilter")
public interface IntentFilter {
    @BField
    List<String> mActions();

    @BField
    List<String> mCategories();
}
