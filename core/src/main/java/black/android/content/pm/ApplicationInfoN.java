package black.android.content.pm;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.content.pm.ApplicationInfo")
public interface ApplicationInfoN {
    @BField
    String credentialEncryptedDataDir();

    @BField
    String credentialProtectedDataDir();

    @BField
    String deviceEncryptedDataDir();

    @BField
    String deviceProtectedDataDir();
}
