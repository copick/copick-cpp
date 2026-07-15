#!/usr/bin/env python
"""Download + unpack the copick sample project (the same archive copick's own test suite
uses) for testing copick-cpp against real data.

Zenodo DOI 10.5281/zenodo.19686100 -> sample_project.zip (md5 8b8941350af1f621effd4903e75255c0).
The archive contains sample_project/ (static tree), sample_overlay/ (overlay tree), and the
config files filesystem.json / filesystem_overlay_only.json.

Usage:
    python fetch_sample_data.py [dest_dir]   # default: ./sample_data
"""
import hashlib
import sys
import urllib.request
import zipfile
from pathlib import Path

URL = "https://zenodo.org/records/19686100/files/sample_project.zip?download=1"
MD5 = "8b8941350af1f621effd4903e75255c0"


def main(dest: str) -> int:
    out = Path(dest)
    out.mkdir(parents=True, exist_ok=True)
    zip_path = out / "sample_project.zip"

    if not zip_path.exists():
        print(f"downloading {URL}")
        urllib.request.urlretrieve(URL, zip_path)

    digest = hashlib.md5(zip_path.read_bytes()).hexdigest()
    if digest != MD5:
        print(f"md5 mismatch: got {digest}, expected {MD5}", file=sys.stderr)
        return 1

    print(f"extracting to {out / 'sample_project'}")
    with zipfile.ZipFile(zip_path) as zf:
        zf.extractall(out / "sample_project")

    print("done. Point a filesystem config's static_root/overlay_root at the extracted trees.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "sample_data"))
