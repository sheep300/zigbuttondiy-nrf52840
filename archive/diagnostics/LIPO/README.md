# Version batterie LiPo 1S — candidate à valider sur carte

Cette version répond à la demande ajoutée pendant l'analyse : batterie définitive LiPo **3,7 V nominal / 4,2 V pleine**. Elle est distincte de C/D et ne remplace pas leur comparaison de consommation.

## Comportement

- Une mesure ADC au démarrage ; première demande de report après la connexion/réintégration Zigbee réussie.
- Une nouvelle mesure et une demande de report toutes les **quatre heures**, à compter du démarrage, via un work différé Zephyr. Pas de boucle d'attente active.
- Envoi direct au coordinateur 0x0000, endpoint 1, des attributs Power Configuration `BatteryVoltage` et `BatteryPercentageRemaining`, comme dans le projet d'origine.
- Le bouton reste P0.17, pull-up, deux fronts, debounce 35 ms. Son chemin ne contient plus aucun ADC ni report batterie.
- La lecture ADC reste dans la workqueue Zephyr ; la mise à jour des attributs et l'envoi restent dans le contexte ZBOSS.
- Pas de logs, USB, watchdog, heartbeat supplémentaire ou LED périodique. Comme V5, cette variante n'a pas de LED de diagnostic bouton.
- Hors réseau, la mesure périodique peut actualiser les attributs locaux mais ne force pas un rejoin pour envoyer la batterie. Une demande d'envoi n'est pas une garantie de livraison radio ; le prochain report périodique donnera une nouvelle occasion de mise à jour.

## Courbe LiPo utilisée

Interpolation linéaire entre les points suivants, bornée à 0–100 %. Cette table est une **estimation générique** et non une courbe mesurée pour ta cellule.

| Tension supposée de cellule | Pourcentage |
|---:|---:|
| ≤ 3,00 V | 0 % |
| 3,30 V | 1 % |
| 3,50 V | 5 % |
| 3,60 V | 10 % |
| 3,70 V | 25 % |
| 3,80 V | 45 % |
| 3,90 V | 65 % |
| 4,00 V | 80 % |
| 4,10 V | 90 % |
| ≥ 4,20 V | 100 % |

Le firmware envoie ce résultat multiplié par deux, conformément à l'unité Zigbee 0,5 %. Le pourcentage dépend de la tension **non arrondie** ; la tension Zigbee est ensuite arrondie au dixième de volt. Des couples tels que 4,0 V / 78 % sont donc possibles. Le pourcentage n'est pas une jauge de charge, ni une protection contre la décharge ; sous charge ou pendant la recharge il peut être trompeur.

## Étalonnage tension : préparé, pas encore effectué

Le canal **VDDH/5**, le gain ADC 1/6, la référence 0,6 V et le facteur logiciel **x5** sont conservés. Une correction séparée est définie dans `src/battery_config.h` :

```text
Vcorrigée_mV = VDDH_mV × BATTERY_CAL_GAIN_NUM / BATTERY_CAL_GAIN_DEN
               + BATTERY_CAL_OFFSET_MV
```

Valeurs livrées : gain 1000/1000 et offset 0 mV. Aucun coefficient inventé n'est présenté comme un étalonnage.

1. Faire un relevé sur **batterie seule**, USB débranché, au moment d'un report de démarrage ou d'un report périodique. Noter la tension au multimètre aux bornes de la batterie, la valeur reçue dans Z2M et l'heure. Un Toggle ne produit plus de nouveau report batterie.
   Pour obtenir une mesure de démarrage sur batterie, il faut un **redémarrage sur batterie seule** : débrancher l'USB après le flash ne refait pas la mesure déjà prise sous USB. Sinon, attendre le prochain report de quatre heures.
2. Refaire plus tard à une tension sensiblement différente. Ne pas essayer de caler la courbe à partir d'une lecture pendant la charge USB.
3. Si la tension annoncée suit celle de la cellule, on pourra corriger un décalage/gain constant. Si elle reste près d'une tension régulée alors que la batterie descend, il faudra d'abord résoudre le chemin de mesure : un coefficient logiciel ne permet pas de retrouver la batterie à partir d'un rail indépendant.
4. L'attribut standard `BatteryVoltage` est en unités de **100 mV**, donc une lecture Z2M ne donne pas un étalonnage fin. Un écart inférieur à environ 50 mV peut simplement venir de l'arrondi. Pour une précision supérieure, il faudra lire `battery_last_mv` via une instrumentation adaptée ou ajouter un canal diagnostique séparé.

Z2M peut appliquer sa propre conversion selon le convertisseur associé au périphérique. La réception effective de ces deux attributs et l'utilisation du pourcentage fourni par le firmware restent à vérifier sur ton contrôleur ; aucun accès Z2M n'a été utilisé ici.

## Utilisation

Le binaire est `artifacts/zephyr.uf2`. Les sources complètes, l'overlay original, le `.config`, le DTS final et le journal sont fournis. Toutes les limites flash sont contrôlées comme pour C/D.

Pour reconstruire depuis la racine du paquet :

```powershell
.\build.ps1 LIPO
```

Tester d'abord C/D reste nécessaire pour isoler la décharge. Cette variante LiPo dérive de V5 et modifie la politique de mesure/report ainsi que la courbe : elle ne doit pas être interprétée comme un test à une seule variable par rapport à D.
