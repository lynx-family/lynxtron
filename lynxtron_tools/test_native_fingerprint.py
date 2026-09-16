"""Real CMake/Ninja local experiment; no mocked build graph."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from native_fingerprint import fingerprint


@unittest.skipUnless(shutil.which("cmake") and shutil.which("ninja")
                     and shutil.which("clang"), "CMake/Ninja/Clang required")
class FingerprintTest(unittest.TestCase):
    def test_real_build_inputs(self):
        with tempfile.TemporaryDirectory(prefix="native-fingerprint-") as tmp:
            root = Path(tmp)
            build = root / "build"
            (root / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\n"
                "project(fingerprint C)\n"
                "add_executable(probe main.c)\n"
                "target_compile_definitions(probe PRIVATE FLAG=${FLAG})\n")
            (root / "value.h").write_text("#define VALUE 0\n")
            (root / "main.c").write_text(
                '#include "value.h"\nint main(void) { return VALUE + FLAG; }\n')
            compiler = shutil.which("clang")

            def configure(flag):
                subprocess.run(["cmake", "-S", str(root), "-B", str(build),
                                "-G", "Ninja", f"-DCMAKE_C_COMPILER={compiler}",
                                f"-DFLAG={flag}"], check=True,
                               stdout=subprocess.DEVNULL)

            def compile_and_hash():
                subprocess.run(["ninja", "-C", str(build), "probe"],
                               check=True, stdout=subprocess.DEVNULL)
                return fingerprint(build, "probe", [compiler], "local-test")

            configure(0)
            with self.assertRaisesRegex(ValueError, "Build first"):
                fingerprint(build, "probe", [compiler], "local-test")
            initial = compile_and_hash()
            self.assertEqual(initial, compile_and_hash())
            (root / "demo.js").write_text("// unrelated frontend change\n")
            self.assertEqual(initial, compile_and_hash())
            (root / "value.h").write_text("#define VALUE 1\n")
            header = compile_and_hash()
            self.assertNotEqual(initial["fingerprint"], header["fingerprint"])
            (root / "main.c").write_text(
                '#include "value.h"\nint main(void) { return VALUE + FLAG + 1; }\n')
            source = compile_and_hash()
            self.assertNotEqual(header["fingerprint"], source["fingerprint"])
            configure(1)
            flags = compile_and_hash()
            self.assertNotEqual(source["fingerprint"], flags["fingerprint"])
            changed_sdk = fingerprint(build, "probe", [compiler], "other-sdk")
            self.assertNotEqual(flags["fingerprint"], changed_sdk["fingerprint"])

            # A new optional include is absent from the *old* dependency log.
            # Demonstrate the unsafe hit rather than pretending the graph is
            # sufficient for cache authorization.
            (root / "main.c").write_text(
                '#if __has_include("optional.h")\n#include "optional.h"\n'
                '#else\n#define OPTIONAL 0\n#endif\n'
                'int main(void) { return OPTIONAL; }\n')
            before_optional = compile_and_hash()
            (root / "optional.h").write_text('#define OPTIONAL 3\n')
            undiscovered = fingerprint(build, "probe", [compiler], "local-test")
            self.assertEqual(before_optional, undiscovered,
                             "Documents the known dependency-discovery hole")
            # Ninja timestamp checking also cannot detect a formerly absent
            # header. Force recompilation by changing a tracked source.
            with (root / "main.c").open("a") as source_file:
                source_file.write("// force dependency rediscovery\n")
            discovered = compile_and_hash()
            self.assertTrue(any(p.endswith("optional.h")
                                for p in discovered["manifest"]["files"]))
            self.assertEqual(subprocess.run([str(build / "probe")]).returncode, 3)

            cold = root / "cold-build"
            subprocess.run(["cmake", "-S", str(root), "-B", str(cold),
                            "-G", "Ninja", f"-DCMAKE_C_COMPILER={compiler}",
                            "-DFLAG=1"], check=True, stdout=subprocess.DEVNULL)
            with self.assertRaisesRegex(ValueError, "Build first"):
                fingerprint(cold, "probe", [compiler], "local-test")


if __name__ == "__main__":
    unittest.main()
