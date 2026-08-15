plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("kotlin-parcelize")
}

fun releaseSigningValue(propertyName: String, envName: String): String? =
    (project.findProperty(propertyName) as String?)?.takeIf { it.isNotBlank() }
        ?: System.getenv(envName)?.takeIf { it.isNotBlank() }

android {
    namespace = "com.lmqr.ha9_comp_service"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.lmqr.ha9_comp_service2"
        minSdk = 27
        targetSdk = 35
        versionCode = 40
        versionName = "3.8.1"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    // Release signing key. Resolved from gradle properties first (e.g. ~/.gradle/gradle.properties
    // for local builds), then environment variables (CI secrets). When absent the release build
    // falls back to the debug key, which produces an APK that cannot update an installed release.
    val releaseStoreFile = releaseSigningValue("a9StoreFile", "A9_KEYSTORE")
    val releaseStorePassword = releaseSigningValue("a9StorePassword", "A9_KEYSTORE_PASSWORD")
    val releaseKeyAlias = releaseSigningValue("a9KeyAlias", "A9_KEY_ALIAS")
    val releaseKeyPassword = releaseSigningValue("a9KeyPassword", "A9_KEY_PASSWORD")
    val hasReleaseKey = listOf(
        releaseStoreFile, releaseStorePassword, releaseKeyAlias, releaseKeyPassword
    ).all { !it.isNullOrBlank() }

    signingConfigs {
        if (hasReleaseKey) {
            create("release") {
                storeFile = rootProject.file(releaseStoreFile!!)
                storePassword = releaseStorePassword
                keyAlias = releaseKeyAlias
                keyPassword = releaseKeyPassword
            }
        }
    }

    buildTypes {
        debug {
            isMinifyEnabled = false
        }

        release {
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            signingConfig = if (hasReleaseKey) {
                signingConfigs.getByName("release")
            } else {
                logger.warn(
                    "No release signing key configured (a9StoreFile / A9_KEYSTORE). " +
                        "Falling back to the debug key: the resulting APK will not install " +
                        "over an existing release build."
                )
                signingConfigs.getByName("debug")
            }
        }
    }
    buildFeatures{
        viewBinding = true
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    kotlinOptions {
        jvmTarget = "11"
    }
    lint {
        disable.add("ExpiredTargetSdkVersion")
    }
}

dependencies {

    implementation("androidx.core:core-ktx:1.15.0")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.preference:preference-ktx:1.2.1")
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.6.1")
}