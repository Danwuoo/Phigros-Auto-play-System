"""Build the offline Android pixel fixture with the installed SDK/JDK.

Output is gitignored under measurements/touch_fixture_android/. No Gradle download.
"""

import os
from pathlib import Path
import subprocess
import sys
import zipfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "fixtures" / "touch_android"
OUTPUT = ROOT / "measurements" / "touch_fixture_android"


def run(*args: object, environment=None) -> None:
    subprocess.run([str(argument) for argument in args], check=True, env=environment)


def main() -> None:
    sdk = Path(os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT") or
               str(Path(os.environ["LOCALAPPDATA"]) / "Android" / "Sdk"))
    build_tools = sdk / "build-tools" / "36.0.0"
    android_jar = sdk / "platforms" / "android-37.0" / "android.jar"
    java_home = Path(os.environ.get("JAVA_HOME") or
                     r"C:\Program Files\Android\Android Studio\jbr")
    os.environ["JAVA_HOME"] = str(java_home)
    binary = ".exe" if sys.platform == "win32" else ""
    javac = java_home / "bin" / ("javac" + binary)
    keytool = java_home / "bin" / ("keytool" + binary)
    for path in (build_tools / ("aapt2" + binary), android_jar, javac, keytool):
        if not path.is_file():
            raise RuntimeError(f"required SDK/JDK tool missing: {path}")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    classes = OUTPUT / "classes"
    classes.mkdir(exist_ok=True)
    source = SOURCE / "src" / "org" / "pas" / "touchfixture" / "MainActivity.java"
    run(javac, "-source", "8", "-target", "8", "-classpath", android_jar,
        "-d", classes, source)
    dex_dir = OUTPUT / "dex"
    dex_dir.mkdir(exist_ok=True)
    class_files = sorted(classes.rglob("*.class"))
    run(build_tools / ("d8.bat" if sys.platform == "win32" else "d8"),
        "--lib", android_jar, "--output", dex_dir, *class_files)
    unaligned = OUTPUT / "fixture-unaligned.apk"
    run(build_tools / ("aapt2" + binary), "link", "-I", android_jar,
        "--manifest", SOURCE / "AndroidManifest.xml", "-o", unaligned)
    with zipfile.ZipFile(unaligned, "a", compression=zipfile.ZIP_DEFLATED) as archive:
        archive.write(dex_dir / "classes.dex", "classes.dex")
    aligned = OUTPUT / "fixture-aligned.apk"
    run(build_tools / ("zipalign" + binary), "-f", "4", unaligned, aligned)
    keystore = OUTPUT / "debug.keystore"
    if not keystore.exists():
        run(keytool, "-genkeypair", "-keystore", keystore, "-storepass", "android",
            "-keypass", "android", "-alias", "fixture", "-dname", "CN=PAS Fixture",
            "-keyalg", "RSA", "-keysize", "2048", "-validity", "3650")
    final = OUTPUT / "pas-touch-fixture.apk"
    environment = dict(os.environ, PAS_FIXTURE_DEBUG_PASSWORD="android",
                       JAVA_HOME=str(java_home))
    run(build_tools / ("apksigner.bat" if sys.platform == "win32" else "apksigner"),
        "sign", "--ks", keystore, "--ks-key-alias", "fixture",
        "--ks-pass", "env:PAS_FIXTURE_DEBUG_PASSWORD",
        "--key-pass", "env:PAS_FIXTURE_DEBUG_PASSWORD", "--out", final,
        aligned, environment=environment)
    run(build_tools / ("apksigner.bat" if sys.platform == "win32" else "apksigner"),
        "verify", final, environment=environment)
    print(final)


if __name__ == "__main__":
    main()

