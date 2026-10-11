package black.android.security.net.config;

import android.content.Context;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.security.net.config.NetworkSecurityConfigProvider")
public interface NetworkSecurityConfigProvider {
    @BStaticMethod
    void install(Context Context0);
}
