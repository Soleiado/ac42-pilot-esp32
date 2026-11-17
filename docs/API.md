# API HTTP

L'interface web et l'API sont servies par le même serveur, sur le port 80 :

- `http://192.168.4.1/` lorsque l'AP est actif ;
- `http://ac42-pilot.local/` via mDNS ;
- `http://<ip-sta>/` lorsqu'un réseau local (STA) est configuré et accessible.

Toutes les réponses sont en JSON sauf la page HTML. Les requêtes `POST`
doivent utiliser `Content-Type: application/json`.

> Il n'y a pas d'authentification HTTP dans cette version. Protégez le réseau
> Wi-Fi et n'exposez pas la carte sur Internet.

## Routes

| Méthode | Route | Description |
|---|---|---|
| GET | `/` | interface web |
| GET | `/api/status` | état complet (polling) |
| GET | `/api/settings` | réglages (sans mots de passe) |
| POST | `/api/settings` | modifier les réglages |
| POST | `/api/mode` | standby, auto, vent |
| POST | `/api/course` | correction de cap/vent |
| POST | `/api/tack` | virement |
| POST | `/api/rudder` | barre directe NFU |
| POST | `/api/response` | jeu de paramètres LO/HI/AUTO |
| POST | `/api/reboot` | redémarrer |
| GET | `/api/log` | journal circulaire |

## GET /api/status

Exemple simplifié :

```json
{
  "ok": true,
  "name": "AC42 Pilot",
  "version": "0.1.0",
  "uptime_s": 1830,
  "free_heap": 182000,
  "simulated": false,
  "pilot": {
    "mode": "Wind",
    "mode_fresh": true,
    "engaged": true,
    "engaged_fresh": true,
    "target_deg": 322.0,
    "target_ref": "M",
    "wind_target_deg": 34.0,
    "wind_target_side": "stbd",
    "response": {
      "valid": true,
      "set": "low",
      "auto": true,
      "level": 4,
      "low": 4,
      "high": 2,
      "wind": 3
    },
    "nfu_active": false
  },
  "instruments": {
    "heading_deg": 321.4,
    "heading_ref": "M",
    "awa_deg": 33.8,
    "aws_kn": 14.1,
    "twa_deg": 40.2,
    "tws_kn": 11.7,
    "sog_kn": 6.2,
    "cog_deg": 317.2,
    "cog_ref": "M",
    "rudder_deg": -2.1,
    "depth_m": 12.4
  },
  "ap": {
    "known": true,
    "present": true,
    "address": 2,
    "address_hex": "0x02",
    "forced": false,
    "override": "auto",
    "age_ms": 120
  },
  "controller": {
    "present": true,
    "address": 7,
    "address_hex": "0x07",
    "forced": false,
    "override": "auto"
  },
  "can": {
    "ready": true,
    "state": "running",
    "bitrate": 250000,
    "rx_frames": 18432,
    "tx_frames": 24,
    "tx_errors": 0,
    "last_rx_age_ms": 60
  },
  "wifi": {
    "mode": "AP",
    "ap_started": true,
    "ap_ssid": "AC42-Pilot-1A2B",
    "ap_ip": "192.168.4.1",
    "sta_connected": false,
    "sta_configured": false,
    "fallback_ap": false,
    "sta_ip": "",
    "hostname": "ac42-pilot",
    "rssi": 0,
    "clients": 1
  },
  "config": {
    "controller_override": "auto",
    "ap_override": "auto",
    "rudder_timeout_ms": 1000,
    "reboot_pending": false
  },
  "warnings": []
}
```

Les valeurs non reçues ou trop anciennes sont `null`. Le firmware considère
les données pilote/instruments comme fraîches pendant 5 s. Les paramètres de
réponse sont rafraîchis environ toutes les 60 s, avec une nouvelle tentative
toutes les 15 s.

## POST /api/mode

```json
{ "mode": "auto" }
```

Modes acceptés : `standby`, `auto`, `wind`.

Réponse :

```json
{ "ok": true, "message": "Mode Auto: 2 trame(s), SA=0x07 cible=0x02", "frames": 2 }
```

## POST /api/course

```json
{ "side": "stbd", "deg": 1 }
```

- `side` : `stbd`, `starboard`, `bear`, `port`, `luff` ;
- `deg` : `1` ou `10`.

La commande n'est acceptée qu'en Auto, Vent ou No Drift, pilote engagé. Le
sens est inversé automatiquement en mode Vent (voir `docs/PROTOCOL.md`).

## POST /api/tack

```json
{ "dir": "stbd" }
```

`port` ou `stbd`. Refusé en standby et en Nav.

## POST /api/rudder

```json
{ "dir": "stbd" }
```

`port`, `stbd` ou `stop`. Uniquement en standby. Le client doit envoyer un
keepalive (`port`/`stbd`) au moins toutes les secondes ; le firmware envoie
`stop` automatiquement sinon, et au plus tard après 8 s.

## POST /api/response

```json
{ "set": "auto" }
```

`auto`, `low` ou `high`. La valeur est envoyée à l'AC42 puis relue via
PGN 130845.

## GET/POST /api/settings

`GET` renvoie les réglages sans les mots de passe :

```json
{
  "ok": true,
  "ap_ssid": "AC42-Pilot-1A2B",
  "ap_password_set": true,
  "sta_ssid": "",
  "sta_password_set": false,
  "hostname": "ac42-pilot",
  "controller_override": "auto",
  "ap_override": "auto",
  "rudder_timeout_ms": 1000,
  "ap_ip": "192.168.4.1",
  "sta_ip": "",
  "wifi_mode": "AP",
  "sta_connected": false,
  "ap_started": true,
  "fallback_ap": false,
  "reboot_pending": false,
  "mac": "AA:BB:CC:DD:EE:FF",
  "version": "0.1.0"
}
```

Le mode Wi-Fi est `AP` quand aucun réseau STA n'est configuré, `STA` quand le
réseau local est accessible, `AP de secours` après environ 15 s sans connexion,
et `AP+STA` si le STA se reconnecte alors que l'AP de secours est déjà actif.

`POST` accepte les mêmes champs. **Aucun réseau STA n'est configuré par
défaut** : la carte démarre en AP. Les mots de passe absents ou vides restent
inchangés (sauf `sta_ssid` vide, qui désactive le STA et efface son mot de
passe). Pour changer un mot de passe, envoyer une valeur de 8 à 63 caractères.

```json
{
  "ap_ssid": "AC42-Pilot",
  "ap_password": "un-mot-de-passe",
  "sta_ssid": "MonBateau",
  "sta_password": "autre-mot-de-passe",
  "hostname": "ac42-pilot",
  "controller_override": "0x07",
  "ap_override": "0x02",
  "rudder_timeout_ms": 1000
}
```

Un changement de SSID/mot de passe/hostname renvoie
`"reboot_required": true` (et `"reboot_pending": true`). Le firmware **ne
redémarre plus automatiquement** après l'enregistrement : l'interface affiche
le résultat et l'utilisateur clique sur **Redémarrer**. Cela évite de perdre
la réponse HTTP ou la connexion Wi-Fi avant d'avoir vu le résultat.

Les adresses s'écrivent `auto`, `0xNN` ou `NN` en hexadécimal.

## Codes d'erreur

| Code | Cas |
|---:|---|
| 200 | commande acceptée |
| 400 | JSON ou paramètre invalide |
| 409 | aucun contrôleur, mauvais mode, standby/engagé incompatible |
| 415 | `Content-Type` non JSON |
| 429 | commandes trop rapprochées |
| 502 | bus CAN non prêt ou émission échouée |

Exemple :

```json
{ "ok": false, "error": "Aucun pupitre/contrôleur B&G actif détecté." }
```

## GET /api/log

Retourne un tableau de messages :

```json
[
  { "ms": 1250, "t": "CAN prêt: 250 kbit/s, TX=GPIO15 RX=GPIO16" },
  { "ms": 3400, "t": "Pupitre/contrôleur actif détecté à l'adresse 0x07" }
]
```

Le journal est circulaire (48 entrées) et n'est pas persistant.
