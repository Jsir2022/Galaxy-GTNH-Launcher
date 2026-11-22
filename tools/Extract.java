import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.Collections;
import java.util.zip.ZipFile;

/** Extracts pinned upstream ZIP content into an empty, launcher-owned directory. */
public class Extract {
    public static void main(String[] args) throws IOException {
        var output = Path.of(args[1]).toAbsolutePath().normalize();
        try (var zip = new ZipFile(args[0])) {
            for (var entry : Collections.list(zip.entries())) {
                var name = entry.getName();
                if (!name.startsWith(args[2])) continue;
                var relative = name.substring(args[2].length());
                if (relative.isEmpty() || Arrays.stream(args, 3, args.length).anyMatch(relative::startsWith)) {
                    continue;
                }
                if (relative.contains("\\") || relative.contains(":")) {
                    throw new IOException("Invalid archive path");
                }
                var target = output.resolve(relative).normalize();
                if (!target.startsWith(output) || target.equals(output)) {
                    throw new IOException("Invalid archive path");
                }
                if (entry.isDirectory()) {
                    Files.createDirectories(target);
                } else {
                    Files.createDirectories(target.getParent());
                    try (var input = zip.getInputStream(entry)) {
                        Files.copy(input, target);
                    }
                }
            }
        }
    }
}
