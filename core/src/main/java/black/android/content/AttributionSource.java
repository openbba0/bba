package black.android.content;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;
import com.android.blackreflection.annotation.BMethod;


@BClassName("android.content.AttributionSource")
public interface AttributionSource {
    @BField
    Object mAttributionSourceState();

    @BMethod
    Object getNext();
}
