# Câblage et mise en service

## Carte

Le firmware cible la carte **Waveshare ESP32-S3-RS485-CAN** (SKU 32154) ou sa
variante à antenne externe **ESP32-S3-RS485-CAN-U** (SKU 34781).

Caractéristiques utiles :

- ESP32-S3, Wi-Fi 2,4 GHz, 16 MB de flash ;
- CAN isolé, piloté par le contrôleur TWAI interne ;
- RS485 isolé (non utilisé ici) ;
- RTC PCF85063 (non utilisé ici) ;
- alimentation 7–36 V par bornier, ou 5 V par USB-C ;
- boîtier rail DIN.

## Brochage

| Fonction | GPIO | Bornier / connecteur |
|---|---|---|
| CAN TX | 15 | — |
| CAN RX | 16 | — |
| CAN H | — | borne CAN H |
| CAN L | — | borne CAN L |
| RS485 TX | 17 | — |
| RS485 RX | 18 | — |
| RS485 DIR | 21 | — |
| RTC SDA | 39 | — |
| RTC SCL | 38 | — |
| Alimentation + | — | 7–36 V DC |
| Alimentation − | — | 0 V |

Le firmware ne configure pas les GPIO RS485/RTC. Ils restent disponibles pour
une évolution.

## Raccordement NMEA 2000

```text
Backbone NMEA 2000
   CAN H ────────────────┬─────────────── CAN H
   CAN L ────────────────┼─────────────── CAN L
                         │
                    carte ESP32
                    (drop cable)
```

Règles importantes :

1. **Ne pas alimenter la carte par le bus NMEA 2000.** Utiliser le bornier
   7–36 V ou l'USB-C.
2. Utiliser un câble de dérivation NMEA 2000 correct (paire torsadée, blindage)
   et un connecteur de dérivation, ou un bornier propre.
3. Le backbone doit avoir **exactement deux terminaisons 120 Ω**, une à chaque
   extrémité. Si la carte est une dérivation au milieu du bus, le jumper
   120 Ω de la carte doit rester **ouvert**. Si elle est en extrémité, il peut
   être fermé.
4. Relier le blindage selon la pratique du navire ; ne pas créer de boucle de
   masse.
5. Vérifier la continuité CAN H/CAN L et l'absence de court-circuit avant
   l'alimentation.
6. Le CAN de la carte est isolé, mais l'alimentation et l'isolation du bus
   doivent rester cohérentes avec l'installation du navire.

## Alimentation

- Bornier : 7–36 V DC, respecter la polarité.
- USB-C : 5 V, pratique pour le flash et les tests à quai.
- La carte possède une isolation de puissance pour la partie CAN/RS485.
- Prévoir un fusible ou disjoncteur adapté au câble d'alimentation.
- Éviter d'alimenter la carte et le bus NMEA 2000 par des sources qui créent
  des différences de potentiel importantes.

## Voyants

| Voyant | Signification |
|---|---|
| PWR | alimentation présente |
| CAN | clignote lors du trafic CAN |
| RS485 vert/bleu | émission/réception RS485 (non utilisé) |

## Antenne Wi-Fi

- Version 32154 : antenne céramique interne.
- Version 34781 : connecteur SMA et antenne externe.
- Pour une portée correcte dans un cockpit, préférer la version U avec
  antenne externe et placer l'antenne dégagée.
- Le boîtier rail DIN est métallique/fermé : vérifier que l'antenne n'est pas
  totalement masquée.

## Première mise en service

1. Câbler le CAN et l'alimentation.
2. Vérifier les terminaisons et l'absence de court-circuit.
3. Flasher le firmware (USB-C).
4. Ouvrir le moniteur série à 115200 bauds : l'adresse IP de la carte y est
   affichée au démarrage et rappelée toutes les 60 s.
5. Au premier démarrage, l'AP **`AC42-Pilot-XXXX`** démarre immédiatement
   (mot de passe `ac42pilot`) : ouvrir `http://192.168.4.1/`.
6. Dans **Réglages**, configurer un réseau local (STA) et/ou le mot de passe
   AP. Vider le SSID STA force l'AP seul.
7. Après redémarrage, si le réseau STA est accessible la carte s'y connecte
   et son IP STA apparaît dans le moniteur ; sinon l'AP de secours redémarre
   après environ 15 s.
8. Vérifier dans le diagnostic :
   - `CAN : running` ;
   - un `PUPITRE` avec une adresse (heartbeat) ;
   - un `AC42` avec une adresse ;
   - les instruments qui se mettent à jour.
9. Tester les modes **à quai**, barre dégagée.
10. Vérifier le sens des corrections et du virement avant toute utilisation en
    mer.

```text
Wi-Fi AP: IP 192.168.4.1 - http://192.168.4.1/
Wi-Fi STA: IP 192.168.1.42 - http://192.168.1.42/ (RSSI -52 dBm)
```

## Dépannage

| Symptôme | Pistes |
|---|---|
| Pas de point d'accès après 15 s | alimentation, flash, mot de passe AP, moniteur série ; vérifier que le STA n'a pas réussi |
| Pas de connexion au réseau STA configuré | SSID/mot de passe, portée Wi-Fi 2,4 GHz, DHCP ; l'AP de secours doit apparaître après 15 s |
| IP introuvable | regarder le moniteur série (ligne `Wi-Fi ... IP ...`) ou le bail DHCP du routeur |
| `CAN non prêt` | câblage CAN H/L, alimentation, terminaison, bus-off |
| Aucun pupitre détecté | le MFD/pupitre doit être allumé et émettre PGN 65305 |
| AC42 non détecté | vérifier l'adresse N2K, forcer `0x02` dans les réglages |
| Commandes ignorées | heartbeat absent, mauvaise adresse cible, AC42 en défaut |
| Barre directe ne s'arrête pas | vérifier le watchdog et le Wi-Fi ; couper l'alimentation du pilote si nécessaire |
| Wi-Fi instable | RSSI, antenne, canal, éloignement du routeur |

## Sécurité

Ce montage envoie des ordres réels à un organe de barre. À quai, déconnecter
la commande du vérin ou s'assurer que personne ne se trouve près de la barre.
En mer, garder une veille permanente et savoir reprendre la barre manuellement.
