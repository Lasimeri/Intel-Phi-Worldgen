plugins {
    id("com.gtnewhorizons.gtnhconvention")
}

// The convention's Java 25 run tasks ask for a JetBrains Runtime (vendor JETBRAINS, for its hotswap agent).
// This project runs them on the system's OpenJDK 25 instead (jdk25-openjdk), with
// `-x setupHotswapAgent25`, since nothing here uses hotswap.
for (name in listOf("runServer25", "runClient25")) {
    tasks.named<JavaExec>(name) {
        javaLauncher.set(javaToolchains.launcherFor { languageVersion.set(JavaLanguageVersion.of(25)) })
    }
}
