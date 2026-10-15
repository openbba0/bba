# Add project specific ProGuard rules here.
# You can control the set of applied configuration files using the
# proguardFiles setting in build.gradle.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# If your project uses WebView with JS, uncomment the following
# and specify the fully qualified class name to the JavaScript interface
# class:
#-keepclassmembers class fqcn.of.javascript.interface.for.webview {
#   public *;
#}

# Uncomment this to preserve the line number information for
# debugging stack traces.
#-keepattributes SourceFile,LineNumberTable

# If you keep the line number information, uncomment this to
# hide the original source file name.
#-renamesourcefileattribute SourceFile

-keep class com.android.bb.** {*; }
-keep class com.android.jnihook.** {*; }
-keep class com.swift.sandhook.** { *; }
-keep class com.scorpion.IHook.** { *; }
-keep class mirror.** {*; }
-keep class android.** {*; }
-keep class com.android.** {*; }

-keep class com.android.blackreflection.** {*; }
-keep @com.android.blackreflection.annotation.BClass class * {*;}
-keep @com.android.blackreflection.annotation.BClassName class * {*;}
-keep @com.android.blackreflection.annotation.BClassNameNotProcess class * {*;}
-keepclasseswithmembernames class * {
    @com.android.blackreflection.annotation.BField.* <methods>;
    @com.android.blackreflection.annotation.BFieldNotProcess.* <methods>;
    @com.android.blackreflection.annotation.BFieldSetNotProcess.* <methods>;
    @com.android.blackreflection.annotation.BFieldCheckNotProcess.* <methods>;
    @com.android.blackreflection.annotation.BMethod.* <methods>;
    @com.android.blackreflection.annotation.BStaticField.* <methods>;
    @com.android.blackreflection.annotation.BStaticMethod.* <methods>;
    @com.android.blackreflection.annotation.BMethodCheckNotProcess.* <methods>;
    @com.android.blackreflection.annotation.BConstructor.* <methods>;
    @com.android.blackreflection.annotation.BConstructorNotProcess.* <methods>;
}