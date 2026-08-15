Automatically patch system.img for use with Hisense A9, adding eink features support
For advanced users, check xda for pre-patched images.

To patch your own treble based system.img you simply have to:

- Download a treble-droid based system.img you want to use
- Download and extract a9_system_patcher.zip from https://github.com/damianmqr/a9_accessibility_service/releases/latest
- Go in terminal to the folder containing unzipped files
- Run `sudo bash patch_system_img.sh /path/to/system.img`
- system_patched.img will be saved in the same directory, ready to be flashed

### Patching on macOS

The patcher loop-mounts an ext4 image and writes SELinux labels into `security.selinux`
extended attributes, and macOS can do neither. So on any non-Linux host the script runs
itself inside a Docker container instead, built from the `Dockerfile` next to it. Install
Docker Desktop or OrbStack, then run it **without** sudo:

```bash
bash patch_system_img.sh /path/to/system.img
```

The container needs `--privileged` for `/dev/loop`, which the script passes for you.
`system_patched.img` and `hisensea9_magisk_module.zip` are written next to the script as
usual. Set `PATCHER_DOCKER=1` to force the container on Linux too, or `PATCHER_DOCKER=0` to
force a native run.

Flashing the resulting file is done the same as any other system.img
**(make sure your bootloader is unlocked! and you have disabled vbmeta verification by reflashing with `fastboot flash vbmeta --disable-verity --disable-verification vbmeta.img`)**

- reboot to fastbootd using `adb reboot fastboot` (or any other method)
- run `fastboot flash system system_patched.img`
- after that's done, run `fastboot -w`
- run `fastboot reboot` and give it a few minutes

Default E-ink features Usage:

**Single Press E-Ink Button** - Refresh Screen

**Double Press E-Ink Button** - Open E-Ink Menu with settings for Per-App refresh modes.

These mappings can be easily changed in the E-Ink Settings app

## Updating an already patched phone without reflashing

Two CI workflows:

- **patched image (image + apk + daemon)** — full build. Compiles the daemon and the settings
  app from source, patches them into the GSI, produces the flashable image and the Magisk
  module. Needed whenever `vndk.rc`, the framework patches or the image itself change.
- **quick build (apk + daemon + magisk module)** — about a minute, no GSI download. Produces
  only what can be installed on a phone that already runs a patched image.

The quick build is the one to use while iterating on the daemon or the app:

```bash
adb install -r a9service.apk
```

and for the daemon, flash `a9_eink_daemon_module.zip` in Magisk and reboot.

That module replaces `/system/bin/a9_eink_server` systemlessly. It deliberately uses the module
id `hisense_a9_eink_daemon`, *not* `hisense_a9_augmented` — installing it under the same id as
the full module would replace it and silently drop the SystemUI, framework and services
patches. Both can be installed side by side.

It only swaps the binary, it cannot register the init service: `service a9_eink_server` lives in
`/system/etc/init/vndk.rc`, and init parses that long before Magisk mounts anything. So it
requires a system image that was already patched, and `customize.sh` aborts if it does not find
the service there. The module also restores the `u:object_r:phhsu_exec:s0` label, without which
init cannot transition into the `phhsu_daemon` domain and the daemon never starts.

## Updating the E-Ink Settings app without reflashing

The settings app ships inside the image at `/system/priv-app/a9service.apk`, but it can be
updated on its own. Download `a9service.apk` from the release you want and install it:

```bash
adb install -r a9service.apk
```

Android keeps the update in `/data` and the app keeps its privileged permissions, so no
reflash and no root are needed. A factory reset drops the update and falls back to the
version baked into the image.

This only works when the new APK is signed with the same key as the one in your image. APKs
built by CI from a given release are; a locally built debug APK is not, and installing one
fails with `INSTALL_FAILED_UPDATE_INCOMPATIBLE`.

## Building a release APK locally

Release builds are signed with a key resolved from Gradle properties (or the matching
environment variables `A9_KEYSTORE`, `A9_KEYSTORE_PASSWORD`, `A9_KEY_ALIAS`,
`A9_KEY_PASSWORD`). Put them in `~/.gradle/gradle.properties` to keep them out of the repo:

```
a9StoreFile=/absolute/path/to/release.jks
a9StorePassword=...
a9KeyAlias=...
a9KeyPassword=...
```

Then `./gradlew :app:assembleRelease`. Without these the build falls back to the debug key
and warns; the resulting APK still runs but cannot update an installed release.

CI reads the same key from the repository secrets `A9_KEYSTORE_BASE64` (the keystore,
base64-encoded), `A9_KEYSTORE_PASSWORD`, `A9_KEY_ALIAS` and `A9_KEY_PASSWORD`. When
`A9_KEYSTORE_BASE64` is unset it skips the rebuild and patches in the committed
`a9service.apk` instead.

Remember to bump `versionCode` in [app/build.gradle.kts](app/build.gradle.kts) for every
release, otherwise the update will not install over the previous one.

## Licensing

### MIT License
Most of the project is licensed under the MIT License unless specified otherwise

The code and releases are provided “as is,” without any express or implied warranty of any kind including warranties of merchantability, non-infringement, title, or fitness for a particular purpose.

### Apache License 2.0
The file `HardwareGestureDetector.kt` includes code derived from AOSP. The original code is subject to the following license:

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

Modifications and additions to the original code are licensed under the MIT License
