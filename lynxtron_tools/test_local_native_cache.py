import concurrent.futures
import json
import os
import random
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from local_native_cache import LocalCache, file_hash, input_key


class LocalCacheTest(unittest.TestCase):
    def test_storage_limits_corruption_and_symlinks(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "artifact"
            source.mkdir()
            (source / "binary").write_bytes(b"native")
            (source / "binary").chmod(0o755)
            (source / "link").symlink_to("binary")
            cache = LocalCache(root / "cache", 25000, 2)
            key = "a" * 64
            self.assertTrue(cache.put(key, source))
            self.assertTrue(cache.restore(key, root / "restored"))
            self.assertEqual(os.readlink(root / "restored/link"), "binary")
            self.assertTrue(os.access(root / "restored/binary", os.X_OK))
            with self.assertRaises(ValueError):
                cache.restore(key, source)
            (cache.entry(key) / "artifact.tar").write_bytes(b"corrupt")
            self.assertFalse(cache.restore(key, root / "corrupt-restore"))
            self.assertTrue(cache.put(key, source))
            (cache.entry(key) / "metadata.json").unlink()
            self.assertFalse(cache.restore(key, root / "missing-restore"))
            for index in range(6):
                self.assertTrue(cache.put(f"{index:064x}", source))
                self.assertLessEqual(len(cache.entries()), 2)
                self.assertLessEqual(sum(p.stat().st_size for p in
                                         cache.root.rglob("*") if p.is_file()), 25000)
            (source / "huge").write_bytes(b"x" * 30000)
            self.assertFalse(cache.put("b" * 64, source))

    def test_parallel_writers_and_ownership(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "binary"
            source.write_bytes(b"native")
            cache = LocalCache(root / "native", 25000, 2)
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
                self.assertTrue(all(pool.map(
                    lambda n: cache.put(f"{n:064x}", source), range(8))))
            self.assertLessEqual(len(cache.entries()), 2)
            with self.assertRaises(ValueError):
                LocalCache(root, 25000, 2)
            with self.assertRaises(ValueError):
                cache.restore("../escape", root / "bad")

    @unittest.skipUnless(shutil.which("ninja") and shutil.which("clang"),
                         "Ninja/Clang required")
    def test_real_compilation_mutations(self):
        with tempfile.TemporaryDirectory(prefix="native-cache-monkey-") as tmp:
            root = Path(tmp)
            src = root / "src"
            src.mkdir()
            (src / "main.c").write_text(
                '#include "nested.h"\n'
                '#if __has_include("optional.h")\n#include "optional.h"\n'
                '#else\n#define OPTIONAL 0\n#endif\n'
                'int main(void) { return VALUE + OPTIONAL + FLAG; }\n')
            (src / "nested.h").write_text('#include "value.h"\n')
            (src / "value.h").write_text('#define VALUE 1\n')
            build = root / "build"
            build.mkdir()
            compiler = Path(shutil.which("clang"))
            cache = LocalCache(root / "cache", 1024 * 1024, 3)
            flags = 0
            reports = []

            def run_case(name, expected_hit):
                identity = {"arch": "host", "flags": flags, "sdk": "local"}
                key = input_key([src, compiler], identity)
                destination = root / f"restore-{name}"
                hit = cache.restore(key, destination)
                self.assertEqual(hit, expected_hit, name)
                (build / "build.ninja").write_text(
                    f'rule cc\n  command = {compiler} -DFLAG={flags} '
                    f'{src / "main.c"} -o $out\n'
                    f'build probe: cc {src / "main.c"}\n')
                # Force a genuinely fresh binary for comparison even on hits.
                (build / "probe").unlink(missing_ok=True)
                subprocess.run(["ninja", "-C", str(build), "probe"],
                               check=True, stdout=subprocess.DEVNULL)
                binary = build / "probe"
                if hit:
                    self.assertEqual(file_hash(destination), file_hash(binary), name)
                    self.assertEqual(subprocess.run([str(destination)]).returncode,
                                     subprocess.run([str(binary)]).returncode)
                else:
                    self.assertTrue(cache.put(key, binary))
                reports.append({"case": name, "hit": hit})

            run_case("cold", False)
            run_case("repeat", True)
            os.utime(src / "main.c", None)
            run_case("mtime-only", True)
            (root / "demo.js").write_text("// not a native input\n")
            run_case("unrelated-js", True)
            (src / "value.h").write_text('#define VALUE 2\n')
            run_case("recursive-header", False)
            (src / "optional.h").write_text('#define OPTIONAL 3\n')
            run_case("new-optional-header", False)
            (src / "optional.h").unlink()
            run_case("remove-optional-header", True)
            flags = 1
            run_case("compile-flags", False)
            # Deterministic monkey cases: each new native value misses, then
            # an identical rerun hits and is compared with a forced rebuild.
            for index, value in enumerate(random.Random(259).sample(range(10, 100), 8)):
                (src / "value.h").write_text(f'#define VALUE {value}\n')
                run_case(f"monkey-{index}", False)
                run_case(f"monkey-{index}-repeat", True)
            before = input_key([src, compiler], {"arch": "arm64"})
            self.assertNotEqual(before, input_key([src, compiler], {"arch": "x64"}))
            self.assertNotEqual(before, input_key([src, compiler],
                                                  {"arch": "arm64", "sdk": "new"}))
            print(json.dumps({"cache_mutations": reports}))

    def test_tools_generators_and_symlink_inputs(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            native = root / "native"
            native.mkdir()
            tool = root / "compiler"
            tool.write_text("tool-v1")
            generator = native / "generate.py"
            generator.write_text("generator-v1")
            (native / "input.json").write_text("{}")
            (native / "header.h").write_text("first")
            (native / "alias.h").symlink_to("header.h")
            def key():
                return input_key([native, tool], {"target": "arm64-release"})
            previous = key()
            for path, value in [(tool, "tool-v2"), (generator, "generator-v2"),
                                (native / "input.json", '{"new":true}'),
                                (native / "header.h", "second")]:
                path.write_text(value)
                current = key()
                self.assertNotEqual(previous, current)
                previous = current
            (native / "alias.h").unlink()
            (native / "alias.h").symlink_to("missing.h")
            with self.assertRaises(FileNotFoundError):
                key()

    def test_escaping_artifact_link_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source"
            source.mkdir()
            (source / "escape").symlink_to("../../outside")
            cache = LocalCache(root / "cache", 25000, 2)
            key = "e" * 64
            self.assertTrue(cache.put(key, source))
            self.assertFalse(cache.restore(key, root / "restored"))
            self.assertFalse((root / "restored").exists())

    def test_cef_payload_layout_and_component_isolation(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            payload = root / "cef"
            framework = payload / "frameworks/Chromium Embedded Framework.framework"
            version = framework / "Versions/A"
            (version / "Resources").mkdir(parents=True)
            (version / "Chromium Embedded Framework").write_bytes(b"framework-fixture")
            (version / "Resources/icudtl.dat").write_bytes(b"resource-fixture")
            (framework / "Versions/Current").symlink_to("A")
            (framework / "Chromium Embedded Framework").symlink_to(
                "Versions/Current/Chromium Embedded Framework")
            (framework / "Resources").symlink_to("Versions/Current/Resources")
            helper = payload / "frameworks/LynxtronWebview.app/Contents/MacOS/LynxtronWebview"
            helper.parent.mkdir(parents=True)
            helper.write_bytes(b"helper-fixture")
            helper.chmod(0o755)
            (payload / "cef_extension.node").write_bytes(b"addon-fixture")
            cache = LocalCache(root / "cache", 100000, 2)
            cef_key = input_key([payload], {"component": "cef-webview", "arch": "arm64"})
            runtime_key = input_key([payload], {"component": "lynxtron", "arch": "arm64"})
            self.assertTrue(cache.put(cef_key, payload))
            self.assertFalse(cache.restore(runtime_key, root / "wrong-component"))
            self.assertTrue(cache.restore(cef_key, root / "restored"))
            for source in payload.rglob("*"):
                restored = root / "restored" / source.relative_to(payload)
                if source.is_symlink():
                    self.assertEqual(os.readlink(source), os.readlink(restored))
                elif source.is_file():
                    self.assertEqual(file_hash(source), file_hash(restored))


if __name__ == "__main__":
    unittest.main()
