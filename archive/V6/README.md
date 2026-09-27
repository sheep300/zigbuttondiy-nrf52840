# ZigButtonDIY V6 — LiPo et envoi de toggle suivi

Firmware destiné à remplacer D5. Compilé pour NCS 3.4.1 / Zephyr 4.4.2 et l’add-on Zigbee local. La compilation et les tests logiciels ne remplacent pas le contrôle sur la carte.

## Mise en service (quelques minutes)

1. Copier `ZigButtonDIY_V6.uf2` sur le bootloader UF2 habituel.
2. Débrancher USB et faire un bref GND–RST, comme pour D5. L’association mémorisée est conservée.
3. Effectuer 10 bascules espacées d’environ 2 secondes. Vérifier 10 événements `toggle` dans Z2M (une publication JSON et une publication `/action` pour la même bascule constituent un seul événement).
4. Refaire une bascule après une minute sans activité. Un flash indique un retour d’envoi positif de la pile ; trois flashes indiquent un échec, une saturation, une commande expirée ou un résultat incertain. Plus de séquences 1/3/1.
5. Vérifier le rapport batterie après connexion. Comparer la tension annoncée à celle du multimètre, sur batterie seule. Le champ ZCL tension a une résolution de 0,1 V ; Z2M peut l’exprimer en mV. Le pourcentage est une estimation LiPo, pas une calibration de cette cellule.

Un succès de transport ne prouve pas l’exécution d’une automatisation Home Assistant. Les flashes peuvent se superposer lors de bascules rapides ; utiliser les événements Z2M pour compter. Si trois flashes apparaissent, noter si Z2M a reçu un toggle avant de refaire une action : un accusé peut être perdu après réception.

## Changements

- Correction sommeil de D5 conservée : application avant démarrage, au signal SKIP_STARTUP et à FIRST_START/REBOOT réussis, dans le contexte Zigbee. Le handler par défaut conserve sa gestion CAN_SLEEP.
- Suppression des moniteurs D2/D3/D4, des copies OSIF instrumentées et de leurs réveils d’échantillonnage.
- P0.17, pull-up, anti-rebond de 35 ms, une commande par changement d’état stable dans les deux sens. Aucun toggle au démarrage.
- File RAM de 8 commandes, un seul envoi en vol. Nouvelle tentative de programmation/allocation toutes les 100 ms tant qu’une commande attend ; expiration après 3 secondes. Une commande hors réseau est rejetée et signalée, pas rejouée après reconnexion.
- Appel direct à `zb_zcl_finish_and_send_packet`, retour immédiat vérifié, callback `zb_zcl_command_send_status_t` vérifié et buffer libéré. Destination coordinateur 0x0000, endpoint 1, cluster On/Off, commande Toggle. Sécurité AUTO et comportement APS ACK unicast du SDK conservés.
- Pas de réémission applicative après un envoi soumis dont le résultat est incertain : un nouveau toggle pourrait inverser une seconde fois. Les retransmissions protocolaires restent confiées à la pile. Aucun système ne peut promettre « exactement une fois » de bout en bout avec une commande toggle sans mécanisme supplémentaire côté destinataire.
- Après 15 secondes sans callback, trois flashes signalent l’incertitude. Le buffer reste à la pile et les envois suivants ne partent pas tant que ce callback n’est pas revenu. Un retour tardif libère correctement le buffer ; un blocage permanent nécessite un reset. Aucun reset automatique ni boucle radio permanente n’est ajouté.
- Mesure batterie au démarrage, rapport après connexion, puis mesure/rapport toutes les 4 heures. Pas de mesure ADC au clic. Une erreur ADC ne désactive plus le bouton. Les rapports ont également un callback avec libération du buffer.
- Cluster Power Configuration et descripteur du candidat LiPo restaurés. Pas de changement de nom/modèle ni de convertisseur Z2M imposé. Si les attributs batterie ne sont pas reconnus, une nouvelle interview peut être nécessaire ; aucun effacement réseau n’est demandé par défaut.

## Batterie LiPo 1S

Entrée interne VDDH/5, facteur de reconstruction x5, courbe interpolée :

| mV | % estimé |
|---:|---:|
| 3000 | 0 |
| 3300 | 1 |
| 3500 | 5 |
| 3600 | 10 |
| 3700 | 25 |
| 3800 | 45 |
| 3900 | 65 |
| 4000 | 80 |
| 4100 | 90 |
| 4200 | 100 |

Le pourcentage ZCL est encodé en demi-pourcents (0..200). Correction gain/offset dans `src/battery_config.h`, laissée à l’identité faute de comparaison réelle. La mesure n’est valide comme tension de batterie que si VDDH suit effectivement la batterie sur cette carte. Pas d’estimation d’autonomie à partir du seul pourcentage. Aucun chargeur ni protection batterie n’est piloté par ce firmware.

## Validation et limites

Tests hôte exécutant le vrai `src/toggle.c` avec des substituts Zephyr/ZBOSS : construction de commande, confirmation positive/négative, manque de buffer, expiration, échec de programmation, envois en série, absence de réseau, callback manquant puis tardif, saturation et erreur immédiate. Tests de monotonie et bornes de la vraie courbe LiPo. Ils ne simulent pas les interruptions matérielles, les courses réelles du noyau ni la radio.

`tests/run.ps1` utilise le compilateur MSVC local avec un lanceur Windows sans CRT. Les journaux, configuration, DTS, ELF, map et contrôles UF2 sont dans `artifacts` et `verification.json`. La compilation n’est pas un essai matériel.

Toutes les partitions restent identiques : application 0x26000..0xEB000, configuration 0xEB000..0xEC000, NVRAM 0xEC000..0xF4000, bootloader 0xF4000..0x100000, zone réservée 0..0x26000. Le fichier UF2 ne couvre que l’application. Le projet original `C:/ncs/zigbuttondiy` et l’add-on installé ne sont pas modifiés.

Reconstruction : placer les sources dans un chemin court puis lancer `build.ps1`. Build de travail : `C:/ncs/zigbutton_diagnostics_20260924/V6/build`.
