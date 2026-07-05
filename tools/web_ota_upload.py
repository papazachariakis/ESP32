#!/usr/bin/env python3
"""POST firmware.bin to ESP32 web /api/ota (works on firmware with web OTA)."""

import argparse
import os
import sys

try:
    import requests
except ImportError:
    import subprocess

    subprocess.check_call([sys.executable, "-m", "pip", "install", "requests", "-q"])
    import requests


def main():
    ap = argparse.ArgumentParser(description="Upload firmware via ESP32 HTTP /api/ota")
    ap.add_argument("--host", required=True, help="ESP32 reachable host, e.g. 203.0.113.5:8080")
    ap.add_argument("--bin", default=os.path.join(os.path.dirname(__file__), "..", "docs", "firmware.bin"))
    args = ap.parse_args()

    host = args.host.strip()
    if not host.startswith("http"):
        host = "http://" + host
    url = host.rstrip("/") + "/api/ota"
    bin_path = os.path.abspath(args.bin)
    if not os.path.isfile(bin_path):
        print("ERROR: missing", bin_path)
        return 1

    size = os.path.getsize(bin_path)
    print(f"POST {size} bytes -> {url}")
    with open(bin_path, "rb") as f:
        r = requests.post(url, files={"firmware": ("firmware.bin", f, "application/octet-stream")}, timeout=300)
    print("HTTP", r.status_code, r.text[:200])
    return 0 if r.status_code == 200 else 1


if __name__ == "__main__":
    raise SystemExit(main())
