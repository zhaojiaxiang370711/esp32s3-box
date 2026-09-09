"""Reject malformed manifests, downgrades and cross-origin firmware URLs."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


class BoxOtaTests(unittest.TestCase):
    def test_release_validation(self):
        source = r'''
#include "box_ota_validation.h"
#include <cassert>
using namespace box_ota;
int main() {
    assert(IsNewer("2.4.3", "2.4.4"));
    assert(IsNewer("2.9.9", "2.10.0"));
    assert(!IsNewer("2.4.4", "2.4.4"));
    assert(!IsNewer("2.4.4", "2.4.3"));
    for (auto bad : {"", "2.4", "2.4.4.1", "2..4", "2.4.", "2.4.4beta", "-1.2.3",
                     "2.4.999999999999999999", "2.4.65536", " 2.4.4"}) {
        std::array<unsigned, 3> parts;
        assert(!ParseVersion(bad, parts));
    }
    assert(ValidHash(std::string(64, 'a')));
    assert(!ValidHash(std::string(63, 'a')));
    assert(!ValidHash(std::string(64, 'g')));
    assert(ValidUrl("https://ota.ainotex.com/box/firmware/2.4.4.bin", "2.4.4"));
    for (auto bad : {"http://ota.ainotex.com/box/firmware/2.4.4.bin",
                     "https://ota.ainotex.com.evil/box/firmware/2.4.4.bin",
                     "https://ota.ainotex.com/box/firmware/2.4.3.bin",
                     "https://ota.ainotex.com/box0/firmware/2.4.4.bin"})
        assert(!ValidUrl(bad, "2.4.4"));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory)
            (path / 'test.cc').write_text(source)
            subprocess.run(['g++', '-std=c++17', '-Wall', '-Werror', '-I',
                            str(ROOT / 'main/boards/alientek/atk-dnesp32s3-box'),
                            str(path / 'test.cc'), '-o', str(path / 'test')], check=True)
            subprocess.run([str(path / 'test')], check=True)

    def test_publisher_rejects_wrong_images(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location('publish_box_ota', ROOT / 'scripts/publish_box_ota.py')
        publisher = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(publisher)
        image = bytearray(4096)
        image[0] = 0xe9
        image[12:14] = (9).to_bytes(2, 'little')
        image[32:36] = (0xabcd5432).to_bytes(4, 'little')
        image[48:53] = b'2.4.4'
        image[80:87] = b'xiaozhi'
        result = publisher.manifest_for(image, '2.4.4')
        self.assertEqual(result['board'], 'atk-dnesp32s3-box')
        self.assertEqual(result['size'], len(image))
        for version in ['2.4.3', '../evil', '2.4.65536']:
            with self.assertRaises(ValueError):
                publisher.manifest_for(image, version)
        image[12] = 0
        with self.assertRaises(ValueError):
            publisher.manifest_for(image, '2.4.4')
