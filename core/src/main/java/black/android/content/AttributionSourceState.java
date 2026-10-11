package black.android.content;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;


@BClassName("android.content.AttributionSourceState")
public interface AttributionSourceState {
    @BField
    String packageName();

    @BField
    int uid();
}
