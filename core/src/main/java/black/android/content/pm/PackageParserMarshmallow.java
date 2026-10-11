package black.android.content.pm;

import android.content.pm.PackageParser;
import android.content.pm.PackageParser.Package;

import java.io.File;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BConstructor;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.content.pm.PackageParser")
public interface PackageParserMarshmallow {
    @BConstructor
    PackageParser _new();
















    @BMethod
    void collectCertificates(Package p, int flags);

    @BMethod
    Package parsePackage(File File0, int int1);
}
