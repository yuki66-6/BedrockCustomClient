# Universal Minecraft Client

Client PvP et survie modulaire pour **Minecraft Java** (mod Fabric) et **Minecraft Bedrock** (overlay Windows),
avec des **profils et des waypoints communs** aux deux éditions.

| Édition | Forme | Ce qu'il voit du jeu |
|---|---|---|
| **Java 26.2** | Mod Fabric (`umc-fabric-1.1.0.jar`) : menu, HUD, modules dans le jeu | Les vraies données du jeu via l'API du client (position, santé, effets, FPS, ping…) |
| **Bedrock (Windows)** | Overlay transparent (`BedrockCustomClient.exe`) | Uniquement clavier, souris, fenêtre, ping public et sauvegardes solo : rien n'est lu dans la mémoire du jeu |

Règles communes : aucun add-on à installer sur un serveur ou un Realm, aucun droit d'administrateur, aucun contournement
d'anti-cheat, aucune donnée envoyée sur Internet, aucune modification automatique d'une installation Minecraft.
Une donnée qui n'est pas disponible est affichée **indisponible**, avec son motif, jamais simulée.

---

## Sommaire

- [Édition Java (Fabric)](#édition-java-fabric)
- [Édition Bedrock (overlay)](#édition-bedrock-overlay)
- [Profils et waypoints communs](#profils-et-waypoints-communs)
- [Capacités : ce que chaque édition sait](#capacités--ce-que-chaque-édition-sait)
- [Compiler depuis les sources](#compiler-depuis-les-sources)
- [État de vérification](#état-de-vérification)
- [Licences](#licences)

---

## Édition Java (Fabric)

### Installation

Prérequis : Minecraft Java **26.2**, [Fabric Loader](https://fabricmc.net/use/) ≥ 0.19.3, **Fabric API** pour 26.2, Java 25
(fourni par le launcher officiel).

1. Installe Fabric Loader pour 26.2 avec l'installeur officiel de Fabric.
2. Copie `umc-fabric-1.1.0.jar` et le jar de Fabric API dans le dossier `mods` de ton profil
   (`%APPDATA%\.minecraft\mods` par défaut).
3. Lance le profil Fabric depuis le launcher.

Rien d'autre n'est modifié : supprimer le jar du dossier `mods` désinstalle le client. Les réglages restent dans
`.minecraft\config\umc` (supprime ce dossier pour tout effacer).

### Utilisation

| Action | Touche / geste |
|---|---|
| Ouvrir / fermer le menu | `Maj droite` (modifiable dans Options → Commandes → Universal Minecraft Client) |
| Zoom | Maintenir `C` |
| Waypoint à ta position | `B` |
| Activer un module | Interrupteur de sa carte, ou clic droit sur la carte |
| Réglages d'un module | Clic sur la carte |
| Raccourci d'un module | Réglages du module → **Raccourci** → appuie sur une touche (`Échap` annule, `Retour arrière` efface) |
| Déplacer / redimensionner le menu | Glisser l'en-tête / la poignée en bas à droite |
| Éditer le HUD | Bouton **Éditer le HUD** : glisser, molette = taille, clic droit = position par défaut |
| Importer un profil ou des waypoints | Glisser le fichier `.json` sur la fenêtre du jeu, menu ouvert |

Le menu comprend : **modules par catégorie**, recherche, favoris (☆), réglages (interrupteurs, curseurs, listes, éditeur
de couleur, texte), pages **Waypoints**, **Profils**, **Capacités** et **Réglages** (thème, HUD, sauvegarde).

### Modules Java (18)

| Catégorie | Module | Ce qu'il fait |
|---|---|---|
| Combat | **CPS Counter** | Clics gauche / droit par seconde (chaque appui réel), record de la session |
| Combat | **Totem Counter** | Totems d'immortalité dans l'inventaire, alerte sous un seuil |
| Mouvement | **Keystrokes** | Touches de déplacement avec tes vrais raccourcis (AZERTY compris), clics, CPS, barre d'espace |
| Mouvement | **ToggleSprint** | Sprint en bascule ou permanent (équivaut à maintenir la touche) |
| Mouvement | **ToggleSneak** | Accroupi en bascule |
| Mouvement | **Speed Meter** | Vitesse réelle (blocs/s ou km/h), vitesse verticale |
| Mouvement | **Coordinates HUD** | X Y Z, équivalent Nether / Overworld, chunk, dimension |
| Mouvement | **Direction HUD** | Point cardinal, axe, angle |
| Rendu | **Custom Crosshair** | 5 formes, couleur, taille, écart, épaisseur, contour |
| Rendu | **Zoom** | Facteur réglable, transition douce, caméra cinématique optionnelle |
| Rendu | **FPS Counter** | FPS réel du jeu, coloré selon la valeur |
| Rendu | **Ping** | Latence indiquée par le serveur (tableau des joueurs) |
| Rendu | **Info Bar** | FPS · ping · position · heure sur une ligne |
| Rendu | **Armor HUD** | Armure et objets en main, durabilité (%, restante) |
| Rendu | **Potion Effects HUD** | Effets actifs, niveau, durée, couleur de la potion |
| Rendu | **Time HUD** | Heure du jeu, jour, heure réelle |
| Utilitaires | **Waypoints** | Waypoints du monde et de la dimension actuels, distance, flèche de direction |
| Joueur | **Death Waypoint** | Waypoint automatique à chaque mort (les N dernières sont gardées) |

Les modules **prévus** (Reach Lab Java, Combat HUD, Combo Counter, calculateur de dégâts des cristaux, page Performance,
gestion des shaders Iris…) sont listés comme tels dans le menu et ne sont pas activables.

Limites connues, aussi affichées dans le menu : les waypoints n'ont pas encore de balise 3D dans le monde ; le réticule
personnalisé remplace aussi l'indicateur d'attaque vanilla et n'accepte pas encore d'image ; le zoom ne réduit pas la
sensibilité de la souris ; ToggleSprint / ToggleSneak se désactivent si l'option vanilla « basculer » correspondante est active.

### Téléportation

La page Waypoints propose **/tp** uniquement quand le serveur t'accorde la permission des commandes (monde solo avec triche,
opérateur). Le bouton envoie la commande normale `/tp`, que le serveur accepte ou refuse : rien n'est forcé côté client.

---

## Édition Bedrock (overlay)

Overlay transparent posé au-dessus de Minecraft for Windows : il n'injecte rien dans le jeu, ne lit ni n'écrit sa mémoire
et ne modifie aucun de ses fichiers.

### Installation

Prérequis : Windows 10 ou 11 64 bits, Minecraft for Windows (Bedrock), carte graphique Direct3D 11. Aucun droit administrateur.

1. Extrais `BedrockCustomClient-1.1.0-win64.zip` dans un dossier où tu peux écrire (par exemple `Documents\BedrockCustomClient`).
2. Lance `BedrockCustomClient.exe`, puis `Maj droite` pour ouvrir le menu (modifiable : Settings → Raccourcis).
   La touche n'agit que quand Minecraft ou le menu est au premier plan, et elle arrive toujours au jeu.

> **SmartScreen** : l'exécutable n'est pas signé. Si Windows affiche « Windows a protégé votre ordinateur », clique
> **Informations complémentaires → Exécuter quand même**.

Utilise Minecraft en **fenêtré** ou **fenêtré sans bordure** : en plein écran exclusif, le jeu peut passer au-dessus de l'overlay.
Désinstallation : icône de la zone de notification → **Quitter**, puis supprime le dossier.

### Pack de ressources « UMC PvP UI » (dans le jeu)

Seule façon de changer l'interface **à l'intérieur** de Minecraft Bedrock sans toucher au jeu : un pack de ressources.
Il marche en solo, sur les serveurs et sur les Realms (pack global), sans rien installer côté serveur.

- **9 réticules** au choix : croix vanilla, croix avec écart, croix écartée + point, point, petit plus, T, carré, cercle.
- **Hotbar sombre** translucide et **case sélectionnée bleue** (couleur du thème).

Installation : double-clique sur `UMC-PvP-UI-1.1.0.mcpack` (Minecraft l'importe), puis **Paramètres → Ressources globales**,
active « UMC PvP UI ». Pour changer de réticule : clique sur le pack activé → icône **engrenage** → choisis le réticule.

Un pack ne peut afficher que des images et l'interface du jeu : pas de CPS, de ping, de keystrokes ni de coordonnées
supplémentaires (pour les coordonnées, active l'option du jeu « Afficher les coordonnées »).
Généré par `python tools/make_resource_pack.py` (images dessinées par le script, aucune texture de Mojang).

### Fonctionnalités

- Menu ClickGUI à perspective 3D, thème personnalisable, recherche, raccourcis par module, éditeur de HUD.
- Modules : CPS Counter, Combat HUD, Reach Lab (simulateur local), Keystrokes, Sprint Status, Jump Counter, Crosshair
  (formes ou **image PNG**), Info Bar, Ping (requête publique RakNet), FPS (de l'overlay), Session Stats, Player Info.
- **Reach Lab** : simulateur local de portée. Il ne change **pas** la portée en jeu, décidée par le jeu et vérifiée par le serveur.
- **Carte des mondes solo** lue dans les sauvegardes (lecture seule) : mode découverte, Overworld / Nether / End,
  waypoints, extensions JSON (`docs/EXTENDING.md`). Un Realm doit d'abord être téléchargé par son propriétaire.
- Page **Capacités** : ce que l'overlay peut savoir, et pourquoi le reste est indisponible.

Les données comme la position, la santé ou l'inventaire en direct ne sont pas accessibles sans lire la mémoire du jeu :
les modules qui en dépendent (coordonnées, armure, effets, zoom…) sont **non pris en charge** sur Bedrock et n'apparaissent pas.

---

## Profils et waypoints communs

Les deux éditions utilisent le même format de profil (**umc-profile v2**, `shared/schemas/profile.schema.json`) :
sections communes `modules`, `theme`, `hud`, sections propres `java/…` et `bedrock/…`. Chaque édition ignore sans
avertissement les modules qu'elle ne possède pas. Les anciens profils Bedrock v1 sont migrés automatiquement (copie dans
`profiles\.backup-v1`).

Profils prédéfinis : BedWars, Crystal PvP (entraînement), Survie, Exploration, Construction, Performance.
Appliquer un profil prédéfini crée un profil personnel modifiable.

| | Java | Bedrock |
|---|---|---|
| Profils | `.minecraft\config\umc\profiles\` | `configs\profiles\` à côté de l'exécutable |
| Profils supprimés | `config\umc\.trash\` | `configs\.trash\` |
| Waypoints (`umc-waypoints`) | `config\umc\waypoints\umc-waypoints.json` | `waypoints\umc-waypoints.json` |
| Exports | `config\umc\exports\` | Page Configs → Exporter |

Clés de monde des waypoints : `java:sp:<dossier>`, `java:mp:<adresse>`, `java:realm:<nom>`, `bedrock:sp:<dossier>`.
Un fichier exporté d'une édition s'importe dans l'autre.

---

## Capacités : ce que chaque édition sait

Chaque donnée exposée par un adaptateur (`GameAdapter`) porte sa valeur, sa disponibilité, sa provenance, son horodatage
et sa durée de validité (`shared/schemas/datapoint.schema.json`). Liste des capacités : `shared/schemas/capabilities.json`.

| Donnée | Java (mod) | Bedrock (overlay) |
|---|---|---|
| Clavier, souris, CPS | Oui | Oui (Raw Input, observé sans blocage) |
| FPS du jeu | Oui | Non (FPS de l'overlay seulement) |
| Ping | Oui (serveur) | Approximatif (requête publique) |
| Position, rotation, vitesse | Oui | Non |
| Santé, faim, armure, inventaire, effets | Oui | Non |
| Dimension, identité du monde | Oui | Monde solo via les sauvegardes |
| Seed | Solo uniquement | Non |
| Carte depuis les sauvegardes | Prévu | Oui (mondes solo) |
| Commandes de chat | Selon les permissions du serveur | Non |

> Chaque serveur a ses propres règles sur les logiciels tiers : vérifie celles du serveur sur lequel tu joues.

---

## Compiler depuis les sources

Arborescence : `bedrock/` (C++), `java/fabric/` (mod), `shared/` (schémas, définitions de modules, profils, thèmes,
traductions), `external/` (dépendances C++), `docs/`.

### Bedrock (C++)

Prérequis : Visual Studio 2022 avec **Développement Desktop en C++**, CMake ≥ 3.24.

```powershell
.\build.ps1                 # Release + tests
.\build.ps1 -Package        # dist\BedrockCustomClient-1.1.0-win64.zip
```

Dépendances incluses : Dear ImGui 1.93.0, nlohmann/json 3.12.0, zlib 1.3.1. Variables de test facultatives :
`BCC_TEST_OUTPUT` (captures des rendus hors écran), `BCC_TEST_WORLD` (copie d'un monde pour tester la carte).

### Java (Fabric)

Prérequis : JDK 25. Le wrapper Gradle télécharge Gradle 9.8.2 (somme SHA-256 vérifiée) puis Loom, Minecraft et Fabric API.

```powershell
cd java\fabric
.\gradlew.bat build         # jar dans build\libs + tests
.\gradlew.bat runClient     # lance un Minecraft de développement avec le mod
```

L'intégration continue (`.github/workflows/ci.yml`) compile et teste les deux éditions à chaque push.
Ajouter des modules ou des extensions : **`docs/EXTENDING.md`**.

---

## État de vérification

Vérifié automatiquement lors de la génération de cette version :

- **Bedrock : 48 tests** C++ réussis : réglages, modules, profils v1 → v2, profils prédéfinis, waypoints, capacités
  (cohérence avec `shared/`), rendu hors écran de toutes les pages, lecteur de sauvegardes (copie d'un vrai monde :
  509 chunks, 100 % des colonnes).
- **Java : 11 tests** réussis sur la logique commune (profils, migration, waypoints, capacités, définitions de modules,
  profils prédéfinis). Le mod complet **compile** contre Minecraft 26.2 et Fabric API, mixins compris.

**Non vérifié en conditions réelles** :

- Pack de ressources Bedrock : contenu vérifié automatiquement (manifeste, tailles des images identiques à celles du jeu,
  sous-packs), mais **pas encore importé dans Minecraft**.
- Java : le mod n'a **pas encore été lancé dans Minecraft**. Menu, HUD, modules, mixins (zoom, comptage des clics) et
  commandes /tp sont à confirmer en jeu.
- Bedrock : affichage par-dessus Minecraft dans chaque mode d'affichage, retour du focus au jeu, ping vers un serveur réel.

---

## Licences

- Dear ImGui — MIT · nlohmann/json — MIT · zlib — licence zlib (copiées dans `licenses\` du paquet Bedrock).
- Fichiers repris du modèle de mod Fabric (wrapper Gradle, icône) : CC0, voir `java/fabric/LICENSE-fabric-example-mod.txt`.

Minecraft est une marque de Mojang Studios / Microsoft. Ce projet n'est ni affilié ni approuvé par eux.
