"""The 16-bit BOX output must carry the same signed PCM sample in both I2S slots."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class BoxAudioTests(unittest.TestCase):
    def test_stereo_packing_mute_full_scale_and_volume(self):
        source = r'''
#include "box_audio_samples.h"
#include <cassert>
#include <vector>
int main() {
    std::vector<int16_t> input(65536), output(131072);
    for (int i = 0; i < 65536; ++i) input[i] = i - 32768;
    for (int volume : {-1, 0, 10, 25, 50, 70, 99, 100, 101}) {
        box_audio::PackStereo(input.data(), output.data(), input.size(), volume);
        int level = std::clamp(volume, 0, 100);
        for (size_t i = 0; i < input.size(); ++i) {
            assert(output[2*i] == output[2*i+1]);
            assert(output[2*i] == int32_t(input[i]) * level * level / 10000);
            if (volume == 100) assert(output[2*i] == input[i]);
            if (volume == 0) assert(output[2*i] == 0);
        }
    }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'test.cc').write_text(source)
            subprocess.run(['g++', '-std=c++17', '-Wall', '-Werror', '-I',
                            str(ROOT / 'main/boards/alientek/atk-dnesp32s3-box'),
                            str(path / 'test.cc'), '-o', str(path / 'test')], check=True)
            subprocess.run([str(path / 'test')], check=True)
