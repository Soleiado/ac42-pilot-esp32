# Protocole AC42 / Simnet utilisé par le firmware

Ce document décrit les trames NMEA 2000 échangées par le firmware. Il s'appuie
sur le travail de reverse engineering du plugin
[`signalk-ac42-autopilot`](https://github.com/CaptnOliv/signalk-ac42-autopilot)
et sur les définitions PGN de [canboat](https://github.com/canboat/canboat).

## 1. Trame CAN NMEA 2000

NMEA 2000 utilise des trames CAN **29 bits** à **250 kbit/s**. L'identifiant
est construit ainsi :

```text
id = (priorité << 26) | (PGN << 8) | adresse_source
```

- priorité : 3 bits (0 = la plus haute, 7 = la plus basse) ;
- PGN : 18 bits ;
- adresse source : 8 bits (0–253 en pratique).

Le firmware écoute tout le bus (filtre d'acceptation matériel ouvert) et
applique les filtres par PGN en logiciel.

### Fast-packet

Les PGN 130850, 130845, 127237 et 130851 dépassent 8 octets et utilisent le
protocole fast-packet NMEA 2000 :

- premier octet : `(compteur_séquence << 5) | numéro_de_trame` ;
- trame 0 : deuxième octet = longueur totale du message, puis 6 octets utiles ;
- trames suivantes : 7 octets utiles par trame ;
- le dernier octet est complété par `0xFF` si nécessaire.

Le compteur de séquence est sur 3 bits (0–7) et ne doit pas être confondu avec
le numéro de trame (5 bits).

## 2. PGN 130850 — Simnet: AP Command

C'est la trame de commande du pilote. Le firmware l'émet en fast-packet, avec
la **priorité 2**, l'identifiant CAN :

```text
0x09FF22SS     (SS = adresse source du contrôleur actif)
```

### Structure

```text
Trame 0 : [seq<<5|00] 0C 41 9F TT FF FF CC
Trame 1 : [seq<<5|01] P6 P7 P8 P9 P10 P11 FF
```

- `41 9F` : fabricant 1857 (Simrad) + industrie 4 (marine), en little-endian ;
- `TT` : adresse NMEA 2000 de l'AC42 (cible) ;
- `CC` : classe de commande — `0x0A` pour les commandes AP, `0x02` pour la
  barre directe NFU ;
- `P6..P11` : six octets de charge utile.

### Commandes

| Commande | Octet événement | P6..P11 |
|---|---:|---|
| Standby | `0x06` | `06 00 00 00 00 00` |
| Auto / maintien de cap | `0x09` | `09 00 FF FF FF FF` |
| Vent / angle apparent | `0x0F` | `0F 00 00 00 00 00` |
| Changement de cap | `0x1A` | `1A 00 dir angleLo angleHi FF` |
| Virement | `0x11` | `11 00 dir FF FF FF` |
| Paramètres bas forcés | `0x08` | `08 00 FF FF FF FF` |
| Paramètres haut forcés | `0x07` | `07 00 FF FF FF FF` |
| Paramètres automatiques | `0x46` | `46 00 FF FF FF FF` |
| Barre directe NFU | `0x0D` | `0D 00 dir 00 00 00` |

### Sens de barre

Le sens dépend du mode pour le changement de cap. C'est contre-intuitif, mais
validé sur un AC42 réel :

| Commande | Mode | `+` / tribord / abattre | `−` / bâbord / lofer |
|---|---|---|---|
| Change course | Auto | `0x03` | `0x02` |
| Change course | Vent | `0x02` | `0x03` |
| Tack | — | `0x03` (tribord) | `0x02` (bâbord) |
| Barre NFU | — | `0x05` (tribord) | `0x04` (bâbord) |
| Barre NFU stop | — | `0xFF` | — |

### Angle

L'angle est exprimé en **0,0001 radian**, little-endian. Les valeurs validées
sur un pupitre B&G réel sont :

- 1° = `0x00AE` ;
- 10° = `0x06D1`.

Le firmware n'accepte que 1° et 10° pour les corrections, afin de rester dans
le domaine validé.

## 3. PGN 130845 — Simnet: Key Value

Ce PGN permet de lire les réglages de l'AC42, notamment les niveaux de
réponse. La requête est émise en fast-packet, priorité 3 :

```text
0x0DFF1DSS     (SS = adresse source du contrôleur actif)
```

Exemple avec le groupe de séquence 1 :

```text
Trame 0 : 20 0E 41 9F TT FF FF FF
Trame 1 : 21 keyLo keyHi 00 00 FF FF FF
Trame 2 : 22 FF FF FF FF FF FF FF
```

Clés utilisées :

| Clé | Réglage |
|---|---|
| `0x1119` | niveau de réponse du jeu Bas |
| `0x111A` | niveau de réponse du jeu Haut |
| `0x111C` | niveau de réponse en mode Vent |

Une réponse AC42 a la structure :

```text
41 9F TT FF FF FF keyLo keyHi 00 op value FF FF FF
```

- `op = 0x02` : réponse de l'AC42 ;
- `op = 0x01` : écriture par un MFD ;
- `value` : 1 à 10.

Le firmware décode aussi les écritures faites depuis un MFD, ce qui permet de
suivre un changement de réglage sans émettre de requête.

## 4. PGN 65305 — Simnet: Device Status / Pilot Mode

PGN 65305, priorité 3, 8 octets, plusieurs variantes distinguées par l'octet
`Report` :

```text
41 9F model report data...
```

| Report | Émetteur | Signification |
|---:|---|---|
| 2 | AC42 | Device Status : octet 4 = `2` manuel, `16` automatique |
| 3 | contrôleur | Send Status : heartbeat du pupitre/MFD |
| 10 | AC42 | Pilot Mode : champ Mode 16 bits |
| 11 | contrôleur | Send Mode : heartbeat du pupitre/MFD |
| 23 | processeur de voile | Sailing Processor Status (ignoré) |

Le heartbeat PGN 65305 est la condition qui autorise l'AC42 à accepter des
commandes. Le firmware mémorise les adresses source des reports 3 et 11, et
utilise la plus récente (hors adresse de l'AC42) comme adresse source des
commandes.

### Bitfield de mode (report 10)

Le champ 16 bits little-endian contient :

| Bit | Valeur | Signification |
|---:|---:|---|
| 1 | `0x0002` | jeu de paramètres Haut actif |
| 2 | `0x0004` | commutation automatique par la vitesse |
| 3 | `0x0008` | Standby |
| 4 | `0x0010` | Heading / Auto |
| 6 | `0x0040` | Nav |
| 8 | `0x0100` | No Drift |
| 10 | `0x0400` | Wind |

Les bits 1 et 2 ne sont significatifs que lorsque le pilote est engagé ; en
standby, l'AC42 les met à zéro.

## 5. PGN 65340 — Simnet: Autopilot Mode State

PGN 65340, priorité 3, 8 octets. L'AC42 l'émet normalement toutes les secondes.

```text
41 9F modeClass mode C D spare flags
```

- `modeClass` : `0` standby, `16` engagé ;
- `mode` : `0` standby, `1` heading/Auto, `3` mode 4, `4` Wind, `5` NFU,
  `6` Navigation.

Le firmware s'en sert en priorité pour l'état engagé/standby et pour le mode.
Le PGN 65305 reste le fallback pour les NAC plus récents qui n'émettent pas
65340.

## 6. PGN 65341 — Simnet: Autopilot Angle

PGN 65341, 8 octets :

```text
41 9F 00 00 mode 00 angleLo angleHi
```

- `mode` = 3 (Wind) pour une consigne d'angle au vent ;
- `angle` en 0,0001 rad, 0–2π, normalisé ensuite en −180…+180°.

Le firmware l'utilise pour la cible de vent. Le cap cible est lu sur 127237.

## 7. PGN 127237 — Heading/Track Control

Fast-packet de 21 octets. Champs utilisés :

| Octet(s) | Champ | Échelle |
|---|---|---|
| 1 bits 6–7 | référence : 0 vrai, 1 magnétique | — |
| 3–4 | angle de barre commandé | 0,0001 rad signé |
| 5–6 | cap à tenir (`Heading-To-Steer`) | 0,0001 rad |
| 19–20 | cap du navire | 0,0001 rad |

Le firmware ne retient la cible que si la source est l'AC42 (une fois son
adresse connue). La référence vraie/magnétique est convertie avec la variation
magnétique du PGN 127250 quand elle est disponible.

## 8. PGN d'instruments

| PGN | Contenu utilisé |
|---|---|
| 127250 | cap, référence, variation magnétique |
| 129026 | COG, SOG |
| 128259 | vitesse surface / fond |
| 128267 | profondeur |
| 130306 | vent apparent ou vrai, référence |
| 127245 | position de barre, angle commandé |
| 127251 | taux de rotation (non utilisé par l'UI) |

Le firmware convertit les unités NMEA 2000 (rad, m/s, m) vers les unités de
l'interface (degrés, nœuds, mètres).

## 9. Détection des adresses

1. Chaque PGN 65305 report 3/11 met à jour la table des contrôleurs.
2. Chaque PGN 65305 report 2/10, 65340, 65341 ou réponse 130845 identifie
   l'adresse de l'AC42.
3. L'AC42 est retiré de la table des contrôleurs.
4. Le contrôleur le plus récent (moins de 5 s) est utilisé comme adresse
   source des commandes.
5. L'adresse AC42 détectée est utilisée comme cible ; à défaut, la cible
   `0x02` est utilisée et signalée dans l'interface.
6. Les adresses peuvent être forcées dans les réglages ; le forçage est
   affiché et prioritaire.

## 10. Sécurités logicielles

- Une commande n'est émise que si un heartbeat de contrôleur est présent
  (sauf adresse source forcée).
- Débit limité à environ 8 commandes par seconde (écart minimum 120 ms).
- Barre directe NFU uniquement en standby.
- Keepalive NFU côté client toutes les 300 ms.
- Watchdog serveur : arrêt de la barre après `rudder_timeout_ms` sans
  keepalive (1 s par défaut), plafond absolu de 8 s.
- Arrêt immédiat de la NFU si le pilote passe engagé.
- Virement par appui long de 2 s.
- Les commandes de correction et de virement sont refusées en Nav et en
  standby.
- Le bus CAN est surveillé ; un état bus-off déclenche une récupération.

## 11. Limites

- Le firmware n'émet pas d'Address Claim NMEA 2000. Il injecte des trames avec
  l'adresse source d'un contrôleur existant. C'est le comportement du plugin
  d'origine et la condition observée pour que l'AC42 obéisse.
- Le mode Nav n'est pas piloté : l'interface le détecte mais désactive les
  corrections.
- Les commandes ne sont pas acquittées par un PGN 130851 : l'interface attend
  le changement d'état diffusé par l'AC42.
- Les temporisations et le comportement exact peuvent varier selon la version
  firmware de l'AC42 et des pupitres.
