# projet-maths

Projet de géométrie algorithmique sous Unreal Engine 5.7 : enveloppes convexes (marche de Jarvis, Graham scan), triangulation incrémentale 2D et triangulation de Delaunay (flips d'arêtes, ajout et suppression de points).

## Prérequis

- **Unreal Engine 5.7** (Epic Games Launcher)
- **Visual Studio 2022** avec les composants listés dans `.vsconfig`.
  Dans le Visual Studio Installer : *Plus* → *Importer la configuration* → choisir `.vsconfig`.

## Démarrage

Le fichier `Maths_T1.sln` n'est **pas versionné** : il est généré par Unreal à partir du `.uproject`, et chacun le régénère sur sa machine.

1. Cloner le dépôt.
2. Clic droit sur `Maths_T1.uproject` → **Generate Visual Studio project files**.
   Si l'option n'apparaît pas (Windows 11 : *Afficher d'autres options*), lancer depuis un terminal :
   ```bat
   "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -projectfiles -project="%CD%\Maths_T1.uproject" -game -rocket -progress
   ```
3. Ouvrir `Maths_T1.sln`, configuration **Development Editor**, plateforme **Win64**, puis compiler (`Ctrl+Maj+B`).
4. Ouvrir `Maths_T1.uproject` (ou lancer depuis Visual Studio avec `F5`).

À refaire après chaque pull qui ajoute, renomme ou supprime des fichiers `.h`/`.cpp`.

Si l'éditeur refuse de s'ouvrir après un pull (« modules missing or built with a different engine version »), supprimer `Binaries/` et `Intermediate/`, puis régénérer et recompiler.

## Structure du code

Tout le code des algorithmes est dans `Source/Maths_T1/` :

| Fichier | Contenu |
|---|---|
| `GeometryTypes.h` | Structures : liste circulaire (Graham), sommets, arêtes, faces du graphe d'incidence |
| `GeometryMathLibrary.*` | Primitives : orientation, déterminant, cercle circonscrit, critère de Delaunay |
| `ConvexHullAlgorithms.*` | Marche de Jarvis, Graham scan |
| `DelaunayTriangulation.*` | Triangulation incrémentale, flips, noyau de Delaunay (ajout / suppression) |
| `GeomUI/` | Scène de test de l'UI (voir plus bas), sans lien avec les algorithmes |

Toutes les fonctions sont exposées en Blueprint (`UBlueprintFunctionLibrary`), catégorie *Geometry*.

Les dossiers `Variant_Strategy/` et `Variant_TwinStick/` viennent du template Top Down.

## Scène de test de l'UI

Une scène dédiée pour essayer le panneau web et voir la sortie des algorithmes dans le niveau. Elle **appelle** les fonctions de `ConvexHullAlgorithms` et `DelaunayTriangulation` telles qu'elles sont, sans rien y changer.

### Mise en place (une seule fois)

1. Compiler le projet (le module dépend maintenant de `WebBrowser`).
2. Dans l'éditeur : *File → New Level → Empty Level*, enregistrer sous `Content/Tests/LVL_TestUI`.
3. *Window → World Settings → GameMode Override* : choisir **GeomTestGameMode**. Enregistrer.
4. *Play* (PIE). Le panneau apparaît à gauche, un nuage de 30 points est créé.

Le niveau ne contient rien : `AGeomTestGameMode` crée la caméra, le visualiseur et le panneau.

### Ce qu'on peut tester

- Les 4 fonctions (touches `1` à `4`) sur des nuages types : disque, gaussien, amas, cercle, grille (points cocycliques), alignés.
- Ajouter, déplacer, supprimer des points à la souris ; le résultat se recalcule à chaque modification.
- Le **mode noyau** (Delaunay) : les ajouts et suppressions passent par `DelaunayKernel_AddPoint` / `DelaunayKernel_RemovePoint`.
- L'onglet **Vérifications** teste la sortie réelle : polygone convexe en sens trigonométrique, tous les points dedans, Euler, pavage de l'enveloppe, pointeurs du graphe d'incidence, critère de Delaunay, comparaison avec une référence JS. Une sortie vide est signalée comme « fonction encore à écrire ».
- Calques : référence JS en gris, cercles circonscrits, Voronoï.

### Fichiers

| Fichier | Rôle |
|---|---|
| `Source/Maths_T1/GeomUI/GeomTestGameMode.*` | Crée tout, affiche le panneau (`SWebBrowser`) dans un rectangle à gauche |
| `Source/Maths_T1/GeomUI/GeomVisualizer.*` | Garde les points, appelle les algorithmes, dessine les résultats (DrawDebug) |
| `Source/Maths_T1/GeomUI/GeomTestPawn.*` | Caméra orthographique, X à droite et Y en haut |
| `Source/Maths_T1/GeomUI/GeomTestPlayerController.*` | Souris et clavier dans le niveau |
| `Source/Maths_T1/GeomUI/GeomBridge.*` | Pont JS ↔ C++ (`window.ue.geom`) |
| `Content/UI/Web/` | Le panneau : `index.html`, `style.css`, `app.js`, `reference.js`, `sim.js` |

### Travailler sur la page sans Unreal

Ouvrir `Content/UI/Web/index.html` dans Chrome : `sim.js` remplace le C++ et simule le niveau, avec les outils de développement habituels. `index.html#vide` simule des fonctions pas encore écrites.

### Limites connues

- Le dessin utilise `DrawDebug*` : rien ne s'affiche dans un build *Shipping*. Tester en PIE ou en *Development*.
- Les fonctions renvoient seulement leur résultat final : pas de lecture pas à pas dans Unreal.

## Contribuer

- Ne jamais pousser directement sur `main` : une branche par sujet, puis une pull request.
- Messages de commit en français au format [Conventional Commits](https://www.conventionalcommits.org/fr/) : `feat: …`, `fix: …`, `docs: …`, `ci: …`, `chore: …`.
- Les PR sont fusionnées en **squash and merge** ; le titre de la PR devient le message du commit sur `main`, il doit donc respecter le même format.
- La CI (hygiène du dépôt, scan de secrets gitleaks, revue des dépendances) doit passer avant la fusion. Elle ne compile pas le projet Unreal : vérifier que ça compile en local avant d'ouvrir la PR.
