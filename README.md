# BlackBox

BlackBox runs cloned apps inside a host process on Android, without installing those APKs as normal apps. The host application id is `com.android.blackboxa`. Minimum SDK is 21, compile SDK is 35, and the app targets SDK 28.

## Modules

| Module | Role |
| --- | --- |
| `app` | Host UI |
| `core` | Virtual engine, service proxies, and guest hook entry |
| `beer` | SandHook, used to hook methods inside a guest process |
| `compiler` | Annotation processors, including `AppHook` registration |
| `reflection` | Hidden API mirrors |

## Guest hooks

`BActivityThread` calls `HookEntry.install` before the guest `Application.onCreate`. `HookEntry` only dispatches by package name. Each guest app has its own `AppHook` subclass.

A new hook is a public class with a public no-arg constructor. Rebuild `core` and the annotation processor writes `AppHookRegistry`. Do not edit `HookEntry` or the generated registry.

The subclass implements three methods:

* `packageName()` — guest package this hook applies to
* `enabled()` — return false to skip SandHook setup and `onHook`
* `onHook(Application)` — install the hooks

Use `com.scorpion.IHook.XposedHelpers` and `XC_MethodHook`. That is the API shipped in `beer`, not `de.robv.android.xposed`.

`RednoteHook` hooks `okhttp3.Request.Builder.url` for `com.xingin.xhs` and logs the URL plus a short stack. It stays off while `enabled()` returns false.

## Requirements

* Android 5.0 (API 21) or higher
* JDK 21 for `:app` and `:core` (Android Studio's JBR). `:beer`, `:compiler`, and `:reflection` compile as Java 17
* Android SDK 35
* NDK 25.2.9519653
* ABIs: `armeabi-v7a`, `arm64-v8a`

## Build

```bash
export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"
./gradlew :app:assembleDebug
```

Debug APKs are written to `app/build/outputs/apk/debug/`. The version name includes the git revision count and `git describe`.

Longer usage notes are in `Docs.md`.

## Credits

* Original framework: VirtualApp, VirtualAPK
* Native hooks: Dobby, xDL, SandHook
* Reflection: BlackReflection, FreeReflection

## License

Copyright 2022 BlackBox

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
