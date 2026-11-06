# Diagramme de Voronoï

Calcul du diagramme de Voronoï 2D d'un nuage de points (cours 5), par **intersection de demi-plans**.

Ce dossier est **indépendant du reste du projet** : il n'appelle ni `ConvexHullAlgorithms`, ni `DelaunayTriangulation`, ni `GeometryMathLibrary`, et ne modifie aucun de ces fichiers. Il peut donc être testé tout de suite, sans attendre la triangulation de Delaunay, et servira ensuite de vérification pour la version construite par dualité.

| Fichier | Contenu |
|---|---|
| `VoronoiDiagram.h` | Structures `FVoronoiCell`, `FVoronoiEdge` et fonctions exposées en Blueprint |
| `VoronoiDiagram.cpp` | L'algorithme. Les fonctions internes sont dans l'espace de noms `VoronoiHalfPlanes` |

## Le principe

On note *P*₁, …, *Pₙ* les sites (les points d'entrée).

**Région d'un site.** La région *Rᵢ* de *Pᵢ* est l'ensemble des points du plan dont *Pᵢ* est le site le plus proche :

```
Rᵢ = { X : ‖X − Pᵢ‖ ≤ ‖X − Pⱼ‖ pour tout j ≠ i }
```

**Un demi-plan par voisin.** La condition « plus près de *Pᵢ* que de *Pⱼ* » définit un demi-plan *Eᵢⱼ*. En élevant les distances au carré (ce qui ne change pas l'ordre, elles sont positives), le terme ‖X‖² s'annule des deux côtés et il reste une inégalité du premier degré :

```
‖X − Pᵢ‖² ≤ ‖X − Pⱼ‖²   ⟺   2 (Pⱼ − Pᵢ) · X ≤ ‖Pⱼ‖² − ‖Pᵢ‖²
```

Sa frontière est la **médiatrice** de [*PᵢPⱼ*].

**La région est leur intersection.** *Rᵢ* = ⋂ *Eᵢⱼ* pour *j* ≠ *i*. Une intersection de demi-plans est toujours un polygone convexe. Si *Pᵢ* est sur le bord de l'enveloppe convexe, ce polygone est infini.

## L'algorithme

Pour chaque site *Pᵢ* :

1. Partir d'un **cadre** rectangulaire qui contient tous les sites (il coupe les régions infinies).
2. Trier les autres sites du plus proche au plus loin.
3. Couper le polygone par chaque demi-plan *Eᵢⱼ* dans cet ordre (découpe de Sutherland-Hodgman par une droite).
4. **Arrêt anticipé** : si tout le polygone tient dans le disque de centre *Pᵢ* et de rayon ‖*Pⱼ* − *Pᵢ*‖ / 2, il est déjà entièrement dans *Eᵢⱼ*, et aussi dans les demi-plans de tous les sites plus lointains. On s'arrête.

La découpe utilise la distance signée à la médiatrice, plus stable que la forme développée :

```
f(X) = n · (X − M),   n = (Pⱼ − Pᵢ) / ‖Pⱼ − Pᵢ‖,   M = (Pᵢ + Pⱼ) / 2
```

*X* est gardé si f(*X*) ≤ ε. Quand une arête du polygone traverse la médiatrice, on ajoute le point d'intersection *A* + *t* (*B* − *A*), avec *t* = f(*A*) / (f(*A*) − f(*B*)).

**Voisins.** Chaque arête du polygone retient le site qui l'a créée. On obtient ainsi les arêtes du diagramme (frontière entre *Rᵢ* et *Rⱼ*) sans calcul supplémentaire, et les morceaux du cadre sont marqués `INDEX_NONE`.

### Complexité

Au pire O(*n*²) découpes par site, donc O(*n*³) au total. L'arrêt anticipé réduit beaucoup ce coût en pratique (seuls les voisins proches découpent vraiment) : environ 4 ms pour 200 points et 0,1 s pour 1 000 points, mesuré hors Unreal. La courbe du banc d'essai donne le temps réel dans la scène. La version par dualité avec Delaunay (O(*n* log *n*) avec un bon Delaunay) reste la cible pour de gros nuages.

### Cas particuliers

| Cas | Comportement |
|---|---|
| Doublons exacts | Une seule région, recopiée pour chaque exemplaire (`DuplicateOf` indique le premier) |
| Tous les sites alignés | Des bandes séparées par des médiatrices parallèles, sans aucun sommet |
| Quatre sites ou plus sur un même cercle | Les sommets confondus sont fusionnés : pas d'arête de longueur nulle |
| 1 ou 2 sites | 1 site : tout le cadre. 2 sites : deux régions séparées par la médiatrice |

### Tolérances

Toutes relatives à la taille du cadre (*s*), pour que le même code marche à 6 cm comme à 60 m :

- ε = 10⁻⁹ *s* : un point « sur » la médiatrice compte comme dedans ;
- 10⁻⁶ *s* : deux sommets ou deux sites plus proches sont confondus.

## Utilisation

```cpp
#include "VoronoiDiagram.h"

TArray<FVoronoiCell> Cells;
FVector BoundsMin, BoundsMax;
UVoronoiDiagram::ComputeVoronoiHalfPlanes(Points, /*BoundsMargin*/ 0.0, Cells, BoundsMin, BoundsMax);

TArray<FVoronoiEdge> Edges = UVoronoiDiagram::ExtractVoronoiEdges(Cells);
TArray<FVector> Vertices = UVoronoiDiagram::ExtractVoronoiVertices(Cells, BoundsMin, BoundsMax);
```

Les trois fonctions sont aussi disponibles en Blueprint, catégorie *Geometry › Voronoi*.

| Fonction | Rôle |
|---|---|
| `ComputeVoronoiHalfPlanes(Sites, BoundsMargin, OutCells, OutBoundsMin, OutBoundsMax)` | Une région par site, dans l'ordre d'entrée. `BoundsMargin` ≤ 0 : marge automatique, max(moitié de la plus grande dimension du nuage, 100 cm) |
| `ExtractVoronoiEdges(Cells)` | Les arêtes du diagramme, une fois chacune, avec les deux sites qu'elles séparent |
| `ExtractVoronoiVertices(Cells, BoundsMin, BoundsMax, Tolerance)` | Les sommets (points où se rejoignent au moins trois régions), sans ceux du cadre |
| `ComputeVoronoiCellSteps(Sites, SiteIndex, BoundsMargin, OutSteps, OutSkippedSites, ...)` | La construction d'**une** région, étape par étape : le cadre, puis une étape par médiatrice essayée (polygone obtenu, partie retirée, a-t-elle coupé). La dernière étape est exactement la région de `ComputeVoronoiHalfPlanes` : les deux passent par le même code |

| Champ de `FVoronoiCell` | Sens |
|---|---|
| `SiteIndex`, `Site` | Indice et position du site |
| `Polygon` | Sommets de la région, sens trigonométrique, Z = 0 |
| `EdgeNeighbors[k]` | Site de l'autre côté de l'arête `Polygon[k] → Polygon[k + 1]`, ou `INDEX_NONE` pour le cadre |
| `bTouchesBounds` | La région touche le cadre : elle est infinie (ou le cadre est trop petit) |
| `DuplicateOf` | Indice du premier exemplaire si ce site est un doublon, sinon `INDEX_NONE` |

## Le voir et le tester

Dans la scène de test de l'UI (voir le README principal), bouton **Voronoï** ou touche `5`.

L'onglet **Vérifications** contrôle la sortie réelle du C++ :

- une région par site distinct ;
- chaque région est convexe, en sens trigonométrique, et contient son site ;
- **la définition elle-même** : chaque sommet et chaque centre de gravité de *Rᵢ* a bien *Pᵢ* comme site le plus proche ;
- les régions pavent exactement le cadre (somme des aires = aire du cadre) ;
- mêmes arêtes que la référence JavaScript (`reference.js`, écrite à part) ;
- **dualité avec Delaunay** : un sommet par triangle de Delaunay dont le centre du cercle circonscrit tombe dans le cadre, et une arête par arête de Delaunay quand tous ces centres y tombent. À titre indicatif si des sites sont cocycliques.

Le banc d'essai passe les 14 cas fixes (dont les cas limites : alignés, doublons, cocycliques, nuage de 6 cm). Dans l'onglet **Calques**, *Triangulation duale* superpose la triangulation de Delaunay de référence pour voir la dualité, et *Cercles circonscrits* montre que chaque sommet du diagramme est le centre d'un cercle vide.

Dans le niveau :

- **survoler un site** met sa région en valeur et relie ses voisins en pointillés : ce sont les arêtes de Delaunay ;
- **survoler un sommet** affiche son cercle vide, qui passe par les sites à égale distance (au moins trois) ;
- **construction pas à pas** (onglet *Scène*) : cliquer un point, puis avancer étape par étape ou lancer la lecture. La médiatrice testée est en pointillés rouges, la partie retirée en rose, la région en cours en turquoise. L'option *Sauter les médiatrices qui ne coupent rien* ne garde que les découpes utiles ; à la fin, le panneau indique combien de sites l'arrêt anticipé a évité d'essayer.

## Suite possible

Quand `IncrementalTriangulation2D` et `FlipEdgesToDelaunay` fonctionneront, une seconde version pourra construire le diagramme par dualité (cours 5, p. 41), en **appelant** ces fonctions sans les modifier :

- chaque triangle donne un sommet : le centre de son cercle circonscrit ;
- chaque arête intérieure donne un segment entre les centres de ses deux triangles ;
- chaque arête du bord donne une demi-droite, sur la médiatrice, vers l'extérieur.

Les deux versions pourront alors se vérifier l'une l'autre.
