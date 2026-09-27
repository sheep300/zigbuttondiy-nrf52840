# D5 � essai de correction du mode sommeil

D4 a donn� 2 / 2 / 4 : RX au repos activ� � la lecture, CAN_SLEEP observ� et aucun appel de sommeil radio instrument�. D5 conserve les contr�les et les codes de D4.

Seul changement fonctionnel : appliquer zigbee_configure_sleepy_behavior(true) dans le contexte Zigbee au signal SKIP_STARTUP (apr�s initialisation des variables et lecture NVRAM, avant le handler BDB), puis aux signaux FIRST_START ou REBOOT r�ussis. La configuration avant zigbee_enable reste pr�sente. Aucun sommeil radio forc�, aucun effacement NVRAM et aucune modification des partitions.

Ce changement teste une hypoth�se de r�initialisation du r�glage pendant le d�marrage ; la cause exacte et la consommation ne sont pas encore confirm�es. Un code 1 / 3 / 1 serait encourageant mais ne prouverait pas � lui seul le courant de veille.

## Test D5

1. Flasher ZigButtonDIY_D5.uf2, d�brancher USB, bref GND�RST.
2. V�rifier imm�diatement un Toggle re�u par Z2M.
3. Attendre trois minutes apr�s reset sans toucher l�interrupteur.
4. Basculer une fois, ignorer le flash imm�diat, noter les trois groupes et si le Toggle arrive.

Les d�tails des observations ci-dessous restent identiques � D4.

---

# Référence des observations conservées de D4

Tu n'as aucune fonction ou registre à lire toi-même : le firmware le fait. **Ce diagnostic ne force pas la radio à dormir et ne constitue pas encore un correctif.**

## Test à faire

1. Flasher `ZigButtonDIY_D5.uf2` par le bootloader habituel.
2. Débrancher l'USB, puis faire un bref **GND–RST** pour démarrer une fenêtre neuve sur batterie.
3. Confirmer immédiatement un Toggle reçu par Z2M. Laisser ensuite l'interrupteur immobile **trois minutes après le reset**.
4. Basculer une fois. Ignorer le petit flash immédiat. Après la pause, compter les **trois groupes**, séparés de deux secondes. Attendre environ dix secondes pour toute la séquence.
5. Communiquer simplement les trois nombres dans l'ordre, par exemple **`1 / 2 / 4`**, et confirmer si le Toggle arrive. L'interprétation sera faite à partir de ces résultats.

Pas besoin d'attendre une nouvelle décharge. Les mesures sont terminées après environ deux minutes trente ; après lecture, l'USB peut être rebranché. Une relecture conserve les compteurs de cette fenêtre, mais le premier groupe peut évoluer car il inclut aussi la valeur actuelle de rx_on_when_idle. Un reset recommence la fenêtre.

## Ce que D4 lit

La fenêtre reste celle de D3 : démarrage après 30 secondes puis observation pendant 120 secondes. D4 conserve le diagnostic radio matériel/pilote/HFXO de D3 en RAM, et ajoute :

- La valeur réelle `zb_get_rx_on_when_idle()`, lue dans le contexte ZBOSS lors des signaux et juste avant la commande Toggle de lecture.
- Le nombre de signaux `ZB_COMMON_SIGNAL_CAN_SLEEP` observés dans la fenêtre.
- Le nombre d'appels à `nrf_802154_sleep_if_idle()` provenant de `zb_trans_enter_sleep()`, et leur résultat accepté/refusé. Chaque appel original est conservé exactement une fois ; aucune demande supplémentaire n'est injectée.

**Attention : les codes LED de D4 sont différents de ceux de D3.**

| Groupe | Code | Signification |
|---|---:|---|
| **1 : rx_on_when_idle** | 1 | `false` au moment de la lecture, aucun `true` observé pendant la fenêtre. Cela ne garantit pas l'absence de changement entre observations. |
| | 2 | `true` au moment de la lecture. |
| | 3 | `false` actuellement, mais `true` a été observé dans la fenêtre. |
| **2 : demandes de sommeil** | 1 | Aucun CAN_SLEEP et aucun appel de sommeil radio instrumenté dans la fenêtre. |
| | 2 | CAN_SLEEP observé, mais aucun appel de sommeil radio instrumenté. |
| | 3 | Au moins un appel de sommeil radio instrumenté. |
| **3 : retour du pilote** | 1 | Tous les appels observés ont été acceptés. |
| | 2 | Certains appels ont été acceptés et d'autres refusés. |
| | 3 | Tous les appels observés ont été refusés. |
| | 4 | Aucun appel observé : aucun résultat à interpréter. |

Une série unique de **5** signifie que la fenêtre est incomplète ou que l'échantillonnage D3 est insuffisant ; **6** signifie que l'appareil n'est pas joint ou que l'état non joint a été observé pendant la fenêtre. Attendre trois minutes complètes après reset. Si 5 ou 6 persiste, le signaler sans conclure sur le sommeil.

Un retour accepté signifie que le pilote accepte la transition, pas qu'il restera endormi ensuite. Une activité radio postérieure peut le réveiller. Les refus sont normaux pendant certaines opérations ; leur simple présence n'est pas une preuve de défaut. Les compteurs concernent uniquement le chemin OSIF `zb_trans_enter_sleep()`, pas tous les chemins internes possibles du pilote. `CAN_SLEEP` signifie que la pile peut attendre, pas à lui seul que le récepteur est arrêté.

## Différence par rapport à D3

ADC toujours absent, même canal 11, même rôle ED, même polling, GPIO, RAM power-down, PM_DEVICE, partitions et mécanismes radio. Le moniteur D2 et le sampler D3 restent présents. Seules les observations supplémentaires et leur lecture LED changent. Les compteurs sont protégés par une courte section critique entre les contextes workqueue et ZBOSS. Aucune entrée/sortie, attente ou commande radio n'est exécutée dans cette section.

Le fichier `src/zb_nrf_transceiver_d4.c` est une copie complète de la source locale. Il conserve le résultat auparavant ignoré de `nrf_802154_sleep_if_idle()` et le transmet au compteur. Il ne change pas l'état logiciel OSIF, ne retente pas les refus et n'ajoute aucune demande de sommeil forcé. CMake remplace uniquement cette copie et celle du moniteur D2 dans le build D4. Les sources du SDK et de l'add-on installés ne sont pas modifiées. Les empreintes sont dans les fichiers `*_reference.json`.

Cette instrumentation perturbe légèrement l'exécution et réveille le CPU pendant la fenêtre D3 ; ce n'est pas une mesure d'autonomie. Aucun heartbeat permanent, log, ADC ou paquet radio diagnostique n'est ajouté. La tension seule, notamment après une charge, ne permet pas de déduire le courant consommé.

## Sources et compilation

NCS 3.4.1 / Zephyr 4.4.2, board `promicro_nrf52840/nrf52840/uf2`. Sources complètes dans `src` et `boards`, configuration et CMake à la racine. Fichiers générés et journal dans `artifacts`, résultats de contrôle dans `verification.json`.

La lecture du getter vient de `C:/ncs/ncs-zigbee/lib/zboss/include/zboss_api.h`. L'appel instrumenté est dans `C:/ncs/ncs-zigbee/subsys/osif/zb_nrf_transceiver.c`. Les retours NONE/BUSY sont ceux de `C:/ncs/v3.4.1/nrfxlib/nrf_802154/driver/src/nrf_802154.c`, fonction `nrf_802154_sleep_if_idle`. Les vérifications portent sur les sources réellement installées.

Dans un chemin court, `.\build.ps1` reconstruit puis contrôle l'UF2 sans flasher. Le build de travail est `C:/ncs/zigbutton_diagnostics_20260924/D5/build`. Les adresses exactes, avertissements et empreinte du binaire sont consignés dans `verification.json`. Aucun essai physique n'est prétendu réalisé par la compilation.

## Résultat à expliquer

D3 a donné **4 / 4 / 4** : radio non désactivée, pilote hors sommeil et HFXO actif dans au moins 90 % des observations de la fenêtre. D4 vérifie désormais la politique de réception et le chemin des demandes de sommeil, sans supposer d'avance la cause de ce résultat.

