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

Toutes les fonctions sont exposées en Blueprint (`UBlueprintFunctionLibrary`), catégorie *Geometry*.

Les dossiers `Variant_Strategy/` et `Variant_TwinStick/` viennent du template Top Down.

## Contribuer

- Ne jamais pousser directement sur `main` : une branche par sujet, puis une pull request.
- Messages de commit en français au format [Conventional Commits](https://www.conventionalcommits.org/fr/) : `feat: …`, `fix: …`, `docs: …`, `ci: …`, `chore: …`.
- Les PR sont fusionnées en **squash and merge** ; le titre de la PR devient le message du commit sur `main`, il doit donc respecter le même format.
- La CI (hygiène du dépôt, scan de secrets gitleaks, revue des dépendances) doit passer avant la fusion. Elle ne compile pas le projet Unreal : vérifier que ça compile en local avant d'ouvrir la PR.
