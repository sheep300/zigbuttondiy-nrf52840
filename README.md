# ZigButtonDIY nRF52840

**ZigButtonDIY** est un petit module Zigbee DIY sur batterie permettant de transformer un **interrupteur bistable ou un simple contact sec** en commande Zigbee pour **Zigbee2MQTT / Home Assistant**.

Le projet est basé sur un **nRF52840 compatible nice!nano / Pro Micro** et fonctionne comme un **Zigbee Sleepy End Device**, afin de limiter au maximum la consommation sur batterie.

L'objectif initial est très simple : pouvoir placer un petit module autonome derrière un interrupteur mural existant et récupérer chaque changement de position dans Home Assistant, **sans relier le module au secteur**.

```text
 Interrupteur / contact sec
          │
          │
     P0.17 / D2
          │
          ▼
      nRF52840
       + LiPo
          │
          │ Zigbee
          ▼
    Zigbee2MQTT
          │
          ▼
    Home Assistant
```

Chaque changement d'état du contact envoie une commande Zigbee :

```text
action: toggle
```

L'interrupteur peut donc être utilisé pour piloter une lampe, un relais, une automatisation Home Assistant ou n'importe quelle autre action.

> ⚠️ Le nRF52840 ne commute **aucune tension secteur**.  
> L'entrée P0.17 est uniquement prévue pour un **contact sec entre P0.17 et GND**.

---

## Fonctionnalités

La version actuelle propose :

- fonctionnement sur **LiPo 1S 3,7 V / 4,2 V** ;
- communication Zigbee ;
- fonctionnement en **Sleepy End Device** ;
- très faible activité radio lorsque le module est au repos ;
- une commande `toggle` à chaque changement stable du contact ;
- détection sur les deux positions d'un interrupteur bistable ;
- anti-rebond logiciel de 35 ms ;
- file d'attente des commandes Zigbee ;
- retour visuel par LED ;
- mesure de la batterie ;
- remontée du niveau de batterie dans Zigbee2MQTT ;
- mesure batterie au démarrage puis toutes les 4 heures ;
- aucune mesure ADC lors d'un changement d'interrupteur ;
- procédure de remise à zéro de l'appairage directement depuis l'interrupteur ;
- recherche de réseau Zigbee sur les canaux 11 à 26 ;
- firmware UF2 prêt à flasher.

---

## Matériel nécessaire

Le projet a été développé pour une carte :

- **nRF52840 compatible nice!nano / Pro Micro**
- avec **bootloader UF2**
- alimentation sur **LiPo 1S 3,7 V**
- un interrupteur bistable ou n'importe quel **contact sec**

La carte utilisée pendant le développement est un clone nRF52840 compatible avec la cible Zephyr :

```text
promicro_nrf52840/nrf52840/uf2
```

Le comportement exact du chargeur LiPo et du circuit d'alimentation peut varier selon les clones.

### Batterie

La version actuelle est prévue pour une :

```text
LiPo 1S
3,7 V nominal
4,2 V pleine charge
```

La batterie doit être raccordée à l'entrée batterie prévue par votre carte nRF52840, en respectant le brochage de celle-ci.

---

## Branchement de l'interrupteur

Le contact utilisé par ZigButtonDIY est :

```text
P0.17 / D2
```

Branchement :

```text
P0.17 / D2 ───── interrupteur ───── GND
```

Le firmware active le pull-up interne du nRF52840.

Le contact peut donc avoir deux états :

```text
Ouvert  → P0.17 au niveau haut
Fermé   → P0.17 relié à GND
```

Les **deux changements d'état** déclenchent une commande `toggle`.

Il n'y a donc aucune notion de position ON ou OFF imposée par le module : il détecte simplement que l'utilisateur a changé la position de l'interrupteur.

---

# Installation

## Firmware prêt à flasher

La version actuelle est :

### **ZigButtonDIY V7**

Le firmware UF2 prêt à l'emploi est disponible à la racine du dépôt :

[`ZigButtonDIY_V7.uf2`](./ZigButtonDIY_V7.uf2)

Aucune compilation n'est nécessaire pour simplement utiliser le module.

---

## Flash par UF2

Sur la carte utilisée pour le développement :

1. relier brièvement **RST à GND deux fois rapidement** ;
2. le bootloader UF2 apparaît comme un lecteur USB ;
3. copier :

```text
ZigButtonDIY_V7.uf2
```

sur ce lecteur ;
4. attendre le redémarrage de la carte ;
5. débrancher l'USB pour revenir sur alimentation batterie.

Le firmware ne modifie pas le bootloader.

Le flash seul ne supprime pas l'association Zigbee déjà enregistrée.

---

# Première utilisation

## Module déjà associé à un réseau

Après le flash :

1. remettre la carte sous tension ;
2. ne pas manipuler l'interrupteur pendant environ **5 secondes** ;
3. attendre l'indication LED de connexion ;
4. basculer l'interrupteur.

Zigbee2MQTT doit recevoir :

```text
action: toggle
```

et publier également :

```text
zigbee2mqtt/<appareil>/action
```

avec :

```text
toggle
```

Le module reste ensuite silencieux au niveau applicatif lorsqu'il n'est pas utilisé, à l'exception des échanges nécessaires au fonctionnement Zigbee et des rapports périodiques de batterie.

---

# Appairage et changement de réseau

V7 permet de supprimer l'association Zigbee directement avec l'interrupteur, sans bouton supplémentaire.

La procédure utilise volontairement un geste difficile à déclencher accidentellement.

## Procédure 6 + 2

Avant de commencer, ouvrir l'appairage sur Zigbee2MQTT.

Puis :

1. effectuer un reset matériel **RST ↔ GND** ;
2. pendant les **5 premières secondes**, effectuer **6 changements de position** ;
3. la LED produit une rafale rapide ;
4. effectuer ensuite **2 changements supplémentaires dans les 5 secondes** ;
5. le module efface son association Zigbee ;
6. il recherche alors un nouveau réseau ;
7. une fois associé, il reprend son fonctionnement normal.

Un aller-retour complet de l'interrupteur représente deux changements de position.

```text
RESET

   ↓

6 bascules
en moins de 5 s

   ↓

confirmation LED

   ↓

2 bascules
supplémentaires

   ↓

Reset Zigbee

   ↓

Recherche réseau

   ↓

Appairage
```

Si les deux bascules de confirmation ne sont pas effectuées, **aucune association n'est supprimée**.

Les bascules utilisées pendant cette procédure ne sont pas envoyées comme commandes `toggle`.

Pour recommencer la procédure, effectuer un nouveau reset matériel.

---

# Recherche du réseau Zigbee

V7 recherche un réseau sur l'ensemble des canaux :

```text
11 à 26
```

Lorsqu'un réseau est déjà enregistré, celui-ci est normalement réutilisé au démarrage.

Après un reset Zigbee volontaire, le module effectue une nouvelle recherche.

Les tentatives automatiques sont limitées afin d'éviter qu'un module hors réseau maintienne inutilement une activité radio élevée pendant une longue période.

---

# Fonctionnement de la LED

La LED permet de connaître rapidement l'état des commandes sans connexion série.

En fonctionnement normal :

| LED | Signification |
|---|---|
| 1 flash court | commande Zigbee transmise avec retour positif de la pile |
| 3 flashes courts | erreur, expiration ou résultat d'envoi incertain |
| flash long | connexion Zigbee réussie |
| rafale rapide | demande de reset/appairage détectée |

Le flash de confirmation indique que la pile Zigbee a accepté ou confirmé l'envoi.

Il ne garantit évidemment pas que l'automatisation Home Assistant située trois étages plus loin a elle-même allumé la lampe. Zigbee a déjà suffisamment de responsabilités comme ça.

---

# Batterie

ZigButtonDIY utilise le cluster Zigbee **Power Configuration**.

La batterie est mesurée :

```text
au démarrage
puis toutes les 4 heures
```

Un changement d'interrupteur **ne déclenche aucune mesure batterie**.

Cela permet de garder le chemin du bouton aussi court que possible :

```text
Interruption GPIO
      ↓
anti-rebond
      ↓
file Toggle
      ↓
Zigbee
```

La mesure batterie utilise l'entrée interne :

```text
VDDH / 5
```

puis reconstruit une tension estimée de la batterie.

---

## Pourcentage LiPo

La courbe actuelle est une estimation pour une LiPo 1S.

Quelques points de référence :

| Tension | Batterie estimée |
|---:|---:|
| 4,20 V | 100 % |
| 4,10 V | 90 % |
| 4,00 V | 80 % |
| 3,90 V | 65 % |
| 3,80 V | 45 % |
| 3,70 V | 25 % |
| 3,60 V | 10 % |
| 3,50 V | 5 % |
| 3,30 V | 1 % |
| 3,00 V | 0 % |

Cette valeur reste **indicative**.

Une LiPo n'a pas une courbe de décharge parfaitement linéaire et ZigButtonDIY n'est pas un fuel gauge.

De plus, la chaîne de mesure VDDH du clone utilisé n'a pas encore été étalonnée précisément avec plusieurs mesures simultanées au multimètre.

L'attribut Zigbee standard `BatteryVoltage` possède également une résolution de seulement **0,1 V**.

---

# Faible consommation

Le module est configuré comme **Zigbee End Device** et utilise le fonctionnement Sleepy End Device de ZBOSS.

Un point important découvert pendant le développement est que le paramètre :

```text
rx_on_when_idle = false
```

doit rester effectif après l'initialisation de la pile Zigbee.

Une version de diagnostic conservait la réception radio active au repos et pouvait vider une LiPo de 200 mAh en quelques heures.

Le firmware actuel réapplique donc le comportement sleepy après l'initialisation Zigbee.

Les essais réalisés sur carte réelle ont montré une différence très importante entre le firmware avec réception permanente et la version corrigée.

L'autonomie longue durée exacte n'est cependant pas encore annoncée : elle dépend notamment de la batterie, du clone nRF52840, du réseau Zigbee et de l'état du contact.

---

# Gestion des commandes Toggle

Chaque changement stable de P0.17 crée une commande.

Le firmware utilise :

- anti-rebond de **35 ms** ;
- file de **8 commandes** ;
- une seule commande Zigbee en vol ;
- expiration d'une commande en attente après **3 secondes** ;
- suivi du retour d'envoi ZBOSS ;
- retransmissions Zigbee laissées à la pile réseau.

En cas d'incertitude après émission, le firmware ne génère volontairement pas un second Toggle applicatif.

Un `toggle` envoyé deux fois produirait en effet exactement le résultat inverse de celui souhaité, ce qui est une manière particulièrement élégante de transformer une gestion d'erreur en bug.

---

# Utilisation avec Zigbee2MQTT

L'appareil apparaît comme un :

```text
Battery powered End Device
```

Une action normale produit notamment :

```json
{
  "action": "toggle"
}
```

Zigbee2MQTT publie également :

```text
zigbee2mqtt/<friendly_name>/action
```

avec :

```text
toggle
```

La batterie apparaît via le cluster Power Configuration.

Le projet est principalement testé avec **Zigbee2MQTT** et **Home Assistant**.

---

# Version actuelle : V7

V7 apporte principalement :

- reset d'appairage avec la séquence **6 + 2 bascules** ;
- recherche Zigbee multicanal 11 à 26 ;
- maintien du fonctionnement Sleepy End Device corrigé ;
- mesure LiPo périodique ;
- file de commandes Toggle ;
- gestion des retours d'envoi ;
- limitation des tentatives de reconnexion ;
- retour LED amélioré.

La procédure de reset, le départ du réseau, le nouvel appairage et les Toggles après réappairage ont été testés sur la carte réelle utilisée pendant le développement.

Les anciennes versions et les firmwares de diagnostic sont conservés dans :

[`archive/`](./archive/)

Ils sont présents pour l'historique et le diagnostic et ne constituent pas les versions recommandées pour une nouvelle installation.

---

# Compiler le projet

Le firmware actuel a été compilé avec :

```text
Nordic NCS 3.4.1
Zephyr 4.4.2
ncs-zigbee local
GNU 14.3.0
```

Cible :

```text
promicro_nrf52840/nrf52840/uf2
```

Le dépôt contient :

```text
CMakeLists.txt
prj.conf
boards/
src/
tests/
build.ps1
check_uf2.py
```

Le module Zigbee utilisé par le projet est attendu ici :

```text
C:\ncs\ncs-zigbee
```

Le script fourni utilise également l'installation NCS locale :

```text
C:\ncs\v3.4.1
```

et la toolchain :

```text
C:\ncs\toolchains\4f5b6ad6dd
```

Pour reconstruire :

```powershell
.\build.ps1
```

Le script :

1. compile le projet ;
2. produit le firmware UF2 ;
3. contrôle automatiquement les adresses contenues dans le fichier UF2.

Le firmware généré se trouve ensuite dans :

```text
build\zephyr\zephyr.uf2
```

---

# Organisation de la Flash

Le projet utilise le partitionnement suivant :

| Zone | Début | Fin |
|---|---:|---:|
| zone réservée / SoftDevice | `0x000000` | `0x026000` |
| application | `0x026000` | `0x0EB000` |
| ZBOSS Product Config | `0x0EB000` | `0x0EC000` |
| ZBOSS NVRAM | `0x0EC000` | `0x0F4000` |
| bootloader UF2 | `0x0F4000` | `0x100000` |

Le script `check_uf2.py` vérifie que le firmware généré reste dans la zone autorisée.

> Ne pas modifier ce partitionnement sans vérifier précisément l'image produite.  
> Le bootloader UF2 occupe la partie haute de la Flash.

---

# Tests

Le dépôt contient également des tests hôte destinés notamment à vérifier :

- la file des commandes Toggle ;
- les expirations ;
- différents cas d'erreur ;
- la logique du geste d'appairage ;
- les limites temporelles de la séquence 6 + 2 ;
- l'annulation d'un reset incomplet ;
- la courbe LiPo.

Ces tests complètent les essais réalisés sur matériel réel mais ne remplacent pas les tests radio et de consommation sur la carte.

---

# Limites actuelles

Quelques points restent volontairement documentés comme tels :

- la tension batterie du clone n'est pas encore étalonnée précisément ;
- le pourcentage LiPo est une estimation ;
- l'autonomie longue durée n'est pas encore chiffrée ;
- tous les clones nRF52840 compatibles nice!nano n'utilisent pas forcément exactement le même circuit d'alimentation ;
- le projet a principalement été validé avec Zigbee2MQTT.

---

# Historique du projet

ZigButtonDIY est né d'un besoin simple : obtenir un module Zigbee réellement compact et autonome pouvant se cacher derrière un interrupteur standard.

Le développement a notamment nécessité plusieurs firmwares de diagnostic afin d'isoler une consommation anormalement élevée de la radio Zigbee.

Les anciennes versions, variantes de diagnostic et premières versions CR2032 sont conservées dans :

[`archive/`](./archive/)

La version recommandée est toujours le firmware présent à la racine du dépôt.

---

## Licence

Aucune licence n'est actuellement indiquée dans le dépôt.

Avant de réutiliser ou redistribuer publiquement le code, ajouter un fichier `LICENSE` correspondant à la licence souhaitée.
