# Bedrock Custom Client

Overlay PvP pour **Minecraft Bedrock Edition sur Windows 11** : un menu façon ClickGUI qui s'ouvre
en pleine partie avec `Insert`, un HUD personnalisable (keystrokes, CPS, réticule, ping…), un simulateur
de portée et une carte de tes mondes solo.

Le client est **non intrusif** : c'est une fenêtre transparente posée au-dessus du jeu. Il n'injecte rien
dans Minecraft, ne lit ni n'écrit sa mémoire et ne modifie aucun de ses fichiers.

---

## Sommaire

- [Installation](#installation)
- [Premier lancement](#premier-lancement)
- [Utilisation](#utilisation)
- [Fonctionnalités](#fonctionnalités)
- [Ce que le client peut et ne peut pas savoir](#ce-que-le-client-peut-et-ne-peut-pas-savoir)
- [Dossiers et profils](#dossiers-et-profils)
- [Dépannage](#dépannage)
- [Compiler depuis les sources](#compiler-depuis-les-sources)
- [État de vérification](#état-de-vérification)
- [Licences](#licences)

---

## Installation

Prérequis : Windows 10 ou 11 en 64 bits, Minecraft for Windows (Bedrock, via le Minecraft Launcher ou le Microsoft Store),
une carte graphique compatible Direct3D 11. Aucun redistribuable à installer, aucun droit administrateur requis.

1. Télécharge `BedrockCustomClient-1.0.0-win64.zip`.
2. Extrais-le dans un dossier où tu peux écrire, par exemple `Documents\BedrockCustomClient`
   (évite `C:\Program Files` : le client range ses réglages à côté de l'exécutable).
3. Lance `BedrockCustomClient.exe`.

> **SmartScreen** : l'exécutable n'est pas signé numériquement. Windows peut afficher
> « Windows a protégé votre ordinateur » : clique **Informations complémentaires → Exécuter quand même**.

Désinstallation : quitte le client (icône de la zone de notification → **Quitter**) et supprime son dossier.
Si le dossier n'était pas accessible en écriture, les réglages sont dans `%APPDATA%\BedrockCustomClient`.

---

## Premier lancement

1. Lance Minecraft et entre dans un monde ou sur un serveur.
   Le client peut aussi lancer le jeu : icône de la zone de notification → **Lancer Minecraft**.
2. Lance `BedrockCustomClient.exe`. Une notification rappelle la touche du menu.
3. Appuie sur **`Insert`** pour ouvrir le menu, puis à nouveau sur `Insert` (ou `Échap`, ou le bouton ×) pour le fermer.

Conseil : utilise Minecraft en **fenêtré** ou **fenêtré sans bordure**. En plein écran exclusif, Windows peut
afficher le jeu par-dessus toute autre fenêtre, overlay compris.

Ouvrir le menu donne le focus à l'overlay. Minecraft passe alors en arrière-plan et peut afficher son menu Pause en solo.
À la fermeture, le focus revient au jeu.

---

## Utilisation

| Action | Comment |
|---|---|
| Ouvrir / fermer le menu | `Insert` (modifiable : Settings → Raccourcis) |
| Activer un module | Interrupteur sur sa carte |
| Plus de réglages | « Plus de réglages » en bas de la carte |
| Raccourci d'un module | Ligne « Raccourci » de la carte → appuie sur une touche (`Échap` annule, `Retour arrière` efface) |
| Déplacer le menu | Glisser l'en-tête |
| Déplacer / redimensionner le HUD | Settings → HUD → **Éditer les positions du HUD** : glisser, molette = taille |
| Profils | Page **Configs** |
| Quitter | Icône de la zone de notification → **Quitter**, ou bouton **Quitter** dans Settings |

Les touches sont **observées** (Raw Input) sans jamais être bloquées : Minecraft les reçoit normalement.
Seule la touche du menu (`Insert`) est réservée au client.

---

## Fonctionnalités

### Menu

- Fenêtre unique : barre latérale (Combat, Movement, Render, Player, Carte, Settings, Configs) et cartes de modules.
- Thème sombre bleu nuit, effet verre, accent personnalisable (préréglages ou couleur libre).
- Perspective 3D réglable qui suit la souris, animations d'ouverture et de survol, recherche et filtre « Activés ».

### Modules

| Catégorie | Module | Description |
|---|---|---|
| Combat | **CPS Counter** | Clics par seconde gauche/droite, pic de la session |
| Combat | **Combat HUD** | CPS, ping, portée simulée, rappel « portée serveur inconnue » |
| Combat | **Reach Lab** | Simulateur local de portée (voir ci-dessous) |
| Movement | **Keystrokes** | Touches de déplacement (détection AZERTY/QWERTY automatique), LMB/RMB avec CPS, barre espace |
| Movement | **Sprint Status** | Sprint déduit du clavier (modes Maintenir / Basculer) |
| Movement | **Jump Counter** | Appuis sur la touche Saut en jeu |
| Movement | **Speed** | Affiché « N/D » : donnée non exposée par le jeu |
| Render | **Crosshair** | 5 styles vectoriels ou **ta propre image PNG**, contour, mode dynamique |
| Render | **Info Bar** | Barre compacte CPS · ping · FPS de l'overlay · heure |
| Render | **Ping** | Latence vers ton serveur (par ex. `geo.hivebedrock.network`) |
| Render | **FPS Counter** | FPS de l'**overlay** (celui du jeu n'est pas accessible) |
| Player | **Session Stats** | Durée, temps en jeu, clics, pic de CPS, touches |
| Player | **Player Info** | État et taille de la fenêtre du jeu, liste des données non accessibles |

### Reach Lab (simulateur)

Le Reach Lab est un **laboratoire local** : il calcule, pour une cible simulée (hitbox de joueur 0,6 × 1,8), si un coup
toucherait avec la portée que tu configures. Il affiche une vue de dessus, compare avec la portée vanilla (3,00 blocs),
estime le taux de réussite sur 1000 coups avec une imprécision de visée, et propose 3 presets enregistrables.

Il ne change **pas** la portée dans Minecraft : celle-ci est décidée par le jeu et vérifiée par le serveur.
L'interface distingue toujours (1) la distance affichée, (2) la portée du simulateur et (3) la portée du serveur.

### Carte du monde

- Lit tes **mondes solo** depuis leurs sauvegardes, en lecture seule. Minecraft peut tourner en même temps.
- **Mode découverte** : seules les zones déjà explorées apparaissent, le reste reste dans le brouillard.
- **Monde actuel (auto)** : choisit le monde sauvegardé le plus récemment ; l'actualisation automatique relit la
  sauvegarde toutes les 15 s pendant que tu joues.
- Dimensions **Overworld / Nether / End**, zoom fluide, glisser pour se déplacer, coordonnées sous la souris.
- **Repères** au clic droit, calques (grille, zones, point d'apparition, ta position à la dernière sauvegarde).
- **Extensions** JSON : repères, zones, couleurs des blocs d'add-ons/mods (`docs/EXTENDING.md`).
- **Realm** : télécharge le monde depuis les paramètres du Realm (propriétaire), il apparaît alors dans la liste.

### Profils

Sauvegarde automatique, profils multiples, import/export JSON, profils supprimés récupérables (`configs\.trash`),
fichier corrompu mis de côté automatiquement. Un profil d'exemple est fourni : `configs\examples\example-profile.json`
(Configs → **Importer…**).

---

## Ce que le client peut et ne peut pas savoir

| Donnée | Disponible | Origine |
|---|---|---|
| Touches, clics, CPS | Oui | Ton clavier et ta souris (Raw Input) |
| Fenêtre du jeu (présente, active, taille) | Oui | Fenêtres Windows |
| Ping vers un serveur | Oui, approximatif | Requête publique de liste de serveurs (RakNet) |
| FPS de l'overlay | Oui | Mesuré par le client |
| Carte d'un monde solo | Oui | Sauvegarde du monde (état de la dernière sauvegarde) |
| FPS réel du jeu | Non | Non exposé aux autres applications |
| Position, vitesse, santé, armure, équipement en direct | Non | Seulement dans la mémoire du jeu, que le client ne lit pas |
| Joueurs autour de toi, distance à une cible | Non | Idem |

Ce qui n'est pas accessible est affiché **N/D** avec une explication, jamais inventé.

> Le client ne modifie pas le jeu, mais chaque serveur a ses propres règles sur les logiciels tiers :
> vérifie celles du serveur sur lequel tu joues.

---

## Dossiers et profils

Le client range ses données à côté de `BedrockCustomClient.exe` (ou dans `%APPDATA%\BedrockCustomClient` si ce dossier
est en lecture seule) :

| Dossier | Contenu |
|---|---|
| `configs\profiles\` | Un fichier JSON par profil |
| `configs\.trash\` | Profils supprimés |
| `configs\examples\` | Profil d'exemple |
| `crosshairs\` | Tes images de réticule |
| `map-extensions\` | Extensions de la carte et `waypoints.json` |
| `logs\` | Journal `bcc.log`, rapports de plantage éventuels |
| `docs\` | `EXTENDING.md` : ajouter images, extensions de carte, modules, calques |

---

## Dépannage

| Problème | Solution |
|---|---|
| Rien ne s'affiche en jeu | Vérifie que le client tourne (icône de la zone de notification). Par défaut le HUD ne s'affiche que quand Minecraft est la fenêtre active (Settings → Général → « HUD seulement en jeu »). |
| L'overlay est masqué par le jeu | Passe Minecraft en fenêtré / fenêtré sans bordure. |
| `Insert` n'ouvre pas le menu | Une autre application utilise peut-être la touche : change-la dans Settings → Raccourcis, ou ouvre le menu depuis l'icône de la zone de notification. |
| Le ping reste « — » | Active le module **Ping** et indique l'adresse de ton serveur. Certains serveurs ne répondent pas à la requête publique. |
| La carte est vide | Seuls les mondes **solo** de ce PC sont lisibles. Pour un Realm : télécharge d'abord le monde. Une dimension jamais visitée est vide. |
| Plantage | Envoie `logs\bcc.log` (et le fichier `.dmp` s'il existe). |

Réglage des ressources : Settings → Général → **Limite de FPS de l'overlay** (0 = synchronisé sur l'écran).
Settings → Performance affiche le CPU, la mémoire et le temps par image de l'overlay.

---

## Compiler depuis les sources

Prérequis : Visual Studio 2022 avec **Développement Desktop en C++** (MSVC x64 + SDK Windows), CMake ≥ 3.24
(celui de Visual Studio, ou le zip officiel de Kitware).

```powershell
.\build.ps1                 # Release + tests
.\build.ps1 -Config Debug
.\build.ps1 -Package        # crée dist\BedrockCustomClient-1.0.0-win64.zip
```

`build.ps1` trouve CMake (PATH, `%LOCALAPPDATA%\BedrockCustomClient-tools`, ou celui de Visual Studio) et l'environnement
MSVC. Si le dossier du projet a un chemin très long, la compilation se fait dans `%TEMP%\bcc-build`.

Dépendances incluses dans `external/` : Dear ImGui 1.93.0, nlohmann/json 3.12.0, zlib 1.3.1.

Tests : `bcc_tests.exe` (lancé par `build.ps1`). Variables facultatives : `BCC_TEST_OUTPUT` (dossier des captures
des rendus hors écran), `BCC_TEST_WORLD` (copie d'un monde pour tester la carte).

Autotest de l'application :

```powershell
BedrockCustomClient.exe --selftest 4 --screenshot out.bmp --report rapport.json --data-dir C:\tmp\bcc
```

Autres options : `--open-menu`, `--launch-minecraft`, `--data-dir <dossier>`.

Ajouter des fonctionnalités : voir **`docs/EXTENDING.md`**.

---

## État de vérification

Vérifié automatiquement (41 tests unitaires et de rendu, tous réussis lors de la génération de cette version) :

- Réglages, modules, raccourcis, bus d'événements, profils (corruption, corbeille, import/export, sauvegarde automatique).
- Rendu complet de toutes les pages et de tous les modules, hors écran, avec la perspective 3D.
- CPS, Raw Input (normalisation des touches), requête de ping RakNet, géométrie du Reach Lab.
- Lecteur de sauvegardes : formats synthétiques et copie d'un vrai monde (509 chunks, 100 % des colonnes, ≈0,1 s).
- Détection automatique du monde actuel, profil d'exemple chargé sans avertissement.

Constaté lors d'un premier essai réel : la fenêtre de l'overlay s'affiche bien au-dessus du bureau (DirectComposition).

**Non vérifié en conditions réelles** (à confirmer en jeu) : affichage par-dessus Minecraft dans chaque mode d'affichage,
retour du focus au jeu à la fermeture du menu, comportement de la souris de Minecraft quand le menu s'ouvre,
ping vers un serveur réel.

---

## Licences

- Dear ImGui — MIT (`licenses\imgui-LICENSE.txt`)
- nlohmann/json — MIT (`licenses\nlohmann-json-LICENSE.txt`)
- zlib — licence zlib (`licenses\zlib-LICENSE.txt`)

Minecraft est une marque de Mojang Studios / Microsoft. Ce projet n'est ni affilié ni approuvé par eux.
