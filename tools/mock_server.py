#!/usr/bin/env python3
"""Local AC42 Pilot UI demo server.

Serves the same src/web/index.html as the firmware and a simulated REST API.
It is meant for UI work and screenshots; it never touches a real autopilot.
"""

from __future__ import annotations

import argparse
import json
import math
import random
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INDEX = ROOT / "src" / "web" / "index.html"

START = time.monotonic()
LOCK = threading.RLock()

STATE = {
    "mode": "Auto",
    "engaged": True,
    "heading": 318.0,
    "target": 322.0,
    "variation": 3.0,
    "wind_target": 32.0,
    "wind_side": "stbd",
    "awa": 32.0,
    "aws": 14.2,
    "twa": 41.0,
    "tws": 11.8,
    "sog": 6.2,
    "cog": 315.0,
    "stw": 6.0,
    "depth": 12.4,
    "rudder": 2.0,
    "response_high": False,
    "response_auto": True,
    "levels": {"low": 4, "high": 2, "wind": 3},
    "nfu": False,
    "nfu_dir": "stop",
    "nfu_until": 0.0,
    "tx": 0,
    "rx": 18432,
    "log": [],
}

SETTINGS = {
    "ap_ssid": "AC42-Pilot-DEMO",
    "sta_ssid": "",
    "hostname": "ac42-pilot",
    "controller_override": "auto",
    "ap_override": "auto",
    "rudder_timeout_ms": 1000,
    "ap_password_set": True,
    "sta_password_set": False,
    "ap_ip": "127.0.0.1:8090",
    "sta_ip": "",
    "mac": "DEMO",
    "version": "0.1.0-demo",
}


def log(message: str) -> None:
    with LOCK:
        stamp = time.monotonic() - START
        STATE["log"].append({"ms": int(stamp * 1000), "t": message})
        del STATE["log"][:-48]


def wrap360(value: float) -> float:
    return value % 360.0


def wrap180(value: float) -> float:
    value = wrap360(value)
    return value - 360.0 if value > 180.0 else value


def update() -> None:
    now = time.monotonic()
    if STATE["nfu"] and now > STATE["nfu_until"]:
        STATE["nfu"] = False
        STATE["nfu_dir"] = "stop"
        log("Barre directe: arrêt automatique (démo)")

    t = now - START
    if STATE["engaged"]:
        if STATE["mode"] == "Auto":
            delta = wrap180(STATE["target"] - STATE["heading"])
            STATE["heading"] = wrap360(STATE["heading"] + max(-0.9, min(0.9, delta * 0.18)))
            STATE["rudder"] = max(-18.0, min(18.0, delta * 0.35 + math.sin(t / 3.1) * 1.2))
        elif STATE["mode"] == "Wind":
            STATE["heading"] = wrap360(STATE["heading"] + math.sin(t / 7.5) * 0.35)
            STATE["rudder"] = math.sin(t / 2.4) * 4.0
        else:
            STATE["rudder"] = math.sin(t / 4.0) * 2.0
    else:
        STATE["heading"] = wrap360(STATE["heading"] + math.sin(t / 9.0) * 0.12)
        STATE["rudder"] = 0.0 if not STATE["nfu"] else (-22.0 if STATE["nfu_dir"] == "port" else 22.0)

    STATE["awa"] = wrap180(STATE["wind_target"] + math.sin(t / 5.3) * 2.2)
    STATE["twa"] = wrap180(STATE["awa"] + math.sin(t / 11.0) * 2.0)
    STATE["aws"] = 14.2 + math.sin(t / 8.0) * 1.1
    STATE["tws"] = 11.8 + math.sin(t / 10.0) * 0.8
    STATE["sog"] = 6.2 + math.sin(t / 13.0) * 0.35
    STATE["stw"] = 6.0 + math.sin(t / 14.0) * 0.25
    STATE["cog"] = wrap360(STATE["heading"] - 3.5 + math.sin(t / 9.0) * 1.1)
    STATE["depth"] = 12.4 + math.sin(t / 20.0) * 0.4


def status_payload() -> dict:
    with LOCK:
        update()
        engaged = bool(STATE["engaged"])
        mode = STATE["mode"] if engaged else "Standby"
        heading = STATE["heading"]
        target = STATE["target"]
        wind_target = STATE["wind_target"]
        side = "stbd" if wind_target >= 0 else "port"
        levels = STATE["levels"]
        response_idx = "wind" if mode == "Wind" else ("high" if STATE["response_high"] else "low")
        resp = {
            "valid": engaged,
            "set": "high" if STATE["response_high"] else "low",
            "auto": bool(STATE["response_auto"]),
            "level": levels[response_idx],
            "low": levels["low"],
            "high": levels["high"],
            "wind": levels["wind"],
        }
        warnings = []
        if STATE["nfu"]:
            warnings.append("Barre directe active: relâchez pour arrêter.")
        uptime = int(time.monotonic() - START)
        return {
            "ok": True,
            "name": "AC42 Pilot",
            "version": "0.1.0-demo",
            "uptime_s": uptime,
            "free_heap": 182000,
            "simulated": True,
            "pilot": {
                "mode": mode,
                "mode_code": 2 if mode == "Auto" else (3 if mode == "Wind" else 1),
                "mode_fresh": True,
                "engaged": engaged,
                "engaged_fresh": True,
                "mode_bits": "0x0014",
                "target_deg": round(wrap360(target), 1) if engaged else None,
                "target_ref": "M",
                "target_mag_deg": round(wrap360(target), 1) if engaged else None,
                "target_true_deg": round(wrap360(target + STATE["variation"]), 1) if engaged else None,
                "wind_target_deg": round(wrap180(wind_target), 1) if engaged and mode == "Wind" else None,
                "wind_target_side": side if engaged and mode == "Wind" else None,
                "response": resp,
                "nfu_active": bool(STATE["nfu"]),
                "nfu_elapsed_ms": 1200 if STATE["nfu"] else 0,
            },
            "instruments": {
                "heading_deg": round(heading, 1),
                "heading_ref": "M",
                "heading_mag_deg": round(heading, 1),
                "heading_true_deg": round(wrap360(heading + STATE["variation"]), 1),
                "variation_deg": STATE["variation"],
                "awa_deg": round(STATE["awa"], 1),
                "aws_kn": round(STATE["aws"], 1),
                "twa_deg": round(STATE["twa"], 1),
                "tws_kn": round(STATE["tws"], 1),
                "twd_mag_deg": round(wrap360(heading + STATE["twa"]), 1),
                "twd_true_deg": round(wrap360(heading + STATE["twa"] + STATE["variation"]), 1),
                "sog_kn": round(STATE["sog"], 1),
                "stw_kn": round(STATE["stw"], 1),
                "cog_deg": round(STATE["cog"], 1),
                "cog_ref": "M",
                "cog_mag_deg": round(STATE["cog"], 1),
                "cog_true_deg": round(wrap360(STATE["cog"] + STATE["variation"]), 1),
                "depth_m": round(STATE["depth"], 1),
                "rudder_deg": round(STATE["rudder"], 1),
                "commanded_rudder_deg": round(STATE["rudder"], 1),
            },
            "ap": {
                "known": True,
                "present": True,
                "address": 2,
                "address_hex": "0x02",
                "effective_address": 2,
                "effective_address_hex": "0x02",
                "forced": False,
                "override": SETTINGS["ap_override"],
                "age_ms": 120,
                "model": 0,
            },
            "controller": {
                "present": True,
                "address": 7,
                "address_hex": "0x07",
                "forced": False,
                "override": SETTINGS["controller_override"],
                "age_ms": 90,
            },
            "can": {
                "ready": True,
                "state": "running",
                "bitrate": 250000,
                "rx_frames": STATE["rx"],
                "tx_frames": STATE["tx"],
                "tx_errors": 0,
                "tx_failed": 0,
                "bus_errors": 0,
                "arb_lost": 0,
                "rx_missed": 0,
                "tx_error_counter": 0,
                "rx_error_counter": 0,
                "last_rx_age_ms": 60,
            },
            "wifi": {
                "mode": "AP",
                "ap_started": True,
                "ap_ssid": SETTINGS["ap_ssid"],
                "ap_ip": "127.0.0.1:8090",
                "sta_connected": False,
                "sta_configured": False,
                "fallback_ap": False,
                "sta_ip": "",
                "hostname": SETTINGS["hostname"],
                "rssi": 0,
                "clients": 1,
                "sta_status": 0,
            },
            "config": {
                "controller_override": SETTINGS["controller_override"],
                "ap_override": SETTINGS["ap_override"],
                "rudder_timeout_ms": SETTINGS["rudder_timeout_ms"],
            },
            "warnings": warnings,
        }


def settings_payload() -> dict:
    with LOCK:
        return {"ok": True, **SETTINGS}


def post_command(kind: str, body: dict) -> tuple[int, dict]:
    with LOCK:
        STATE["tx"] += 1
        if kind == "mode":
            mode = str(body.get("mode", ""))
            if mode not in ("standby", "auto", "wind"):
                return 400, {"ok": False, "error": "Mode invalide"}
            if mode == "standby":
                STATE["engaged"] = False
                STATE["mode"] = "Standby"
                log("Mode Standby (démo)")
            elif mode == "auto":
                STATE["engaged"] = True
                STATE["mode"] = "Auto"
                STATE["target"] = STATE["heading"]
                log("Mode Auto (démo)")
            else:
                STATE["engaged"] = True
                STATE["mode"] = "Wind"
                STATE["wind_target"] = STATE["awa"]
                log("Mode Vent (démo)")
            return 200, {"ok": True, "message": "Mode appliqué", "frames": 2}

        if kind == "course":
            if not STATE["engaged"]:
                return 409, {"ok": False, "error": "Pilote en standby"}
            side = str(body.get("side", body.get("dir", "")))
            deg = int(body.get("deg", 0))
            if side not in ("port", "stbd") or deg not in (1, 10):
                return 400, {"ok": False, "error": "Correction invalide"}
            sign = 1 if side == "stbd" else -1
            if STATE["mode"] == "Wind":
                STATE["wind_target"] = wrap180(STATE["wind_target"] + sign * deg)
                log(f"Correction vent {sign * deg:+d}° (démo)")
            else:
                STATE["target"] = wrap360(STATE["target"] + sign * deg)
                log(f"Correction cap {sign * deg:+d}° (démo)")
            return 200, {"ok": True, "message": "Correction envoyée", "frames": 2}

        if kind == "tack":
            if not STATE["engaged"]:
                return 409, {"ok": False, "error": "Pilote en standby"}
            side = str(body.get("dir", ""))
            if side not in ("port", "stbd"):
                return 400, {"ok": False, "error": "Direction invalide"}
            delta = 100 if side == "stbd" else -100
            if STATE["mode"] == "Wind":
                STATE["wind_target"] = -STATE["wind_target"]
            STATE["target"] = wrap360(STATE["target"] + delta)
            log(f"Virement {side} (démo)")
            return 200, {"ok": True, "message": "Virement envoyé", "frames": 2}

        if kind == "rudder":
            if STATE["engaged"]:
                return 409, {"ok": False, "error": "Barre directe en standby uniquement"}
            direction = str(body.get("dir", ""))
            if direction not in ("port", "stbd", "stop"):
                return 400, {"ok": False, "error": "Direction invalide"}
            if direction == "stop":
                STATE["nfu"] = False
                STATE["nfu_dir"] = "stop"
                STATE["rudder"] = 0.0
                log("Barre stop (démo)")
            else:
                STATE["nfu"] = True
                STATE["nfu_dir"] = direction
                STATE["nfu_until"] = time.monotonic() + 1.2
                STATE["rudder"] = 22.0 if direction == "stbd" else -22.0
                log(f"Barre {direction} (démo)")
            return 200, {"ok": True, "message": "Barre", "frames": 2}

        if kind == "response":
            setting = str(body.get("set", ""))
            if setting not in ("auto", "low", "high"):
                return 400, {"ok": False, "error": "Réglage invalide"}
            STATE["response_auto"] = setting == "auto"
            STATE["response_high"] = setting == "high"
            log(f"Réponse {setting} (démo)")
            return 200, {"ok": True, "message": "Réponse appliquée", "frames": 2}

        return 404, {"ok": False, "error": "Commande inconnue"}


class Handler(BaseHTTPRequestHandler):
    server_version = "AC42PilotDemo/0.1"

    def log_message(self, fmt: str, *args) -> None:
        # Keep the terminal readable; the UI has its own log page.
        return

    def _send(self, code: int, content_type: str, body: bytes) -> None:
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(body)

    def _json(self, code: int, payload: dict) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self._send(code, "application/json; charset=utf-8", body)

    def _read_json(self) -> dict:
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0:
            return {}
        raw = self.rfile.read(length)
        try:
            value = json.loads(raw.decode("utf-8"))
            return value if isinstance(value, dict) else {}
        except Exception:
            return {}

    def do_GET(self) -> None:
        path = self.path.split("?", 1)[0]
        if path in ("/", "/index.html"):
            if not INDEX.exists():
                self._send(500, "text/plain", b"index.html missing")
                return
            self._send(200, "text/html; charset=utf-8", INDEX.read_bytes())
            return
        if path == "/favicon.ico":
            self._send(204, "text/plain", b"")
            return
        if path == "/api/status":
            self._json(200, status_payload())
            return
        if path == "/api/settings":
            self._json(200, settings_payload())
            return
        if path == "/api/log":
            with LOCK:
                self._json(200, list(STATE["log"]))
            return
        self._json(404, {"ok": False, "error": "Route inconnue"})

    def do_POST(self) -> None:
        path = self.path.split("?", 1)[0]
        if path == "/api/settings":
            body = self._read_json()
            with LOCK:
                old_network = (SETTINGS["ap_ssid"], SETTINGS["sta_ssid"],
                               SETTINGS["hostname"])
                for key in ("ap_ssid", "sta_ssid", "hostname",
                            "controller_override", "ap_override"):
                    if key in body and isinstance(body[key], str):
                        SETTINGS[key] = body[key]
                if isinstance(body.get("rudder_timeout_ms"), (int, float)):
                    SETTINGS["rudder_timeout_ms"] = int(body["rudder_timeout_ms"])
                if body.get("ap_password"):
                    SETTINGS["ap_password_set"] = True
                if body.get("sta_password"):
                    SETTINGS["sta_password_set"] = True
                reboot_required = (
                    old_network != (SETTINGS["ap_ssid"], SETTINGS["sta_ssid"],
                                    SETTINGS["hostname"])
                    or bool(body.get("ap_password"))
                    or bool(body.get("sta_password"))
                )
            log("Réglages enregistrés (démo)")
            self._json(200, {
                "ok": True,
                "reboot_required": reboot_required,
                "message": "Réglages enregistrés (démo)."
                + (" Redémarrage requis." if reboot_required else ""),
            })
            return
        if path == "/api/reboot":
            log("Redémarrage simulé (démo)")
            self._json(200, {"ok": True, "message": "Redémarrage simulé"})
            return
        if path == "/api/mode":
            code, payload = post_command("mode", self._read_json())
            self._json(code, payload)
            return
        if path == "/api/course":
            code, payload = post_command("course", self._read_json())
            self._json(code, payload)
            return
        if path == "/api/tack":
            code, payload = post_command("tack", self._read_json())
            self._json(code, payload)
            return
        if path == "/api/rudder":
            code, payload = post_command("rudder", self._read_json())
            self._json(code, payload)
            return
        if path == "/api/response":
            code, payload = post_command("response", self._read_json())
            self._json(code, payload)
            return
        self._json(404, {"ok": False, "error": "Route inconnue"})


def main() -> None:
    parser = argparse.ArgumentParser(description="AC42 Pilot UI demo server")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()

    log("Serveur de démonstration démarré")
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"AC42 Pilot demo: http://{args.host}:{args.port}/")
    print("Mode démonstration: aucune commande réelle n'est envoyée.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nArrêt.")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
