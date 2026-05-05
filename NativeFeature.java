package novamesh;

public final class NativeFeature {
    private NativeFeature() {}

    public static boolean isLinux() {
        return System.getProperty("os.name").toLowerCase().contains("linux");
    }

    public static boolean isWindows() {
        return System.getProperty("os.name").toLowerCase().contains("windows");
    }

    public static boolean isArm64() {
        String arch = System.getProperty("os.arch").toLowerCase();
        return arch.contains("aarch64") || arch.contains("arm64");
    }

    public static boolean canUseMaliGpuStats() {
        return isLinux();
    }
}