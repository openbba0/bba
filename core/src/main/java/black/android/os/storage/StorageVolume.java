package black.android.os.storage;

import java.io.File;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.os.storage.StorageVolume")
public interface StorageVolume {
    @BField
    File mInternalPath();

    @BField
    File mPath();
}
