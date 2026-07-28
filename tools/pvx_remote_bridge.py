#!/usr/bin/env python3
"""
PVX remote bridge: LAN -> HA, public -> Cloudflare quick tunnel.
Serves the fotovoltaiko-new cockpit without exposing full HA.
"""
from __future__ import annotations

import asyncio
import json
import threading
import time
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import websockets

HA = "http://192.168.99.100:8123"
WS = "ws://192.168.99.100:8123/api/websocket"
USER = "ioannis"
PASS = "papajohn123"
PORT = 8767
ROOT = Path(r"c:\Users\papaz\Documents\Claude\Projects\ESP32")
PAGE = ROOT / "docs" / "fotovoltaiko-new.html"
TOKEN_FILE = ROOT / "tools" / "_pvx_llat.txt"

PREFIXES = (
    "sensor.inverter_",
    "sensor.tp_bstbd_25c_2_",
    "binary_sensor.tp_bstbd_25c_2_",
    "sensor.breaker_",
    "switch.breaker_",
    "switch.inverter",
)

STATES: dict[str, dict] = {}
LOCK = threading.Lock()
HA_OK = False


def login_token() -> str:
    if TOKEN_FILE.exists():
        t = TOKEN_FILE.read_text(encoding="utf-8").strip()
        if t:
            return t

    def post(url, data, headers=None):
        body = data if isinstance(data, (bytes, bytearray)) else json.dumps(data).encode()
        req = urllib.request.Request(url, data=body, headers=headers or {"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=20) as r:
            return json.loads(r.read().decode() or "{}")

    flow = post(
        f"{HA}/auth/login_flow",
        {"client_id": f"{HA}/", "handler": ["homeassistant", None], "redirect_uri": f"{HA}/"},
    )
    res = post(
        f"{HA}/auth/login_flow/{flow['flow_id']}",
        {"client_id": f"{HA}/", "username": USER, "password": PASS},
    )
    body = urllib.parse.urlencode(
        {"grant_type": "authorization_code", "code": res["result"], "client_id": f"{HA}/"}
    ).encode()
    return post(
        f"{HA}/auth/token",
        body,
        {"Content-Type": "application/x-www-form-urlencoded"},
    )["access_token"]


def want(eid: str) -> bool:
    return any(eid.startswith(p) or eid == p.rstrip("_") for p in PREFIXES)


async def ha_loop():
    global HA_OK
    while True:
        try:
            tok = login_token()
            async with websockets.connect(WS, max_size=16 * 1024 * 1024) as ws:
                await ws.recv()
                await ws.send(json.dumps({"type": "auth", "access_token": tok}))
                auth = json.loads(await ws.recv())
                if auth.get("type") != "auth_ok":
                    raise RuntimeError(auth)
                HA_OK = True
                print("HA connected", flush=True)
                await ws.send(json.dumps({"id": 1, "type": "get_states"}))
                while True:
                    msg = json.loads(await ws.recv())
                    if msg.get("id") == 1:
                        with LOCK:
                            STATES.clear()
                            for s in msg.get("result") or []:
                                eid = s.get("entity_id") or ""
                                if want(eid):
                                    STATES[eid] = {
                                        "entity_id": eid,
                                        "state": s.get("state"),
                                        "attributes": s.get("attributes") or {},
                                    }
                        print("states", len(STATES), flush=True)
                        break
                await ws.send(json.dumps({"id": 2, "type": "subscribe_events", "event_type": "state_changed"}))
                async for raw in ws:
                    msg = json.loads(raw)
                    if msg.get("type") != "event":
                        continue
                    s = (msg.get("event") or {}).get("data", {}).get("new_state")
                    if not s:
                        continue
                    eid = s.get("entity_id") or ""
                    if want(eid):
                        with LOCK:
                            STATES[eid] = {
                                "entity_id": eid,
                                "state": s.get("state"),
                                "attributes": s.get("attributes") or {},
                            }
        except Exception as e:
            HA_OK = False
            print("HA loop error", e, flush=True)
            await asyncio.sleep(4)


def start_ha_thread():
    def run():
        asyncio.run(ha_loop())

    threading.Thread(target=run, daemon=True).start()


class Handler(BaseHTTPRequestHandler):
    def _cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.send_header("Cache-Control", "no-store")

    def do_OPTIONS(self):
        self.send_response(204)
        self._cors()
        self.end_headers()

    def do_GET(self):
        path = self.path.split("?", 1)[0]
        if path in ("/", "/index.html", "/fotovoltaiko-new.html"):
            html = PAGE.read_text(encoding="utf-8")
            # Force bridge mode defaults into page by injecting script before </body>
            inject = """
<script>
(() => {
  const KEY='pvx_web_cfg_v1';
  const bridge = location.origin;
  // Bridge mode: page talks to same origin /api/states (no HA token needed in browser)
  window.PVX_BRIDGE = bridge;
  const old = JSON.parse(localStorage.getItem(KEY) || '{}');
  // Keep token unused; mark bridge
  localStorage.setItem(KEY, JSON.stringify({...old, bridge: bridge, mode:'bridge'}));
})();
</script>
"""
            if "window.PVX_BRIDGE" not in html:
                html = html.replace("</body>", inject + "</body>")
            data = html.encode("utf-8")
            self.send_response(200)
            self._cors()
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return

        if path == "/api/health":
            body = json.dumps({"ok": True, "ha": HA_OK, "entities": len(STATES)}).encode()
            self.send_response(200)
            self._cors()
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if path == "/api/states":
            with LOCK:
                body = json.dumps(list(STATES.values())).encode()
            self.send_response(200)
            self._cors()
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        self.send_response(404)
        self._cors()
        self.end_headers()

    def log_message(self, fmt, *args):
        print("[http]", fmt % args, flush=True)


def main():
    if not PAGE.exists():
        raise SystemExit(f"missing {PAGE}")
    start_ha_thread()
    # wait briefly for first states
    for _ in range(20):
        if STATES:
            break
        time.sleep(0.5)
    httpd = ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    print(f"PVX bridge http://127.0.0.1:{PORT}  entities={len(STATES)} ha={HA_OK}", flush=True)
    httpd.serve_forever()


if __name__ == "__main__":
    main()
