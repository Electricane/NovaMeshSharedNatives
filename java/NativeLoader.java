package novamesh;

import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

public final class NativeLoader {
    private NativeLoader() {}

    public static void loadFromJar(String libName) {
        String os = osName();
        String arch = archName();

        if (!os.equals("linux")) {
            throw new UnsupportedOperationException("Native library not supported on this OS: " + os);
        }

        String fileName = "lib" + libName + ".so";
        String resourcePath = "/natives/linux/" + arch + "/" + fileName;

        try (InputStream in = NativeLoader.class.getResourceAsStream(resourcePath)) {
            if (in == null) {
                throw new UnsatisfiedLinkError("Missing native library in JAR: " + resourcePath);
            }

            Path tempDir = Files.createTempDirectory("novamesh-natives");
            tempDir.toFile().deleteOnExit();

            Path extracted = tempDir.resolve(fileName);
            Files.copy(in, extracted, StandardCopyOption.REPLACE_EXISTING);
            extracted.toFile().deleteOnExit();

            System.load(extracted.toAbsolutePath().toString());
        } catch (Throwable t) {
            throw new RuntimeException("Unable to load " + libName + " from " + resourcePath + ": " + t.getMessage(), t);
        }
    }

    private static String osName() {
        String os = System.getProperty("os.name").toLowerCase();
        if (os.contains("linux")) return "linux";
        if (os.contains("windows")) return "windows";
        if (os.contains("mac")) return "macos";
        return os.replaceAll("[^a-z0-9]+", "_");
    }

    private static String archName() {
        String arch = System.getProperty("os.arch").toLowerCase();

        if (arch.equals("amd64") || arch.equals("x86_64")) {
            return "x86_64";
        }

        if (arch.equals("aarch64") || arch.equals("arm64")) {
            return "aarch64";
        }

        if (arch.startsWith("arm")) {
            return "arm";
        }

        return arch.replaceAll("[^a-z0-9_]+", "_");
    }
}