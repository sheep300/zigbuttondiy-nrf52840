# ZigButtonDIY V7

Version prête à flasher, avec reset d’appairage par l’interrupteur bistable. Le sommeil D5/V6, la file de toggles et la mesure LiPo sont conservés. Les tests logiciels et la compilation sont réalisés ; l’effacement puis l’appairage sur une carte réelle restent à valider.

## Installer sans perdre le réseau actuel

Copier `ZigButtonDIY_V7.uf2` sur le bootloader habituel. Débrancher USB et faire un bref GND–RST. Ne pas manipuler l’interrupteur durant les cinq premières secondes d’exécution de l’application. Le flash seul ne supprime pas l’appairage existant.

Deux flashes très courts annoncent le démarrage de l’application et l’ouverture de la fenêtre de reset. Un flash long (environ 0,8 seconde) indique que la pile a signalé une connexion réussie ; il est différé jusqu’à la fin de la fenêtre initiale si nécessaire. Cela ne prouve pas que Home Assistant a déjà traité toutes les informations de l’appareil. Le souci transitoire des premières commandes observé en V6 n’est pas déclaré corrigé.

## Changer de réseau : 6 + 2 bascules

1. Ouvrir l’autorisation d’appairage sur le Z2M voulu. Pour éviter de rejoindre l’ancien réseau, ne pas y laisser l’appairage ouvert.
2. Faire GND–RST. Dès les deux petits flashes de démarrage, effectuer **6 changements de position en moins de 5 secondes**. Un aller-retour compte pour deux bascules.
3. La LED produit une rafale rapide : effectuer **2 bascules supplémentaires dans les 5 secondes qui suivent la sixième** pour confirmer.
4. V7 demande le reset Zigbee via `zb_bdb_reset_via_local_action(0)`. Deux flashes moyens indiquent que le signal de fin de reset a été reçu avec succès ; un flash long indique ensuite la connexion au réseau. Si les événements se succèdent très vite, le flash long peut remplacer l’indication intermédiaire.
5. Sans confirmation, rien n’est effacé. Trois flashes courts indiquent l’annulation de confirmation ou une erreur.

**Les bascules de la fenêtre initiale et de la confirmation sont consommées : elles ne commandent pas la domotique et ne sont pas rejouées.** Après 5 secondes sans séquence complète, l’usage normal reprend. La reconnaissance du reset ne se réarme pas pendant l’usage normal : il faut un nouveau reset matériel.

Seuls les paramètres Zigbee sont remis à zéro par l’API prévue par ZBOSS. Selon son contrat, elle conserve notamment le compteur de trames sortantes et les éventuelles données applicatives. Ni firmware, ni bootloader, ni partitions ne sont effacés par un accès flash direct de l’application. La fiche de l’ancien appareil dans l’ancien Z2M peut nécessiter un nettoyage séparé.

## Recherche de réseau et batterie

V7 recherche sur les canaux 11 à 26 ; V6 était limité au canal 11. Le réseau mémorisé reste utilisé au démarrage tant que le reset d’appairage n’est pas confirmé.

`CONFIG_ZIGBEE_DEV_REJOIN_TIMEOUT_MS=120000` limite la période de tentatives automatiques selon le mécanisme du SDK. Après environ deux minutes sans succès, les relances s’arrêtent ; une opération radio déjà en cours peut se terminer après ce délai. Ce n’est pas une coupure radio forcée à la milliseconde près. Une nouvelle bascule en usage normal appelle `user_input_indicate()` et peut relancer les tentatives. Elle ne sera pas rejouée comme toggle après connexion : refaire une bascule quand le réseau est disponible.

La fenêtre du geste emploie seulement les interruptions GPIO et des échéances ponctuelles, sans scrutation permanente. Le scan multicanal consomme pendant l’appairage, pas en continu lorsque l’appareil est associé. Aucun chiffre de courant nouveau n’a été mesuré.

La LiPo 1S (3,7 V nominal, 4,2 V pleine) est mesurée au démarrage puis toutes les quatre heures ; rapport initial après connexion, puis rapports périodiques de tension et pourcentage. L’entrée VDDH/5, la reconstruction x5 et la courbe V6 restent inchangées. Le pourcentage est estimatif, le gain/offset restent à l’identité faute de mesures comparées. L’attribut tension ZCL a une résolution de 0,1 V. Une erreur ADC n’empêche pas le bouton de fonctionner.

## Toggles et LED en usage normal

- Un flash court : retour d’envoi positif de la pile Zigbee, pas une garantie d’exécution d’une automatisation.
- Trois flashes courts : échec, saturation, expiration ou résultat incertain.
- Une commande par changement stable de P0.17, anti-rebond de 35 ms, file de huit commandes, un envoi en vol.
- Attente maximale avant soumission de trois secondes. Réessais d’allocation/programmation tant que cette attente n’est pas expirée.
- Pas de nouveau toggle applicatif après un envoi incertain : il pourrait provoquer une seconde inversion. Retransmissions protocolaires confiées à Zigbee.
- Après 15 secondes sans callback d’envoi, signalement d’incertitude et conservation du buffer à la pile. Un callback tardif débloque les envois ; un blocage permanent nécessite un reset.

Si le reset Zigbee lui-même n’a pas confirmé sa fin après environ 15 secondes, trois flashes signalent le problème, sans répétition automatique de l’effacement. Faire un reset matériel et vérifier l’état réseau avant de recommencer.

## Vérification courte sur carte

1. Après flash sans geste de reset, attendre le démarrage puis vérifier plusieurs toggles et la présence du rapport batterie.
2. Pour valider la nouvelle fonction, ouvrir l’appairage Z2M, exécuter volontairement 6 + 2 et vérifier la reconnexion puis un toggle. Ce test efface bien l’ancienne association sur l’appareil.
3. Une séquence 6 sans les deux confirmations doit conserver l’association après expiration des cinq secondes de confirmation.

## Sources, tests et compilation

NCS 3.4.1 / Zephyr 4.4.2, add-on Zigbee local, board `promicro_nrf52840/nrf52840/uf2`. `build.ps1` reconstruit depuis un chemin court et contrôle l’UF2. Build utilisé : `C:/ncs/zigbutton_diagnostics_20260924/V7/build`.

Les tests hôte incluent le vrai code de toggle et de pairing avec des substituts des API Zephyr/ZBOSS : seuils du geste, frontières temporelles, annulation, absence de toggles pendant le geste, attente d’initialisation de pile, un seul appel de reset, signaux leave/join rapprochés, expiration d’une demande non soumise. Les tests V6 de file, erreurs, callback tardif et courbe LiPo restent exécutés. Ils ne remplacent pas des essais radio ni des tests de concurrence matérielle.

Résultats : `verification.json`, `artifacts/build.log`, `artifacts/tests.log`. `tests/run.ps1` permet de relancer les tests hôte sur l’installation MSVC locale.

La zone application reste 0x26000..0xEB000 ; configuration produit 0xEB000..0xEC000 ; NVRAM 0xEC000..0xF4000 ; bootloader 0xF4000..0x100000 ; zone réservée 0..0x26000. Le contrôle UF2 vérifie chaque bloc. Le projet original et le SDK ne sont pas modifiés.


## Historique et �tat de validation

Les anciennes versions sont dans `archive/`. La version courante et son fichier UF2 sont � la racine. Le SDK Nordic et l�add-on Zigbee doivent �tre install�s s�par�ment aux chemins utilis�s par les scripts et CMake.

Sur la carte de l�utilisateur : toggles re�us, rapport batterie 88 %, d�part du r�seau puis nouvel appairage confirm� par Z2M et toggle apr�s r�appairage le 27 septembre 2026. Cela valide le r�appairage sur le m�me r�seau ; migration vers un autre r�seau, d�lai sans r�seau disponible et autonomie longue dur�e non test�s mat�riellement. Le probl�me transitoire des premi�res commandes apr�s d�marrage n�est pas d�clar� corrig�.
