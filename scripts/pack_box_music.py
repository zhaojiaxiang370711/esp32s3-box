#!/usr/bin/env python3
"""Convert three local MP3s into indexed Opus assets, preserving an existing BOX resource image."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def opus_packets(ogg):
    packets, pending = [], bytearray()
    cursor = 0
    while cursor < len(ogg):
        if ogg[cursor:cursor + 4] != b'OggS' or cursor + 27 > len(ogg):
            raise ValueError('Invalid Ogg page')
        count = ogg[cursor + 26]
        segments = ogg[cursor + 27:cursor + 27 + count]
        if len(segments) != count:
            raise ValueError('Truncated Ogg table')
        cursor += 27 + count
        for length in segments:
            if cursor + length > len(ogg):
                raise ValueError('Truncated Ogg packet')
            pending.extend(ogg[cursor:cursor + length]); cursor += length
            if length < 255:
                packets.append(bytes(pending)); pending.clear()
    if pending or len(packets) < 3 or not packets[0].startswith(b'OpusHead') or not packets[1].startswith(b'OpusTags'):
        raise ValueError('Incomplete Opus stream')
    return packets[2:]


def index_packets(packets):
    offsets, payload = [0], bytearray()
    for packet in packets:
        if not 1 <= len(packet) <= 2048:
            raise ValueError('Invalid Opus packet size')
        payload.extend(packet); offsets.append(len(payload))
    return (struct.pack('<4sIIII', b'BXM1', 24000, 60, len(packets), len(packets) * 60)
            + struct.pack('<' + 'I' * len(offsets), *offsets) + payload)


def unpack_assets(data):
    count, checksum, length = struct.unpack_from('<III', data)
    if count > 4096 or length > len(data) - 12 or sum(data[12:12 + length]) & 0xffff != checksum or count * 44 > length:
        raise ValueError('Invalid base resource image')
    files = {}
    for i in range(count):
        name, size, offset, width, height = struct.unpack_from('<32sIIHH', data, 12 + i * 44)
        name = name.split(b'\0')[0].decode()
        start = 12 + count * 44 + offset
        if start + size + 2 > 12 + length or data[start:start + 2] != b'ZZ':
            raise ValueError('Invalid resource entry')
        files[name] = (data[start + 2:start + 2 + size], width, height)
    return files


def pack_assets(files):
    table, payload = bytearray(), bytearray()
    for name, (data, width, height) in files.items():
        if len(name.encode()) >= 32:
            raise ValueError('Asset name too long')
        table.extend(struct.pack('<32sIIHH', name.encode(), len(data), len(payload), width, height))
        payload.extend(b'ZZ' + data)
    body = table + payload
    result = struct.pack('<III', len(files), sum(body) & 0xffff, len(body)) + body
    if len(result) > 8 * 1024 * 1024:
        raise ValueError('Music and existing assets exceed the 8 MiB resource partition')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('songs', nargs=3, type=Path, help='Ordered: Lacrimosa, Pull The Trigger, Titans')
    args = parser.parse_args()
    files = unpack_assets(args.base.read_bytes())
    with tempfile.TemporaryDirectory() as directory:
        for i, source in enumerate(args.songs):
            path = Path(directory) / f'{i}.opus'
            subprocess.run(['ffmpeg', '-nostdin', '-v', 'error', '-i', str(source), '-map', '0:a:0', '-vn',
                            '-ac', '1', '-ar', '24000', '-c:a', 'libopus', '-b:a', '64k', '-vbr', 'off',
                            '-frame_duration', '60', '-application', 'audio', str(path)], check=True)
            data = index_packets(opus_packets(path.read_bytes()))
            files[f'box_music_{i}.bxm'] = (data, 0, 0)
            print(f'Track {i + 1}: {source.name} -> {len(data)} bytes')
    result = pack_assets(files)
    assert unpack_assets(result) == files
    args.output.write_bytes(result)
    print(f'Resource image: {len(result)} / {8 * 1024 * 1024} bytes; {len(files)} assets')


if __name__ == '__main__':
    main()
