#!/usr/bin/env python3
"""Publish one BOX app image, verifying its version and hash before switching latest.json."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import uuid


def manifest_for(data, version):
    if not re.fullmatch(r'\d{1,5}\.\d{1,5}\.\d{1,5}', version):
        raise ValueError('Version must be three numeric components')
    if any(int(part) > 65535 for part in version.split('.')):
        raise ValueError('Version component exceeds 65535')
    if not 4096 <= len(data) <= 0x3f0000 or data[0] != 0xe9:
        raise ValueError('Expected an ESP32-S3 BOX application image, not a merged flash image')
    if int.from_bytes(data[12:14], 'little') != 9:
        raise ValueError('Image chip must be ESP32-S3')
    if int.from_bytes(data[32:36], 'little') != 0xabcd5432:
        raise ValueError('App description is missing')
    if data[48:80].split(b'\0')[0].decode() != version:
        raise ValueError('Image version differs from release version')
    if data[80:112].split(b'\0')[0] != b'xiaozhi':
        raise ValueError('Unexpected app project name')
    return dict(board='atk-dnesp32s3-box', version=version,
                url=f'https://ota.ainotex.com/box/firmware/{version}.bin',
                size=len(data), sha256=hashlib.sha256(data).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware', required=True, type=Path)
    parser.add_argument('--version', required=True)
    parser.add_argument('--host', default='root@47.239.235.154')
    parser.add_argument('--identity', type=Path)
    args = parser.parse_args()
    manifest = manifest_for(args.firmware.read_bytes(), args.version)
    options = ['-o', 'BatchMode=yes']
    if args.identity:
        options += ['-o', 'IdentitiesOnly=yes', '-i', str(args.identity)]
    ssh = ['ssh', *options, args.host]
    scp = ['scp', '-q', *options]
    base = '/var/www/esp32s3-ota/box'
    temporary = f'{base}/firmware/.upload-{uuid.uuid4().hex}'
    final = f'{base}/firmware/{args.version}.bin'
    latest_tmp = f'{base}/.latest-{uuid.uuid4().hex}.json'
    quote = shlex.quote
    try:
        subprocess.run([*scp, str(args.firmware), f'{args.host}:{temporary}'], check=True)
        remote_hash = subprocess.check_output([*ssh, f'sha256sum {quote(temporary)}'], text=True).split()[0]
        if remote_hash != manifest['sha256']:
            raise RuntimeError('Uploaded firmware hash mismatch')
        # Never replace an existing version with different bytes.
        command = (f'if test -e {quote(final)}; then '
                   f'test "$(sha256sum {quote(final)} | cut -d " " -f 1)" = {quote(remote_hash)} && '
                   f'rm {quote(temporary)}; else chmod 644 {quote(temporary)} && '
                   f'mv {quote(temporary)} {quote(final)}; fi')
        subprocess.run([*ssh, command], check=True)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'latest.json'
            path.write_text(json.dumps(manifest, indent=2) + '\n')
            subprocess.run([*scp, str(path), f'{args.host}:{latest_tmp}'], check=True)
        subprocess.run([*ssh, f'chmod 644 {quote(latest_tmp)} && mv {quote(latest_tmp)} {base}/latest.json'], check=True)
    finally:
        subprocess.run([*ssh, f'rm -f {quote(temporary)} {quote(latest_tmp)}'], check=False)
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
