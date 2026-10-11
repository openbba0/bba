package black.android.content.pm;


import android.content.pm.PackageParser;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.content.pm.PackageParser")
public interface PackageParserPie {
    @BStaticMethod
    void collectCertificates(PackageParser.Package p, boolean skipVerify);
}
