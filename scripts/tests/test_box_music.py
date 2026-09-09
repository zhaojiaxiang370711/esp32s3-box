"""Validate the offline music index and resource pack without hardware."""
import importlib.util
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('pack_box_music', ROOT / 'scripts/pack_box_music.py')
packer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packer)


class BoxMusicTests(unittest.TestCase):
    def test_preserves_resources_and_rejects_corruption(self):
        original = {'index.json': (b'{"version":1}', 0, 0), 'icon.bin': (b'1234', 12, 20)}
        data = packer.pack_assets(original)
        self.assertEqual(packer.unpack_assets(data), original)
        files = packer.unpack_assets(data)
        files['box_music_0.bxm'] = (packer.index_packets([b'abc', b'defg']), 0, 0)
        restored = packer.unpack_assets(packer.pack_assets(files))
        for name, value in original.items():
            self.assertEqual(restored[name], value)
        with self.assertRaises(ValueError):
            packer.unpack_assets(data[:-1])
        with self.assertRaises(ValueError):
            packer.pack_assets({'oversize': (b'0' * (8 * 1024 * 1024), 0, 0)})

    def test_device_index_parser(self):
        source = r'''
#include "box_music_data.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
int main(int argc, char** argv) {
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(input), {}};
    box_music::Track track;
    assert(track.Load(bytes.data(), bytes.size()));
    assert(track.Count() == 2);
    size_t size;
    assert(track.Packet(1, size)[0] == 'd' && size == 4);
    assert(track.Packet(2, size) == nullptr);
    assert(!track.Load(bytes.data(), bytes.size() - 1));
    bytes[24] = 0;
    assert(!track.Load(bytes.data(), bytes.size()));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'test.cc').write_text(source)
            (path / 'track.bxm').write_bytes(packer.index_packets([b'abc', b'defg']))
            subprocess.run(['g++', '-std=c++17', '-Wall', '-Werror', '-I',
                            str(ROOT / 'main/boards/alientek/atk-dnesp32s3-box'),
                            str(path / 'test.cc'), '-o', str(path / 'test')], check=True)
            subprocess.run([str(path / 'test'), str(path / 'track.bxm')], check=True)
