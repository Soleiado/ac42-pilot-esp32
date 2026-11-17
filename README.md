# AC42 Pilot — interface web pour pilote Simrad AC42

Firmware pour le module
[**Waveshare ESP32-S3-RS485-CAN**](https://www.waveshare.com/product/esp32-related/boards-kits/esp32-s3/esp32-s3-rs485-can.htm)
qui expose une interface web mobile pour contrôler un pilote automatique
**Simrad AC42** sur un bus **NMEA 2000**.

Le projet s'inspire du plugin Signal K
[`signalk-ac42-autopilot`](https://github.com/CaptnOliv/signalk-ac42-autopilot)
pour l'interface et pour le protocole Simnet, mais il fonctionne de manière
autonome : pas de Signal K, pas de serveur, pas d'ordinateur de bord. La carte
se connecte directement au bus NMEA 2000 et démarre en point d'accès Wi-Fi.
Un réseau local (STA) peut être configuré depuis la page de réglages ; si ce
réseau devient inaccessible, l'AP de secours est réactivé automatiquement.

![Interface web AC42 Pilot — vue desktop](docs/screenshot-desktop.png)

Interface web en **vue desktop** (1280 px). Interface mobile (390 px) :
[docs/screenshot-mobile.png](docs/screenshot-mobile.png).

Page de réglages réseau : [docs/screenshot-settings.png](docs/screenshot-settings.png).

## Inspiration

Le firmware, l’interface web et l’application Android sont **inspirés du plugin
SignalK [`signalk-ac42-autopilot`](https://github.com/CaptnOliv/signalk-ac42-autopilot)**,
qui contrôle un AC42 depuis Signal K. Ce projet reprend :

- la présentation et l’ergonomie de l’interface web (compas, modes, corrections,
  virement long, barre directe NFU, réglages) ;
- les trames Simnet PGN 130850 et 130845, les événements de commande et les
  directions validées sur un AC42 réel ;
- la détection du contrôleur B&G actif et l’émission des commandes avec son
  adresse source NMEA 2000.

La différence principale est l’architecture : ici, tout est embarqué sur la
carte ESP32-S3, sans Signal K, sans `canboat` et sans ordinateur de bord. Le
plugin `signalk-ac42-autopilot` reste la référence du protocole et la source
d’inspiration de ce projet. Ce projet n’est ni affilié ni approuvé par Signal K
ou par l’auteur du plugin.

## Réalisation — vibe coding

Ce projet a été réalisé en **« vibe coding »** : le firmware ESP32, l’interface
web embarquée, l’application Android, la documentation et les tests ont été
écrits en dialogue avec une IA, à partir d’objectifs et de retours humains.

- **Outil** : [OpenCode](https://opencode.ai), agent de développement en ligne
  de commande.
- **Modèle** : **DeepSeek** (DeepSeek V4.1 Flash).

Le protocole AC42/Simnet n’a pas été inventé : il provient du plugin
`signalk-ac42-autopilot` et des essais sur le matériel. Le « vibe coding » a
accéléré l’écriture du code, pas remplacé la vérification — la mise en service
reste à faire à quai, avec les précautions ci-dessous.

## Avertissement — sécurité

**Ce projet envoie de vraies commandes à un pilote automatique.** Il n'est ni
certifié, ni validé par Simrad/Navico. Une erreur de câblage, de protocole ou
d'utilisation peut provoquer un mouvement de barre inattendu.

- Essayez d'abord **à quai**, barre dégagée, avec quelqu'un prêt à passer en
  standby.
- Gardez toujours une veille humaine et un moyen d'arrêter le pilote.
- Ne vous reposez jamais sur cette interface pour la sécurité du navire.
- Vérifiez que l'injection de trames sur votre bus NMEA 2000 est autorisée et
  adaptée à votre installation.

L'AC42 n'accepte les commandes PGN 130850 que si elles portent **l'adresse
source d'un contrôleur B&G/Simrad actif** (pupitre, MFD, Triton…) qui émet son
heartbeat PGN 65305. Le firmware détecte ce contrôleur et émet les commandes
avec son adresse source, exactement comme le plugin Signal K. Sans contrôleur
actif sur le bus, les commandes sont refusées par l'AC42.

## État du projet

- Firmware compilé avec PlatformIO 8.0.0 / Arduino ESP32 3.3.12.
- Tests de trames NMEA 2000 : 60 vérifications, toutes passées.
- Interface web testée sur desktop et mobile (390 px) avec le serveur de
  démonstration.
- Application Android compilée (APK debug) et tests de parsing JSON : 3/3.
- **Pas encore validé sur un AC42 réel avec cette carte** : le protocole est
  issu du plugin `signalk-ac42-autopilot`, validé sur un AC42, mais la mise en
  service doit être faite à quai.

## Fonctionnalités

- Interface web embarquée, responsive, en français, sans dépendance externe.
- Modes **Standby**, **Auto** (maintien de cap), **Vent** (angle apparent).
- Corrections de cap **±1°** et **±10°**, avec sens adapté au mode.
- **Virement** bâbord/tribord par appui long de 2 s.
- **Barre directe NFU** en standby : appui maintenu, keepalive 300 ms,
  arrêt automatique par watchdog (délai configurable, plafond 8 s).
- Jeu de paramètres de barre **LO / HI / AUTO** et niveau de réponse 1–10 lu
  sur le PGN 130845.
- Détection automatique de l'adresse du contrôleur B&G et de l'AC42 ; adresses
  forçables depuis l'interface.
- Compas, cible, vent apparent/vrai, SOG, COG, barre, profondeur selon les PGN
  disponibles sur le bus.
- Diagnostic : état CAN, adresses N2K, compteurs, journal, avertissements.
- Wi-Fi **AP par défaut**, réseau STA configurable via la page de réglages,
  **AP de secours automatique**, mDNS `ac42-pilot.local`, réglages persistants
  en NVS.
- Serveur de démonstration PC pour travailler l'interface sans matériel.
- Tests de trames NMEA 2000 exécutables sur PC.

## Matériel

Le projet utilise le module
[**Waveshare ESP32-S3-RS485-CAN**](https://www.waveshare.com/product/esp32-related/boards-kits/esp32-s3/esp32-s3-rs485-can.htm)
(SKU 32154, ou 34781 pour la version à antenne externe). C'est ce module qui est
ciblé par `platformio.ini` et par le brochage ci-dessous.

- Alimentation 7–36 V sur le bornier, ou USB-C 5 V pour le développement.
- Connexion CAN H / CAN L au backbone NMEA 2000.
- Terminaison 120 Ω aux deux extrémités du backbone. Le jumper 120 Ω de la
  carte ne doit être fermé que si la carte est en extrémité de bus.
- Un contrôleur/pupitre B&G ou Simrad actif sur le même bus.

Brochage utilisé :

| Signal | GPIO |
|---|---|
| CAN TX | 15 |
| CAN RX | 16 |
| RS485 TX (non utilisé) | 17 |
| RS485 RX (non utilisé) | 18 |
| RS485 DIR (non utilisé) | 21 |
| RTC SDA/SCL (non utilisé) | 39 / 38 |

Voir [docs/WIRING.md](docs/WIRING.md) pour le câblage et les précautions.

## Compilation et flash

Prérequis : [PlatformIO](https://platformio.org/) (VS Code ou CLI).

```bash
pio run
pio run -t upload
pio device monitor
```

Le projet utilise :

- PlatformIO `espressif32@8.0.0` ;
- framework Arduino ESP32 3.x ;
- ArduinoJson 7 ;
- carte cible `esp32-s3-devkitc-1`, flash 16 MB, partition usine de 3 MB ;
- USB CDC activé au démarrage (`ARDUINO_USB_CDC_ON_BOOT=1`) pour le moniteur
  série sur le port USB-C.

Si la carte n'est pas détectée, maintenir **BOOT**, appuyer sur **RESET**,
relâcher **BOOT**, puis relancer l'upload. Le port série est à 115200 bauds.
Si l'upload à 921600 bauds échoue, réduire `upload_speed` à `460800` dans
`platformio.ini`.

Après compilation, la mémoire utilisée est d'environ **1,06 Mo de flash** et
**58 Ko de RAM** (hors piles Wi-Fi), ce qui laisse de la marge dans la
partition de 3 Mo.

## Wi-Fi

**Aucun réseau local n'est configuré en usine.** Au démarrage :

- si un SSID STA a été enregistré et est accessible, la carte s'y connecte ;
- si le SSID STA est vide, l'AP démarre immédiatement ;
- si un SSID STA est configuré mais inaccessible, l'AP de secours démarre
  après environ **15 secondes** ;
- si la carte était connectée puis perd le réseau, l'AP de secours démarre
  après environ 10 secondes ;
- une fois l'AP actif, il reste disponible même si le STA se reconnecte
  (mode AP+STA) jusqu'au prochain redémarrage.

Valeurs par défaut de l'AP :

| Paramètre | Valeur |
|---|---|
| SSID | **`AC42-Pilot-XXXX`** (suffixe MAC) |
| Mot de passe | **`ac42pilot`** |
| Adresse | **192.168.4.1** |

L'ESP32-S3 est **2,4 GHz uniquement**. Un réseau local configuré en 5 GHz seul
ne pourra pas être rejoint et déclenchera l'AP de secours.

> Changez le mot de passe AP par défaut dans la page de réglages.

> **Mise à jour depuis la version précédente** : l'ancien réseau STA
> éventuellement mémorisé est effacé. L'AP démarre, puis le réseau peut être
> reconfiguré dans la page de réglages.

## Premier démarrage

1. Alimenter la carte.
2. Se connecter au Wi-Fi **`AC42-Pilot-XXXX`** avec le mot de passe
   **`ac42pilot`**.
3. Ouvrir **http://192.168.4.1/**.
4. Aller dans **Réglages** pour configurer un réseau local (STA), le mot de
   passe AP, le nom d'hôte ou les adresses N2K.
5. Cliquer sur **Enregistrer**. Si l'interface indique qu'un redémarrage est
   nécessaire, cliquer sur **Redémarrer**.
6. Après redémarrage :
   - si le réseau STA est accessible, la carte s'y connecte et l'IP apparaît
     dans le moniteur série ;
   - sinon, l'AP de secours redémarre après environ 15 secondes.
7. Pour revenir à l'AP seul, vider le SSID STA dans les réglages.

Le changement de SSID, de mot de passe ou de nom d'hôte enregistre les
paramètres puis demande un redémarrage explicite : l'interface affiche le
résultat de l'enregistrement et ne coupe pas la connexion toute seule.

### Page de réglages

- **Enregistrer** affiche le résultat dans la page (succès ou erreur exacte).
- Un changement réseau (SSID, mot de passe, nom d'hôte) est enregistré puis
  appliqué après un clic sur **Redémarrer**.
- Une erreur de validation ou d'écriture NVS est affichée dans la page ; le
  détail est aussi écrit dans le journal (`/api/log`) et dans le moniteur
  série :
  ```text
  Réglages: POST reçu (… octets, type 'application/json')
  Réglages enregistrés
  ```
  ou
  ```text
  Réglages refusés: <motif>
  ```

## Adresse IP dans le moniteur série

L'adresse IP de la carte est affichée au démarrage et rappelée toutes les
60 secondes environ :

```text
Wi-Fi AP: IP 192.168.4.1 - http://192.168.4.1/
Wi-Fi STA: IP 192.168.1.42 - http://192.168.1.42/ (RSSI -52 dBm)
```

Commande :

```bash
pio device monitor
```

Si l'AP et le STA sont actifs, les deux adresses sont affichées.

## Utilisation

- **Standby / Auto / Vent** : envoie le changement de mode à l'AC42.
- **−1° / +1° / −10° / +10°** : correction de cap en Auto, correction d'angle
  au vent en Vent. Les commandes sont ignorées/inhibées en standby.
- **Virement** : maintenir le bouton 2 secondes ; la barre de progression
  évite les déclenchements accidentels.
- **Barre directe** : en standby, les boutons ±10° deviennent `BARRE BÂBORD`
  et `BARRE TRIBORD`. Maintenir le doigt fait bouger la barre ; relâcher
  envoie `stop`. Le firmware arrête aussi la barre si le keepalive disparaît
  ou après 8 secondes.
- **Réponse** : appuyer sur la tuile pour cycler AUTO → LO → HI → AUTO.
- **Réglages** : Wi-Fi, adresses N2K, délai de sécurité barre, diagnostic et
  journal.

Les boutons sont désactivés tant qu'aucun heartbeat de contrôleur n'est
détecté, car l'AC42 refuserait les commandes. L'adresse de l'AC42 est détectée
automatiquement ; à défaut, la cible 0x02 est utilisée et signalée.

## Architecture

```text
src/
  main.cpp                 initialisation et boucle principale
  protocol/
    n2k_codec.c/.h         identifiants 29 bits, fast-packet
    ac42_protocol.c/.h     décodage PGN, construction des trames, état
  n2k_bus.cpp/.h           pilote TWAI 250 kbit/s, réception, bus-off
  ac42_controller.cpp/.h   adresses, commandes, sécurité NFU, niveaux
  wifi_manager.cpp/.h      AP+STA, mDNS
  web_server.cpp/.h        REST, réglages, journal, UI embarquée
  app_config.cpp/.h        NVS
  app_log.cpp/.h           journal circulaire
  web/index.html           interface web (un seul fichier)
tests/test_protocol.c      tests hôtes du protocole
tools/mock_server.py       serveur de démonstration de l’UI
android/                   application Android native (Kotlin/Compose)
docs/                      protocole, câblage, API
```

Le cœur protocole est du **C pur**, sans dépendance Arduino/ESP32. Il est donc
testable sur PC.

## Tests du protocole

Sous Linux, macOS ou WSL :

```bash
cd tests
./run_tests.sh
```

Ou manuellement :

```bash
gcc -std=c11 -Wall -Wextra -O2 -Iinclude \
  tests/test_protocol.c src/protocol/n2k_codec.c src/protocol/ac42_protocol.c \
  -lm -o tests/test_protocol
./tests/test_protocol
```

Les tests vérifient notamment les trames PGN 130850 octet par octet (modes,
corrections, virement, barre, réponse), la requête PGN 130845, la
réassemblage fast-packet et le décodage des PGN 65305/65340/65341/127237/
127250/130306/129026/127245.

## Serveur de démonstration

Pour modifier l'interface sans carte :

```bash
python tools/mock_server.py --port 8090
```

Puis ouvrir http://127.0.0.1:8090/. Le serveur simule les modes, les
instruments, les corrections, le virement et la barre directe. Il est marqué
**MODE DÉMO** dans l'interface et n'envoie aucune trame réelle.

## Application Android

Un sous-projet Android natif se trouve dans [`android/`](android/README.md).
Il reprend l’interface et les commandes de la page web, avec un compas, les
modes Auto/Vent/Standby, les corrections, le virement long, la barre directe
NFU, les réglages et le diagnostic.

- Langage : Kotlin + Jetpack Compose.
- Connexion : API REST du boîtier, HTTP local.
- Adresse par défaut : `http://192.168.4.1`, modifiable dans l’application.
- Compilation Android validée et tests de parsing JSON : 3/3.

### Télécharger l’APK

[**⬇ Télécharger l’APK Android v0.1.0 (debug)**](https://github.com/Soleiado/ac42-pilot-esp32/releases/latest/download/ac42-pilot-0.1.0-debug.apk)

- APK signé avec la clé de développement Android, à installer manuellement
  (autoriser les sources inconnues), pas via le Play Store.
- `minSdk 26`, `targetSdk 34`, environ 9,6 Mo.
- SHA-256 : `DBEE949F710D2886A7F779C5211E927E7D0D46458BB0764230585AE8C0E65FCF`
- Toutes les versions : [page Releases](https://github.com/Soleiado/ac42-pilot-esp32/releases).

Le build local reste possible :

```powershell
cd android
$env:JAVA_HOME = 'C:\Program Files\Eclipse Adoptium\jdk-21.0.13.9-hotspot'
.\gradlew.bat :app:assembleDebug
```

## API REST

Les endpoints sont documentés dans [docs/API.md](docs/API.md). Exemples :

```bash
curl http://192.168.4.1/api/status
curl -X POST http://192.168.4.1/api/mode \
  -H 'Content-Type: application/json' -d '{"mode":"auto"}'
curl -X POST http://192.168.4.1/api/course \
  -H 'Content-Type: application/json' -d '{"side":"stbd","deg":1}'
```

## Protocole

Les détails des PGN, des octets de commande, du mode bitfield et de la
nécessité d'utiliser l'adresse source du contrôleur sont dans
[docs/PROTOCOL.md](docs/PROTOCOL.md).

## Limites connues

- **Nav** n'est pas implémenté : le mode est affiché, mais les corrections et
  virements sont désactivés.
- Confirmé sur AC42. Les NAC-2/NAC-3/AC12 sont probablement compatibles mais
  n'ont pas été testés ici.
- Le vent vrai (TWA/TWS) n'est affiché que si le bus fournit un PGN 130306
  true/water referenced ou une direction de vent vraie exploitable ; avec un
  capteur apparent seul, seuls AWA/AWS sont disponibles.
- Le firmware n'émet pas d'Address Claim NMEA 2000 : il injecte les trames de
  commande avec l'adresse du contrôleur actif, comme le plugin d'origine.
- Pas d'authentification HTTP : protégez le Wi-Fi et n'exposez pas la carte
  sur un réseau non maîtrisé.
- RS485 et RTC de la carte ne sont pas utilisés dans cette version.
- Pas d'OTA ni de persistance des journaux/historiques.
- L'historique de cap de l'interface est côté navigateur (60 s).

## Crédits et licence

- Protocole AC42/Simnet : reverse engineering du plugin
  [`signalk-ac42-autopilot`](https://github.com/CaptnOliv/signalk-ac42-autopilot)
  (Apache-2.0).
- Définitions PGN : [canboat](https://github.com/canboat/canboat)
  (Apache-2.0).
- Carte et brochage : documentation Waveshare ESP32-S3-RS485-CAN.
- Réalisation : **vibe coding** avec **OpenCode** et **DeepSeek**.

Le code de ce dépôt est publié sous licence MIT — voir [LICENSE](LICENSE).
Les informations de protocole provenant de projets tiers restent soumises à
leurs licences respectives.
