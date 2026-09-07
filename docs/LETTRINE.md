# Lettrine et mapping LED — WordClock FR 8×13

> Reconstitué depuis `archive/WordClock_code_Lettrine.xlsx`, qui numérote les LED
> de **1 à 104**. Ce document est la version **0-based (0 → 103)**, celle utilisée
> par le code Arduino. C'est le document de référence : toute modification de la
> grille se répercute ici **avant** le code.

## 1. Topologie du ruban

- 104 LED WS2812B, 8 lignes × 13 colonnes.
- Câblage en **serpentin** : lignes paires (0, 2, 4, 6) de gauche à droite,
  lignes impaires (1, 3, 5, 7) de droite à gauche.
- LED 0 en haut à gauche.

| Ligne | Sens | Index LED (gauche → droite) |
|---|---|---|
| 0 | → | 0 … 12 |
| 1 | ← | 25 … 13 |
| 2 | → | 26 … 38 |
| 3 | ← | 51 … 39 |
| 4 | → | 52 … 64 |
| 5 | ← | 77 … 65 |
| 6 | → | 78 … 90 |
| 7 | ← | 103 … 91 |

## 2. Grille des lettres (lecture visuelle)

```
L0   U N E U F D E U X S E P T      LED   0 →  12
L1   C I N Q U A T R E H U I T      LED  25 →  13
L2   T R O I S I X M I N U I T      LED  26 →  38
L3   O N Z E M I D I X S T A R      LED  51 →  39
L4   H E U R E S K M O I N S Y      LED  52 →  64
L5   L E T V I N G T - C I N Q      LED  77 →  65
L6   D I X Q U A R T D E M I E      LED  78 →  90
L7   T I M E B Y W I Z O O O O      LED 103 →  91
```

Lettres mutualisées — c'est ce qui permet de tenir en 13 colonnes :

| Partage | Détail |
|---|---|
| `UNEUF` | `UNE` (0-2) et `NEUF` (1-4) partagent `N` et `E` |
| `CINQUATRE` | `CINQ` (22-25) et `QUATRE` (17-22) partagent le `Q` (LED 22) |
| `TROISIX` | `TROIS` (26-30) et `SIX` (30-32) partagent le `S` (LED 30) |
| `MIDIX` | `MIDI` (44-47) et `DIX` (43-45) partagent `D` et `I` |
| `HEURES` | `HEURE` = 52-56 (1 h), `HEURES` = 52-57 (2 h → 11 h) |
| `LET` | `ET` (75-76) et `LE` (76-77) partagent le `E` (LED 76) |

Lettres de remplissage, jamais allumées : `K` (58), `Y` (64), `S T A R` (39-42).
Le tiret (69) n'est allumé que dans « VINGT-CINQ ».

## 3. Mapping heures

| Heure | Mots | LED |
|---|---|---|
| 0 | MINUIT | 33,34,35,36,37,38 |
| 1 | UNE HEURE | 0,1,2 + 52…56 |
| 2 | DEUX HEURES | 5,6,7,8 + 52…57 |
| 3 | TROIS HEURES | 26…30 + 52…57 |
| 4 | QUATRE HEURES | 17…22 + 52…57 |
| 5 | CINQ HEURES | 22,23,24,25 + 52…57 |
| 6 | SIX HEURES | 30,31,32 + 52…57 |
| 7 | SEPT HEURES | 9,10,11,12 + 52…57 |
| 8 | HUIT HEURES | 13,14,15,16 + 52…57 |
| 9 | NEUF HEURES | 1,2,3,4 + 52…57 |
| 10 | DIX HEURES | 43,44,45 + 52…57 |
| 11 | ONZE HEURES | 48,49,50,51 + 52…57 |
| 12 | MIDI | 44,45,46,47 |

## 4. Mapping minutes

| Minute | Mots | LED |
|---|---|---|
| 00 | *(rien)* | — |
| 05 | CINQ | 65,66,67,68 |
| 10 | DIX | 78,79,80 |
| 15 | ET QUART | 75,76 + 81…85 |
| 20 | VINGT | 70…74 |
| 25 | VINGT-CINQ | 65…74 (tiret 69 inclus) |
| 30 | ET DEMIE | 75,76 + 86…90 |
| 35 | MOINS VINGT-CINQ | 59…63 + 65…74 |
| 40 | MOINS VINGT | 59…63 + 70…74 |
| 45 | MOINS LE QUART | 59…63 + 76,77 + 81…85 |
| 50 | MOINS DIX | 59…63 + 78,79,80 |
| 55 | MOINS CINQ | 59…63 + 65,66,67,68 |

> **Écart avec le tableur d'origine** : sa ligne « 30 min » donnait `86;87;88;89`,
> soit `DEMI`. Le code retient `86…90`, soit `DEMIE`, qui est la bonne valeur.
> C'est le tableur qui est à corriger, pas le code.

## 5. Groupes annexes

| Groupe | LED | Règle |
|---|---|---|
| AM (matin) | 93, 94 | 2 pastilles |
| PM (après-midi) | 91, 92, 93, 94 | 4 pastilles |
| TIMEBYWIZ | 95 … 103 | allumé de 21:00:00 à 21:00:59 |

## 6. Règles d'affichage

La minute est arrondie au multiple de 5 le plus proche : `minIdx = (m + 2) / 5`,
valeur comprise entre 0 et 12.

| minIdx | minutes réelles | mot | heure prononcée |
|---|---|---|---|
| 0 | 00 01 02 | *(heure pile)* | h |
| 1 | 03 → 07 | CINQ | h |
| 2 | 08 → 12 | DIX | h |
| 3 | 13 → 17 | ET QUART | h |
| 4 | 18 → 22 | VINGT | h |
| 5 | 23 → 27 | VINGT-CINQ | h |
| 6 | 28 → 32 | ET DEMIE | h |
| 7 | 33 → 37 | MOINS VINGT-CINQ | **h + 1** |
| 8 | 38 → 42 | MOINS VINGT | h + 1 |
| 9 | 43 → 47 | MOINS LE QUART | h + 1 |
| 10 | 48 → 52 | MOINS DIX | h + 1 |
| 11 | 53 → 57 | MOINS CINQ | h + 1 |
| 12 | 58 59 | *(heure pile)* | h + 1 |

Chaque tranche dure exactement 5 minutes et l'écart entre l'heure réelle et
l'heure lue ne dépasse jamais 2 minutes.

Trois règles en découlent, et ce sont celles qui ont produit les défauts corrigés :

1. **Le passage à « MOINS » et le passage à l'heure suivante sont un seul et même
   événement.** Le code bascule à `m > 32`, ce qui correspond exactement à
   `minIdx > 6`. Si les deux seuils divergent, l'horloge affiche pendant quelques
   minutes une heure qui n'existe pas, par exemple « onze heures et demie » à
   10 h 31.
2. **`minIdx == 12` est une heure pleine, pas un « moins cinq ».** L'arrondi donne
   60 pour les minutes 58 et 59 : la tranche à afficher est l'heure pile de
   `h + 1`. Ramener cette valeur sur 11 allongeait « moins cinq » à 7 minutes.
3. **AM/PM se lit sur l'heure réelle, jamais sur l'heure prononcée.** À 23 h 40 on
   affiche « minuit moins vingt », mais on est toujours l'après-midi.

`MINUIT` et `MIDI` ne prennent jamais le mot `HEURE(S)`. `1 h` prend `HEURE` au
singulier (52-56), les autres `HEURES` (52-57).

Conséquence normale de la règle 3 : `MIDI` et `MINUIT` sont les seules phrases qui
changent de pastille en cours d'affichage. `MIDI` s'affiche de 11 h 58 à 12 h 02,
donc en AM deux minutes puis en PM trois. C'est correct : à 11 h 58 il est encore
le matin.
