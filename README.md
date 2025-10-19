# Algorithme de hachage rapide pour quadruplet IP/port

## Introduction

Le but de cette documentation est d'implémenter une fonction de hachage rapide basée sur le quadruplet (IP source, port source, IP destination, port destination). Des exemples permettront de vérifier que le hachage est différent pour des quadruplets très similaires.

## Préambule

### OU EXCLUSIF (XOR)

L'opérateur XOR est un opérateur binaire qui effectue une opération logique.

Il prend deux bits en entrée et renvoie un bit en sortie selon la table de vérité suivante :

| A | B | A XOR B |
|---|---|---------|
| 0 | 0 |    0    |
| 0 | 1 |    1    |
| 1 | 0 |    1    |
| 1 | 1 |    0    |

Il permet de « mélanger » des données. Par exemple, considérons deux couples d'adresses IP différents :

- Couple A : A1 = `192.168.0.1`, A2 = `192.168.0.2`  
- Couple B : B1 = `192.168.0.1`, B2 = `192.168.0.3`

XOR(A1, A2) = 0.0.0.3
XOR(B1, B2) = 0.0.0.2

Le fait d'effectuer un XOR entre les éléments des couples A et B produit un résultat qui mélange l'information. Voir l'exemple implémenté dans `xor.c`.

### Décalage de bits vers la gauche ou vers la droite (`<<` et `>>`)

Les opérations de décalage de bits `>>` et `<<` sont des opérations binaires permettant aussi de mélanger les données, mais des informations peuvent être perdues.

Par exemple, pour un octet donné :

value : 00001111
value >> 2 : 00000011

Les deux bits de poids faible `11` de `00001111` ont été perdus (ils ont été décalés hors de l'octet).

De même, pour un décalage vers la gauche :

value : 11110000
value << 2 : 11000000

Les deux bits de poids fort `11` de `11110000` ont été perdus.

Une solution pour éviter cette perte est d'utiliser des rotations circulaires (ROL).

### ROL (Rotation linéaire)

L'opérateur ROL effectue des rotations de bits sans perte d'information, mais il peut être un peu plus coûteux en temps de calcul.

Par exemple :

value : 11110000
ROL(value, 2) : 11000011

La rotation récupère les bits qui auraient été perdus par un simple décalage et les réinjecte à l'autre extrémité.

## Fonction de hachage 

Le but de ces algorithmes est de produire un hachage différent pour des quadruplets très similaires.  
Cependant, deux contraintes principales doivent être prises en compte :

- **La rapidité d'exécution** : sans optimisation matérielle spécifique, il s’agit du nombre de cycles processeur exécutés.  
- **Le taux de collision** : un faible taux de collision indique une meilleure répartition de l’information, autrement dit un mélange plus uniforme dans la table de hachage.

### Alogrithme XOR

```c
hash = src ^ dst ^ ((port_src << 16) | port_dst);
hash *= 0x9E3779B9; // Nombre d'or
```
Cet algorithme offre un nombre de cycles faible et une distribution correcte.

### Alogrithme XOR + ROL

```c
hash = src;
hash = ROL32(hash, 7) ^ dst;
hash = ROL32(hash, 13) ^ ((port_src << 16) | port_dst);
hash *= 0x9E3779B9; // Nombre d'or
hash ^= hash >> 16;
```

Cet algorithme utilise davantage de cycles, mais produit une meilleure distribution des valeurs de hachage.

#### Conclusion 

Globalement, ces deux algorithmes peuvent être considérés comme fonctionnels et permettent d’obtenir un hachage rapide et bien distribué.