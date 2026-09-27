# D2 — observation du thread Zigbee, sans appareil de mesure

Cette version repart de D. Elle ne constitue pas encore un correctif de consommation : elle observe le temps passé dans l'appel d'attente `k_poll()` de la couche Zigbee et donne le résultat par LED, uniquement lorsque tu actionnes le contact.

## Premier essai : quelques minutes

1. Après recharge, flasher **ZigButtonDIY_D2.uf2** via le bootloader UF2 habituel.
2. Débrancher l'USB. Noter heure et tension. Confirmer qu'un Toggle arrive dans Z2M.
3. Laisser le contact immobile **deux minutes**, puis le basculer une seule fois.
4. Ignorer le tout petit flash immédiat de l'IRQ. **Après une pause d'environ 0,7 seconde**, compter les clignotements de diagnostic, plus longs. Attendre la fin de la séquence avant une nouvelle action.
5. Communiquer le nombre de clignotements, le résultat du Toggle dans Z2M et la tension. Un second contrôle après 15 minutes peut vérifier la répétabilité. Inutile d'attendre onze heures ou de laisser la protection batterie couper à nouveau.

| Clignotements après la pause | Signification |
|---:|---|
| **1** | Au moins 90 % du temps de la fenêtre passé dans l'attente Zigbee. |
| **2** | Entre 10 % inclus et 90 % exclus : beaucoup de temps hors attente. |
| **3** | Moins de 10 % : le thread entre très peu ou très brièvement dans cette attente. |
| **4** | ZBOSS se déclare non joint au moment du contrôle. |
| **5** | Moins d'une minute d'observation disponible : attendre au moins deux minutes avant de recommencer. |
| Seulement le flash très bref, sans séquence ensuite | L'IRQ fonctionne, mais le callback ZBOSS ou l'affichage différé n'a peut-être pas abouti ; le signaler. |

La première fenêtre commence au démarrage, les suivantes au dernier relevé ayant une fenêtre d'au moins une minute. Après un relevé précoce de moins d'une minute, la fenêtre est conservée. Les clignotements durent 150 ms chacun, espacés de 350 ms. Aucun clignotement autonome ou heartbeat n'est ajouté. Les actions répétées pendant une séquence la rendent difficile à interpréter : attendre quelques secondes.

**Un seul clignotement ne certifie pas une faible consommation.** Le temps mesuré est du temps écoulé dans `k_poll`, y compris les interruptions et les autres tâches pendant cette attente. Ce n'est ni une mesure directe du CPU en WFI, ni de la radio éteinte. Si le code 1 revient alors que la batterie baisse fortement, la suite devra examiner radio, MPSL, autres threads et périphériques. Les codes 2/3 justifieront l'étude de l'activité du thread et des réveils. Le code 4 oriente vers le réseau/rejoin. Les seuils sont des classes diagnostiques, pas une spécification énergétique.

## Une différence expérimentale : l'instrumentation

ADC absent, Power Configuration absent, même rôle ED, canal 11, même polling, RAM power-down, PM_DEVICE, GPIO P0.17, pull-up, debounce et partitions que D. Le programme ne force pas de pause et ne modifie pas `rx_on_when_idle`, les délais ZBOSS ou les demandes radio. Le Toggle reste envoyé comme dans D.

Les seuls ajouts sont le chronométrage en RAM et sa lecture par une courte séquence LED sur demande. Cette instrumentation consomme un peu de CPU à chaque attente et de LED au relevé ; elle n'est pas destinée à chiffrer des microampères. Aucun ADC, report batterie, log ou trafic radio diagnostique n'est réintroduit.

Le fichier complet `src/zb_nrf_platform_d2.c` est une copie de la source locale de l'add-on, avec seulement l'inclusion du moniteur et deux appels autour du `k_poll` existant. CMake remplace ce seul fichier **pour le build D2** et refuse de configurer s'il ne trouve pas exactement une source à remplacer. Le SDK et le module `C:/ncs/ncs-zigbee` restent inchangés. `source_reference.json` identifie l'empreinte de la source de départ.

La documentation [Zephyr sur l'attente k_poll](https://docs.zephyrproject.org/latest/kernel/services/polling.html) décrit ce mécanisme. L'implémentation réellement utilisée a été vérifiée dans `C:/ncs/v3.4.1/zephyr/kernel/poll.c` et dans l'OSIF local ; aucune hypothèse de correctif fondée uniquement sur une version distante n'est appliquée.

## Fichiers et reconstruction

Sources complètes dans `src`, `prj.conf`, `Kconfig`, `CMakeLists.txt`, `boards`. Les fichiers générés et le journal sont dans `artifacts`. L'UF2 à utiliser est aussi fourni à la racine sous le nom **ZigButtonDIY_D2.uf2**.

Dans un chemin court, avec NCS et le module aux emplacements déjà utilisés :

```powershell
.\build.ps1
```

Le script compile puis vérifie l'UF2 ; il ne flashe rien. Le build de travail utilisé est `C:/ncs/zigbutton_diagnostics_20260924/D2/build`. Les résultats vérifiés sont dans `verification.json`.

## Résultats utilisateur qui motivent D2

- C après reset, 24/09 à 18 h 28 : 3,982 V, LED OK.
- C le 25/09 à 07 h 04 : 3,980 V, LED OK ; durée 12 h 36.
- D le 25/09 à 07 h 11 : 3,984 V, Toggle OK ; environ une minute d'USB juste avant.
- D à 18 h 11 : 0,005 V en sortie ; 2,987 V après un très bref branchement USB puis débranchement. Compatible avec un déclenchement de protection après décharge ; le courant consommé n'a pas été mesuré.

Ces observations impliquent fortement l'ensemble Zigbee et ses dépendances. Elles ne permettent pas encore d'identifier le composant fautif. D2 doit apporter une observation supplémentaire, pas être présenté comme une réparation déjà validée.
