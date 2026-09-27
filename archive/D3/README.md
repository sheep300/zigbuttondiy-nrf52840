# D3 — observation radio, pilote et horloge

**D3 est un diagnostic, pas un correctif ni un test d'autonomie.** Il ajoute une fenêtre de mesure de deux minutes à D2, sans changer les paramètres Zigbee, le polling, l'ADC absent, la gestion d'énergie ni les partitions. Aucun flash n'a été réalisé par l'assistant.

## Ce que tu fais

1. Flasher `ZigButtonDIY_D3.uf2` par le bootloader habituel.
2. Débrancher l'USB, puis faire **un bref contact GND–RST** pour lancer une fenêtre neuve sur batterie seule. Ce reset fixe le début du test ; il ne corrige pas la cause de l'absence de réaction observée auparavant après flash/débranchement.
3. Vérifier immédiatement un Toggle dans Z2M, puis laisser l'interrupteur immobile **trois minutes après le reset**.
4. Basculer une fois. Ignorer le petit flash immédiat. Après une pause de 0,7 seconde, la LED donne **trois groupes**, séparés par deux secondes d'extinction. Attendre la fin, environ dix secondes au maximum.
5. Envoyer les trois nombres dans l'ordre, par exemple **`1 / 4 / 4`**, ainsi que la tension et si le Toggle arrive dans Z2M.

Les résultats sont figés à la fin de la fenêtre. Une autre bascule permet de relire les mêmes trois groupes ; elle ne recommence pas la mesure. Un reset recommence une fenêtre. Il n'est pas nécessaire de laisser fonctionner des heures ni de recharger entre deux relectures.

## Les trois groupes

| Groupe | Ce qui est compté |
|---|---|
| **1 — radio** | Proportion des observations où le registre matériel RADIO.STATE est différent de DISABLED. |
| **2 — pilote** | Proportion où le pilote 802.15.4 n'est pas dans son état interne SLEEP. |
| **3 — horloge** | Proportion où HFCLKSTAT indique l'oscillateur haute fréquence à quartz HFXO actif. |

Pour chaque groupe :

| Clignotements | Proportion d'observations |
|---:|---|
| **1** | Moins de 1 % |
| **2** | De 1 % inclus à 10 % exclus |
| **3** | De 10 % inclus à 90 % exclus |
| **4** | Au moins 90 % |

Deux résultats spéciaux remplacent les trois groupes :

- **5 clignotements, une seule série** : fenêtre pas encore terminée ou observations trop peu nombreuses/trop espacées. Attendre trois minutes complètes après reset ; si 5 persiste, le signaler.
- **6 clignotements, une seule série** : appareil actuellement non joint, ou état non joint observé pendant la fenêtre. Ce résultat n'est pas une mesure de repos connecté.

Si tu vois seulement le petit flash initial mais aucune série après la pause, le signaler. Si aucune LED ne répond, noter l'état sans réinitialiser immédiatement. Éviter les bascules pendant l'affichage : une nouvelle demande remplace la séquence en cours.

Exemples d'interprétation, qui restent des orientations :

- `4 / 4 / 4` : radio et pilote très souvent actifs ; rechercher réception persistante, trafic ou réseau instable.
- `1 / 4 / 4` : matériel radio surtout désactivé, mais pilote hors SLEEP et HFXO actif ; examiner les ressources/créneaux radio conservés par le pilote.
- `1 / 1 / 4` : pilote surtout en sommeil, HFXO souvent actif ; un autre demandeur d'horloge peut exister.
- `1 / 1 / 1` : aucune activité durable de ces trois éléments n'a été vue pendant cette fenêtre. Cela ne prouve ni un courant faible ni l'absence d'un problème intermittent plus tard.

## Méthode et limites

Après un délai initial de **30 secondes**, environ 1 200 observations sont prises sur **120 secondes**. L'espacement varie de 50 à 149 ms pour réduire la synchronisation avec les polls Zigbee. Un petit générateur arithmétique déterministe est utilisé ; il ne demande pas le périphérique RNG. Le work ne se reprogramme plus après la fin de la fenêtre : il n'y a pas de heartbeat permanent.

Les lectures sont en lecture seule : RADIO.STATE, CLOCK.HFCLKSTAT et `nrf_802154_core_state_get()`. Aucun appel sleep/receive supplémentaire, aucune écriture aux tâches RADIO, aucune demande de clock, aucune émission diagnostique et aucun ADC n'est ajouté. Les commandes Toggle existantes restent inchangées. Le moniteur D2 est conservé, avec ses résultats en RAM ; l'affichage LED devient celui de D3.

L'échantillonnage réveille temporairement le CPU et peut modifier légèrement l'ordonnancement. **D3 ne doit pas servir à mesurer l'autonomie ni un courant de repos.** Les pourcentages sont des fractions d'échantillons, pas un rapport cyclique exact. Des trames très courtes peuvent échapper à l'observation, les trois états ne sont pas lus atomiquement, et le marqueur réseau est mis à jour lors des signaux ZBOSS. Une fenêtre calme n'exclut pas une dérive ultérieure. Les observations ne distinguent pas tous les mécanismes de consommation internes au silicium.

Une fenêtre est refusée si elle compte moins de 600 observations, si un intervalle dépasse une seconde ou si sa durée dépasse 125 secondes. Les compteurs sont publiés par une opération atomique à la fin ; la lecture LED ne consulte les compteurs qu'une fois la fenêtre terminée. Les données RAM détaillées `d3_*` sont accessibles avec un débogueur, mais aucun débogueur n'est nécessaire pour les trois groupes.

## Sources vérifiées

- `C:/ncs/v3.4.1/nrfxlib/nrf_802154/common/include/nrf_802154.h:486–513` : l'état sommeil du pilote libère sa demande d'horloge haute fréquence et ses créneaux radio ; un autre module peut garder l'horloge active.
- `.../driver/src/nrf_802154_core.h:61` : `RADIO_STATE_SLEEP` est le seul état sans demandes de ressources radio.
- `.../driver/src/nrf_802154_core.c:2837` : le getter renvoie simplement l'état interne volatile, sans modifier le pilote.
- `C:/ncs/ncs-zigbee/subsys/osif/zb_nrf_transceiver.c:211` : l'OSIF demande le sommeil et ignore le résultat. D3 n'utilise pas le drapeau logiciel OSIF comme preuve du sommeil matériel.
- [Spécification Nordic du registre RADIO.STATE](https://docs.nordicsemi.com/r/bundle/ps_nrf52840/page/radio.html) : DISABLED signifie absence d'opération radio ; les autres états correspondent aux phases RX/TX et transitions. Ce registre seul ne mesure pas la consommation totale de la carte.

Le getter interne est choisi après inspection de cette version précise du pilote, et non comme API portable pour toutes les versions de NCS. L'ensemble compile avec les en-têtes locaux de NCS 3.4.1 / Zephyr 4.4.2. Le SDK et l'add-on ne sont pas modifiés. Le seul remplacement OSIF est la copie instrumentée déjà utilisée dans D2.

## Fichiers

Sources complètes : `src`, `boards`, `CMakeLists.txt`, `Kconfig`, `prj.conf`. Fichiers générés et journal dans `artifacts`. Contrôles dans `verification.json`. Le firmware à flasher est fourni à la racine.

Reconstruction depuis un chemin court :

```powershell
.\build.ps1
```

Le script utilise le SDK existant, compile et contrôle l'UF2, sans flasher. Le dossier de travail compilé est `C:/ncs/zigbutton_diagnostics_20260924/D3`. Les cinq partitions restent identiques à celles de D et D2 ; tous les blocs UF2 et segments chargés de l'ELF sont contrôlés dans la zone application. Les avertissements éventuels et les bornes exactes sont inscrits dans le fichier de vérification.

## Résultats précédents

C a répondu après 12 h 36 avec 3,982 → 3,980 V. D a conduit à une sortie batterie coupée après 11 h ; un très bref USB a rétabli une sortie à 2,987 V. D2, après reset sur batterie, a donné 2 puis 1 clignotement : après stabilisation, au moins 90 % du temps était passé dans l'attente du thread Zigbee. Les relevés D2 étaient 4,107 V après 8 minutes, puis 4,064 V après le relevé annoncé à 41 minutes. Cette dernière baisse, prise après une charge, ne suffit pas à elle seule à prouver une décharge anormale : la relaxation de la batterie reste un facteur. D3 cherche des états radio persistants, pas à déduire des mA de ces tensions.
