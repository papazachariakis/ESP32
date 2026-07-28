#!/usr/bin/env python3
"""Install PVX cards permanently: update Lovelace resource to /local/ + optional GitHub URL probe."""
from __future__ import annotations

import asyncio
import json
import urllib.parse
import urllib.request
from pathlib import Path

import websockets

HA = "http://homeassistant.local:8123"
WS = "ws://homeassistant.local:8123/api/websocket"
USER = "ioannis"
PASS = "papajohn123"
CARD_JS = "fotovoltaiko-extreme-cards.js"
LOCAL_URL = f"/local/{CARD_JS}"
GH_URL = f"https://papazachariakis.github.io/ESP32/{CARD_JS}"
SRC = Path(r"c:\Users\papaz\Documents\Claude\Projects\ESP32\docs") / CARD_JS


def http_token() -> str:
    def post(url, data, headers=None):
        body = data if isinstance(data, (bytes, bytearray)) else json.dumps(data).encode()
        req = urllib.request.Request(url, data=body, headers=headers or {"Content-Type": "application/json"})
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
        return post(
            f"{HA}/auth/token",
            body,
            {"Content-Type": "application/x-www-form-urlencoded"},
        )["access_token"]
    raise RuntimeError("login failed")


async def set_resource(url: str) -> None:
    tok = http_token()
    async with websockets.connect(WS, max_size=8 * 1024 * 1024) as ws:
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

        # try supervisor API to write file via addon
        for endpoint in ("/addons", "/info", "/host/info"):
            r = await call("supervisor/api", endpoint=endpoint, method="get")
            print("supervisor", endpoint, r.get("success"), (r.get("error") or "")[:120], flush=True)

        res = await call("lovelace/resources")
        resources = res.get("result") or []
        matched = [r for r in resources if CARD_JS in (r.get("url") or "")]
        if matched:
            for r in matched:
                ur = await call(
                    "lovelace/resources/update",
                    resource_id=r["id"],
                    res_type="module",
                    url=url,
                )
                print("update", r.get("id"), ur.get("success"), ur.get("error"), flush=True)
        else:
            cr = await call("lovelace/resources/create", res_type="module", url=url)
            print("create", cr.get("success"), cr.get("result") or cr.get("error"), flush=True)

        # verify local fetch
        try:
            req = urllib.request.Request(f"{HA}{LOCAL_URL}")
            with urllib.request.urlopen(req, timeout=10) as resp:
                print("LOCAL_FETCH", resp.status, "bytes", len(resp.read()), flush=True)
        except Exception as e:
            print("LOCAL_FETCH_FAIL", e, flush=True)

        try:
            with urllib.request.urlopen(GH_URL, timeout=15) as resp:
                print("GH_FETCH", resp.status, "bytes", len(resp.read()), flush=True)
        except Exception as e:
            print("GH_FETCH_FAIL", e, flush=True)


def main():
    print("src bytes", SRC.stat().st_size if SRC.exists() else None, flush=True)
    asyncio.run(set_resource(LOCAL_URL))


if __name__ == "__main__":
    main()
