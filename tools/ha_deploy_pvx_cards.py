#!/usr/bin/env python3
"""Serve docs/ with CORS + register PVX cards + update ΦΩΤΟΒΟΛΤΑΙΚΟ NEW dashboard."""
from __future__ import annotations

import asyncio
import json
import socket
import threading
import urllib.parse
import urllib.request
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import websockets

HA = "http://homeassistant.local:8123"
WS = "ws://homeassistant.local:8123/api/websocket"
USER = "ioannis"
PASS = "papajohn123"
ROOT = Path(r"c:\Users\papaz\Documents\Claude\Projects\ESP32")
DOCS = ROOT / "docs"
OUT = ROOT / "tools" / "ha_dash_fotovoltaiko_new.json"
URL_PATH = "fotovoltaiko-new"
TITLE = "ΦΩΤΟΒΟΛΤΑΙΚΟ NEW"
ICON = "mdi:solar-power-variant"
PORT = 8766
CARD_JS = "fotovoltaiko-extreme-cards.js"
# Permanent HA resource (GitHub Pages). Prefer this over LAN :8766.
GH_RESOURCE_URL = f"https://papazachariakis.github.io/ESP32/{CARD_JS}"
LOCAL_RESOURCE_URL = f"/local/{CARD_JS}"


def lan_ip() -> str:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("192.168.99.100", 80))
        return s.getsockname()[0]
    except OSError:
        return "192.168.99.15"
    finally:
        s.close()


class CORSHandler(SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=str(DOCS), **k)

    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(204)
        self.end_headers()

    def log_message(self, fmt, *args):
        print("[http]", fmt % args, flush=True)


def start_server() -> ThreadingHTTPServer:
    srv = ThreadingHTTPServer(("0.0.0.0", PORT), CORSHandler)
    t = threading.Thread(target=srv.serve_forever, daemon=True)
    t.start()
    return srv


def http_token() -> str:
    def post(url, data):
        req = urllib.request.Request(
            url, data=json.dumps(data).encode(), headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=30) as r:
            return json.loads(r.read().decode() or "{}")

    for _ in range(3):
        flow = post(
            f"{HA}/auth/login_flow",
            {"client_id": f"{HA}/", "handler": ["homeassistant", None], "redirect_uri": f"{HA}/"},
        )
        res = post(
            f"{HA}/auth/login_flow/{flow['flow_id']}",
            {"client_id": f"{HA}/", "username": USER, "password": PASS},
        )
        if "result" not in res:
            continue
        body = urllib.parse.urlencode(
            {"grant_type": "authorization_code", "code": res["result"], "client_id": f"{HA}/"}
        ).encode()
        req = urllib.request.Request(
            f"{HA}/auth/token",
            data=body,
            headers={"Content-Type": "application/x-www-form-urlencoded"},
        )
        with urllib.request.urlopen(req, timeout=20) as r:
            return json.loads(r.read().decode())["access_token"]
    raise RuntimeError("login failed")


def build_cfg() -> dict:
    return {
        "views": [
            {
                "title": "PVX",
                "path": "pvx",
                "icon": "mdi:lightning-bolt-circle",
                "type": "masonry",
                "cards": [
                    {
                        "type": "custom:pvx-hero-card",
                        "title": "ΦΩΤΟΒΟΛΤΑΪΚΟ",
                        "tag": "PVX · OWN CARDS",
                        "meta": "Deye · Basen 30kWh · Grid · Breaker · custom Lovelace",
                    },
                    {
                        "type": "custom:pvx-kpi-card",
                        "title": "LIVE POWER",
                        "subtitle": "Real-time inverter rails",
                        "pv_entity": "sensor.inverter_pv_power",
                        "load_entity": "sensor.inverter_load_power",
                        "grid_entity": "sensor.inverter_grid_power",
                        "battery_entity": "sensor.inverter_battery_power",
                    },
                    {
                        "type": "custom:pvx-flow-card",
                        "title": "POWER FLOW",
                        "pv_entity": "sensor.inverter_pv_power",
                        "load_entity": "sensor.inverter_load_power",
                        "grid_entity": "sensor.inverter_grid_power",
                        "battery_entity": "sensor.inverter_battery_power",
                        "soc_entity": "sensor.inverter_battery",
                    },
                    {
                        "type": "custom:pvx-battery-card",
                        "title": "BASEN 30kWh",
                    },
                    {
                        "type": "custom:pvx-cells-card",
                        "title": "CELL MATRIX",
                        "count": 16,
                        "base_entity": "sensor.tp_bstbd_25c_2_cell_voltage",
                    },
                    {
                        "type": "custom:pvx-today-card",
                        "title": "TODAY ENERGY",
                    },
                    {
                        "type": "custom:pvx-breaker-card",
                        "title": "BREAKER",
                        "switch_entity": "switch.breaker_switch",
                    },
                    {
                        "type": "entities",
                        "title": "Quick controls",
                        "show_header_toggle": False,
                        "entities": [
                            "switch.inverter",
                            "switch.inverter_off_grid",
                            "switch.breaker_switch",
                            "switch.piestiko_tuya_local",
                        ],
                    },
                ],
            }
        ]
    }


async def deploy(resource_url: str) -> int:
    cfg = build_cfg()
    OUT.write_text(json.dumps(cfg, ensure_ascii=False, indent=2), encoding="utf-8")
    print("wrote", OUT, flush=True)

    tok = http_token()
    print("login ok", flush=True)
    async with websockets.connect(WS, max_size=32 * 1024 * 1024) as ws:
        await ws.recv()
        await ws.send(json.dumps({"type": "auth", "access_token": tok}))
        assert json.loads(await ws.recv()).get("type") == "auth_ok"
        nid = 1

        async def call(t, **e):
            nonlocal nid
            msg = {"id": nid, "type": t, **e}
            nid += 1
            await ws.send(json.dumps(msg))
            while True:
                r = json.loads(await ws.recv())
                if r.get("id") == msg["id"]:
                    return r

        # resources
        res = await call("lovelace/resources")
        resources = res.get("result") or []
        existing = [r for r in resources if CARD_JS in (r.get("url") or "")]
        if existing:
            for r in existing:
                print("update resource", r.get("id"), flush=True)
                await call(
                    "lovelace/resources/update",
                    resource_id=r["id"],
                    res_type="module",
                    url=resource_url,
                )
        else:
            cr = await call("lovelace/resources/create", res_type="module", url=resource_url)
            print("create resource", cr.get("success"), cr.get("result") or cr.get("error"), flush=True)
            if not cr.get("success"):
                return 1

        # dashboard upsert
        dashboards = (await call("lovelace/dashboards/list")).get("result") or []
        found = next((d for d in dashboards if d.get("url_path") == URL_PATH), None)
        if not found:
            cr = await call(
                "lovelace/dashboards/create",
                title=TITLE,
                url_path=URL_PATH,
                icon=ICON,
                require_admin=False,
                show_in_sidebar=True,
            )
            print("create dash", cr.get("success"), cr.get("result") or cr.get("error"), flush=True)
            if not cr.get("success"):
                return 1
        else:
            print("dash exists", found.get("id"), flush=True)

        sv = await call("lovelace/config/save", url_path=URL_PATH, config=cfg)
        print("save", sv.get("success"), sv.get("error"), flush=True)
        if not sv.get("success"):
            return 1

        chk = await call("lovelace/config", url_path=URL_PATH)
        views = (chk.get("result") or {}).get("views") or []
        cards = views[0].get("cards", []) if views else []
        types = [c.get("type") for c in cards]
        print("VERIFY cards=", len(cards), "types=", types, "url=/" + URL_PATH, flush=True)
        print("RESOURCE", resource_url, flush=True)
        print("SUCCESS", flush=True)
        return 0


def main() -> int:
    import argparse
    import time

    ap = argparse.ArgumentParser()
    ap.add_argument("--serve-only", action="store_true")
    ap.add_argument("--deploy-only", action="store_true")
    ap.add_argument("--keep-alive", type=int, default=0, help="seconds to keep HTTP server (0=exit after deploy)")
    args = ap.parse_args()

    if not (DOCS / CARD_JS).exists():
        raise SystemExit(f"missing {DOCS / CARD_JS}")
    ip = lan_ip()
    resource_url = f"http://{ip}:{PORT}/{CARD_JS}"

    srv = None
    if not args.deploy_only:
        srv = start_server()
        print(f"serving {DOCS} on :{PORT} -> {resource_url}", flush=True)
        with urllib.request.urlopen(resource_url, timeout=5) as r:
            print("card js bytes", len(r.read()), flush=True)
        if args.serve_only:
            print("serve-only — running forever", flush=True)
            while True:
                time.sleep(3600)

    code = 0 if args.serve_only else asyncio.run(deploy(resource_url))
    if srv and args.keep_alive > 0:
        print(f"keeping server {args.keep_alive}s", flush=True)
        time.sleep(args.keep_alive)
        srv.shutdown()
    elif srv and not args.serve_only:
        # leave daemon thread until process exits; sleep so HA can fetch once
        time.sleep(1)
    return code


if __name__ == "__main__":
    raise SystemExit(main())
