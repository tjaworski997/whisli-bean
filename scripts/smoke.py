#!/usr/bin/env python3
"""Checks a running device through its API, on temporary data it removes afterwards.

Usage: WHISLI_BEAN_PASSWORD=<password> scripts/smoke.py <ip>
Do not run it while someone is using the panel: it adds and removes a button, a webhook and a server.
MQTT messages are not looked at, only whether the device reports them as sent.
"""
import base64
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

host = sys.argv[1]
token = base64.b64encode(f"whisli:{os.environ.get('WHISLI_BEAN_PASSWORD', 'whisli_gogo')}".encode()).decode()
CODE = "SMOKE-TEST"
failures = 0


def call(path, fields=None):
    data = urllib.parse.urlencode(fields).encode() if fields is not None else None
    request = urllib.request.Request(f"http://{host}/api/v1{path}", data=data, headers={"Authorization": f"Basic {token}"})
    try:
        with urllib.request.urlopen(request, timeout=20) as response:
            return response.status, json.load(response)
    except urllib.error.HTTPError as error:
        return error.code, json.load(error)


def check(what, ok, detail=""):
    global failures
    print(f"{'ok  ' if ok else 'FAIL'} {what}" + (f": {detail}" if detail and not ok else ""))
    failures += not ok


def steps_after_test(gesture="short"):
    before = (call("/state")[1]["actions"]["run"] or {}).get("count", 0)
    call("/buttons/test", {"code": CODE, "gesture": gesture})
    for _ in range(40):
        time.sleep(0.25)
        run = call("/state")[1]["actions"]["run"]
        if run and run["count"] > before and all(step["status"] in ("ok", "failed") for step in run["steps"]):
            return {step["type"]: step["status"] for step in run["steps"]}
    return None


status, system = call("/system")
check("device answers", status == 200, status)
print(f"     version {system['version']}, free memory {system['heap'] // 1024} KB")
request = urllib.request.Request(f"http://{host}/api/v1/buttons")
try:
    urllib.request.urlopen(request, timeout=10)
    check("the API needs signing in", False)
except urllib.error.HTTPError as error:
    check("the API needs signing in", error.code == 401, error.code)

hook = server = 0
try:
    hook = call("/hooks", {"name": "Smoke test"})[1]["id"]
    server = call("/servers", {"name": "Smoke test"})[1]["id"]
    check("a server with a bad MAC is refused", call("/servers/update", {"id": server, "name": "x", "mac": "zz"})[0] == 400)
    check("a webhook with a bad URL is refused", call("/hooks/update", {"id": hook, "name": "x", "url": "ftp://x", "method": "POST", "body": "none"})[0] == 400)
    check("a button code with / is refused", call("/buttons", {"code": "A/B", "name": "x"})[0] == 400)
    check("an old list of actions is refused", call("/buttons", {"code": CODE, "name": "x", "actions": '{"short":[]}'})[0] == 400)
    # Port 9 of the device itself: nothing listens, which is enough for a request that fails on its own.
    call("/hooks/update", {"id": hook, "name": "Smoke test", "url": f"http://{host}:9/", "method": "POST", "body": "none"})
    call("/servers/update", {"id": server, "name": "Smoke test", "mac": "AA:BB:CC:DD:EE:FF", "broadcast": "127.0.0.1"})

    actions = {"short": {"enabled": True, "webhook": hook, "wol": server}, "long": {"enabled": False, "wol": server}}
    check("a button saves", call("/buttons", {"code": CODE, "name": "Smoke test", "gestures": 0, "actions": json.dumps(actions)})[0] == 200)
    check("a note saves", call("/buttons", {"code": CODE, "name": "Smoke test", "note": "Notatka: żółć"})[1]["buttons"][-1].get("note") == "Notatka: żółć")
    check("a save without a note keeps it", call("/buttons", {"code": CODE, "name": "Smoke test"})[1]["buttons"][-1].get("note") == "Notatka: żółć")
    check("a note that is too long is refused", call("/buttons", {"code": CODE, "name": "Smoke test", "note": "x" * 401})[0] == 400)
    check("a note can be cleared", "note" not in call("/buttons", {"code": CODE, "name": "Smoke test", "note": ""})[1]["buttons"][-1])
    steps = steps_after_test()
    check("a failed webhook does not stop Wake-on-LAN", steps == {"webhook": "failed", "wol": "ok"}, steps)
    time.sleep(0.5)
    before = (call("/state")[1]["actions"]["run"] or {}).get("count", 0)
    call("/buttons/test", {"code": CODE, "gesture": "long"})
    time.sleep(1)
    check("a switched-off gesture does nothing", (call("/state")[1]["actions"]["run"] or {}).get("count", 0) == before)
    saved = next(b for b in call("/buttons")[1]["buttons"] if b["code"] == CODE)
    check("a switched-off gesture keeps its settings", saved["actions"]["long"].get("wol") == server, saved["actions"])
finally:
    call("/buttons/delete", {"code": CODE})
    if hook:
        call("/hooks/remove", {"id": hook})
    if server:
        call("/servers/remove", {"id": server})

left = [b["code"] for b in call("/buttons")[1]["buttons"] if b["code"] == CODE]
check("the temporary data is gone", not left, left)
print("\nall passed" if not failures else f"\n{failures} FAILED")
sys.exit(1 if failures else 0)
