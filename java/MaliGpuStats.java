package novamesh;

@NativeTarget(
        name = "maligpustats",
        os = "linux",
        source = "src/main/native/linux/maligpustats.c"
)

public final class MaliGpuStats {
    public static boolean isAvailable = false;
    static {
        try {
            NativeLoader.loadFromJar("maligpustats");
            isAvailable = true;
        } catch (UnsatisfiedLinkError e) {
            System.out.println("[NVMesh] Unable to load maligpustats -> [ " + e.getMessage() + " ] -> Marked as Unavailable.");
            isAvailable = false;
        }
    }

    private MaliGpuStats() {}

    private static native String getGpuStatsJsonNative();

    public static String getGpuStatsJson() {
        try {
            return getGpuStatsJsonNative();
        } catch (Throwable t) {
            return "{\"available\":false,\"reason\":\"" + t.getMessage() + "\"}";
        }
    }

    public static boolean isAvailable() {
        return isAvailable;
    }
}
