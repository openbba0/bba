package black.android.rms.resource;

import java.util.List;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.rms.resource.ReceiverResource")
public interface ReceiverResourceN {
    @BField
    List<String> mWhiteList();
}
