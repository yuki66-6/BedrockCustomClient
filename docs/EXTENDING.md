# Étendre Universal Minecraft Client

Ce guide explique comment ajouter du contenu au client **sans toucher au cœur** :

1. [Où sont les dossiers](#1-où-sont-les-dossiers)
2. [Ajouter une image de réticule](#2-ajouter-une-image-de-réticule) — aucune programmation
3. [Étendre la carte du monde](#3-étendre-la-carte-du-monde) — repères, zones, couleurs de blocs de mods (JSON)
4. [Ajouter un module Bedrock](#4-ajouter-un-module-c) — C++
5. [Ajouter un module Java](#5-ajouter-un-module-java-fabric) — Java (Fabric)
6. [Ajouter un calque à la carte](#6-ajouter-un-calque-à-la-carte-c) — C++
7. [Brancher ta propre source de carte](#7-brancher-ta-propre-source-de-carte-c) — C++
8. [Déclarer un module dans les définitions communes](#8-déclarer-un-module-dans-les-définitions-communes)
9. [Règles à respecter](#9-règles-à-respecter)

Les sections 1 à 4, 6 et 7 concernent l'overlay Bedrock (`bedrock/`), la section 5 le mod Java (`java/fabric/`).

---

## 1. Où sont les dossiers

Le client range ses données dans son **dossier de données** :

- à côté de `BedrockCustomClient.exe` si ce dossier est accessible en écriture (mode portable) ;
- sinon dans `%APPDATA%\BedrockCustomClient`.

| Dossier | Contenu |
|---|---|
| `configs\profiles\` | Profils JSON (un fichier par profil) |
| `configs\.trash\` | Profils supprimés (récupérables) |
| `crosshairs\` | Images de réticule personnalisées |
| `map-extensions\` | Extensions de la carte (repères, zones, couleurs de blocs) |
| `waypoints\` | `umc-waypoints.json` : tes waypoints, au format commun avec Java |
| `logs\` | Journal `bcc.log` et éventuels rapports de plantage |

Raccourcis : icône de la zone de notification → **Ouvrir le dossier des profils**, ou les boutons
« Ouvrir le dossier » dans les modules Crosshair et la page Carte.

---

## 2. Ajouter une image de réticule

1. Prépare une image **PNG avec transparence** (JPG, BMP et GIF fonctionnent aussi), idéalement entre 16 et 256 px de côté
   (4096 px maximum).
2. Copie-la dans le dossier `crosshairs\`.
3. Dans le menu : **Render → Crosshair → Plus de réglages → Images personnalisées** :
   clique **Rafraîchir**, puis choisis ton image dans la liste. Le type passe automatiquement sur « Image ».
4. Règle la taille avec **Échelle de l'image**. Le choix est enregistré dans le profil actif.

> Astuce : le réticule de Minecraft reste visible (le client ne modifie pas le jeu). Pour n'afficher que le tien,
> utilise un pack de ressources qui rend le réticule du jeu transparent.

---

## 3. Étendre la carte du monde

La page **Carte** affiche tes mondes **solo** à partir de leurs fichiers de sauvegarde (lecture seule).
Seules les zones déjà générées existent dans la sauvegarde : la carte se dévoile donc au fil de
tes explorations (**mode découverte**).

Tout fichier `*.json` placé dans `map-extensions\` est chargé au démarrage de la page (bouton **Recharger les
extensions** pour le relire sans redémarrer). Un fichier peut contenir trois sections, toutes facultatives :

```json
{
  "name": "Mes repères",
  "markers": [
    { "label": "Base", "x": 120, "z": -340, "color": "#4A8BFF", "icon": "home" },
    { "label": "Portail", "x": -48, "z": 260, "color": "#B04AFF", "icon": "star", "dimension": "overworld" }
  ],
  "areas": [
    { "label": "Village", "x1": 0, "z1": 0, "x2": 96, "z2": 64, "color": "#FFD04060" }
  ],
  "blockColors": {
    "monaddon:ruby_ore": "#D0204A",
    "monaddon:marble": "#EDEDED"
  }
}
```

| Champ | Description |
|---|---|
| `markers[].x`, `z` | Coordonnées en blocs (celles affichées par « Afficher les coordonnées » dans Minecraft) |
| `markers[].icon` | `pin` (défaut), `home`, `star`, `flag`, `skull` |
| `markers[].color`, `areas[].color` | `#RRGGBB` ou `#RRGGBBAA` (AA = opacité) |
| `markers[].dimension` | `overworld` (défaut) — seule dimension affichée pour l'instant |
| `areas[]` | Rectangle entre (`x1`,`z1`) et (`x2`,`z2`) |
| `blockColors` | Couleur d'un bloc par son identifiant complet. Sert aux **blocs d'add-ons / mods** (ou pour changer une couleur vanilla). |

Les waypoints ajoutés depuis la carte (clic droit → **Ajouter un repère ici**) sont enregistrés dans
`waypoints\umc-waypoints.json`, au format commun **umc-waypoints** (`shared/schemas/waypoints.schema.json`),
le même que le mod Java : un fichier exporté d'une édition s'importe dans l'autre. L'ancien
`map-extensions\waypoints.json` est migré automatiquement (renommé en `.migrated`).

---

## 4. Ajouter un module (C++)

Un module, c'est **une classe** plus **une ligne d'enregistrement**. La carte dans le menu, les réglages,
le raccourci clavier, la sauvegarde dans les profils et le HUD déplaçable sont automatiques.

### 4.1 Créer la classe

`bedrock/src/modules/render/HelloModule.h` :

```cpp
#pragma once
#include "modules/Module.h"

namespace bcc {

class HelloModule : public Module {
public:
    HelloModule();
    Icon icon() const override { return Icon::Star; }      // voir gui/Icons.h
    bool hasHud() const override { return true; }           // élément HUD déplaçable
    ImVec2 defaultHudPosition() const override { return {0.5f, 0.1f}; } // fraction de l'écran
    void drawHud(HudContext& ctx) override;

private:
    StringSetting* text_;
    ColorSetting* color_;
};

} // namespace bcc
```

`bedrock/src/modules/render/HelloModule.cpp` :

```cpp
#include "modules/render/HelloModule.h"
#include "modules/HudDraw.h"

namespace bcc {

HelloModule::HelloModule()
    : Module("hello",                       // identifiant unique : a-z, 0-9, _
             "Hello",                       // nom affiché
             "Affiche un texte personnalisé.", // description de la carte
             Category::Render) {            // Combat, Movement, Render ou Player
    text_ = add<StringSetting>("text", "Texte", "Texte affiché", "Salut !");
    color_ = add<ColorSetting>("color", "Couleur", "", Color{1, 1, 1, 1});
}

void HelloModule::drawHud(HudContext&) {
    ImGui::TextColored(hud::col(color_->get()), "%s", text_->get().c_str());
}

} // namespace bcc
```

> Pas d'icône adaptée ? Utilise `Icon::Generic`, ou ajoute un cas dans `bedrock/src/gui/Icons.cpp`.

### 4.2 L'enregistrer

Dans `bedrock/src/modules/ModuleRegistry.cpp` :

```cpp
#include "modules/render/HelloModule.h"
// ...
m.emplace<HelloModule>();
```

C'est tout : `CMakeLists.txt` compile automatiquement tous les `.cpp` de `bedrock/src/`.

### 4.3 Compiler et tester

```bash
powershell -ExecutionPolicy Bypass -File build.ps1
```

Le script compile, lance les tests et indique où se trouve l'exécutable.

### 4.4 Ce qu'un module peut utiliser

| Besoin | Comment |
|---|---|
| Réglages | `add<BoolSetting>`, `IntSetting`, `FloatSetting` (slider), `EnumSetting` (liste), `ColorSetting`, `KeySetting`, `StringSetting`, `ActionSetting` (bouton) |
| Afficher un réglage selon un autre | `setting->setVisibleIf([this]{ return mode_->get() == 1; });` |
| Réglages affichés d'office sur la carte | `int primarySettingCount() const override` (les autres passent sous « Plus de réglages ») |
| Élément HUD | `hasHud()`, `defaultHudPosition()`, `drawHud(HudContext&)` |
| Dessin plein écran (réticule…) | `drawOverlay(OverlayContext&)` (`ctx.drawList`, `ctx.areaMin/areaMax`) |
| Aperçu animé sur la carte du menu | `drawCardPreview()` |
| Contenu spécial dans les réglages | `drawExtraSettings()` |
| Clavier / souris réels | `ctx.telemetry->input()` : `keyDown(vk)`, `mouseDown(...)`, `cps(...)` |
| Événements | dans `onInit(ModuleContext& c)` : `c.bus->subscribe<KeyEvent>(...)` (garder un `Subscription`) |
| État de la fenêtre du jeu | `ctx.telemetry->game()` |
| Ping serveur | `ctx.telemetry->ping()` (module Ping configuré) |
| Images | `ctx.telemetry->textures()->get(chemin)` → `ImGui::GetWindowDrawList()->AddImage(...)` |
| Cycle de vie | `onInit`, `onEnable`, `onDisable`, `onUpdate(now, dt)`, `onShutdown` |
| Donnée indisponible | `limitation()` renvoie l'explication affichée à l'utilisateur ; `hud::unavailable(...)` dans le HUD |

### 4.5 Tester un module

Ajoute un cas dans `tests/` (voir `tests/test_modules.cpp` et `tests/test_full_gui.cpp`) :

```cpp
TEST_CASE("Hello : texte par défaut") {
    HelloModule m;
    CHECK(static_cast<StringSetting*>(m.findSetting("text"))->get() == "Salut !");
}
```

---

## 5. Ajouter un module Java (Fabric)

Même principe que sur Bedrock : une classe, une ligne d'enregistrement. Carte dans le menu, réglages, raccourci,
favori, sauvegarde dans le profil et élément HUD déplaçable sont automatiques.

`java/fabric/src/client/java/umc/client/modules/HelloModule.java` :

```java
package umc.client.modules;

import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.hud.HudContext;
import umc.client.hud.HudText;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.common.settings.Settings;

public final class HelloModule extends Module {
	private final Settings.Text text = add(new Settings.Text("text", "Texte", "Message affiché", "Bonjour !", 32));

	public HelloModule() {
		super("hello", "Hello", "Affiche un message", Category.RENDER, false);
	}

	@Override
	public boolean hasHud() {
		return true;
	}

	@Override
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) {
		// Dessine à l'origine (0, 0) et renvoie la taille : position et échelle sont gérées par le HUD.
		return new HudText(ctx).label("Hello ", text.get()).draw(g);
	}
}
```

Enregistre-le dans `registerModules()` de `umc/client/UmcClient.java` : `m.register(new HelloModule());`.

Ce qu'un module Java peut utiliser :

| Outil | Rôle |
|---|---|
| `Settings.Bool / Int / Float / Choice / Color / Text` | Réglages (même JSON que Bedrock) |
| `onTick(Minecraft)`, `onEnable()`, `onDisable()` | Cycle de vie (tick client, 20 fois par seconde, module actif) |
| `ctx.adapter()` (`GameAdapter`) | Données du jeu sous forme de `DataPoint` : valeur, disponibilité, motif |
| `HudText`, `Draw` | Texte multicolore avec fond, rectangles arrondis, interrupteurs |
| `limitation()` | Limite affichée dans les réglages du module |
| `Umc.waypoints`, `Umc.addWaypointHere(…)` | Waypoints du monde actuel |

Ajoute ensuite son identifiant à `ModuleDefinitions.JAVA_MODULE_IDS` et déclare-le dans les définitions communes
(section 8). `.\gradlew.bat build` vérifie que les définitions et `JAVA_MODULE_IDS` concordent ; au démarrage, le mod
signale dans son journal tout écart avec les modules réellement enregistrés.

---

## 6. Ajouter un calque à la carte (C++)

Les éléments dessinés sur la carte (grille, repères, zones, point d'apparition, joueur) sont des **calques**.
Pour en ajouter un, dérive de `MapLayer` (`bedrock/src/map/MapLayers.h`) :

```cpp
class ChunkGridLayer : public MapLayer {
public:
    const char* id() const override { return "chunk_grid"; }
    const char* name() const override { return "Grille des chunks"; }
    void draw(MapDrawContext& ctx) override {
        // ctx.worldToScreen({x, z}) convertit des coordonnées de blocs en pixels.
        // ctx.drawList, ctx.zoom, ctx.viewMin/viewMax (blocs visibles)…
    }
};
```

Puis enregistre-le dans `registerDefaultMapLayers()` (`bedrock/src/map/MapLayers.cpp`). Chaque calque apparaît
automatiquement dans la liste des calques de la page Carte, avec un interrupteur.

---

## 7. Brancher ta propre source de carte (C++)

La page Carte liste les mondes de chaque **source** (`MapSource`, `bedrock/src/map/MapSource.h`). La source
`CustomMapSource` (`bedrock/src/map/CustomMapSource.h/.cpp`) est un emplacement prévu pour ton code. Elle apparaît dans
la page Carte sous le nom renvoyé par `name()` :

- `listWorlds()` peut renvoyer des dossiers au format Bedrock (`db/` + `level.dat`) rangés ailleurs, par exemple des
  copies de mondes de Realm téléchargés : la carte les lit automatiquement ;
- ou redéfinis `buildMap()` pour construire toi-même un `WorldMapResult` à partir d'autres données.

Toutes les sources sont créées dans `createMapSources()` (`bedrock/src/map/MapSource.cpp`) ; ajoute la tienne à la liste
si tu en écris une autre. Une source ne doit lire que des fichiers que tu possèdes, en lecture seule.

---

## 8. Déclarer un module dans les définitions communes

`shared/module-definitions/modules.json` décrit chaque module pour les deux éditions (schéma
`shared/schemas/module-definition.schema.json`) : identifiant, catégorie, nom, description, capacités nécessaires et,
pour `java` et `bedrock`, un statut :

| Statut | Signification |
|---|---|
| `tested` | Implémenté et vérifié en conditions réelles |
| `implemented` | Implémenté, testé automatiquement |
| `partial` | Implémenté avec des limites (expliquées dans `note`) |
| `planned` | Prévu : affiché comme tel, jamais activable |
| `unsupported` | Impossible dans cette édition sans contourner ses protections : non affiché |

`providedBy` indique quand la fonction est fournie par autre chose qu'un module (`map_page`, `launcher`). Les tests des
deux éditions vérifient que les modules réellement enregistrés correspondent à ces statuts.

---

## 9. Règles à respecter

Le client est conçu pour rester **non intrusif** et honnête :

- **Jamais** d'injection, de lecture ou d'écriture de la mémoire de Minecraft, ni de modification de ses fichiers.
- La carte lit les sauvegardes en **lecture seule**. Elle n'écrit jamais dans un dossier de monde.
- **Pas** de fonction qui donne un avantage en multijoueur (portée, radar de joueurs, ESP…). Reach Lab reste un
  simulateur local.
- Une donnée que le jeu n'expose pas doit être affichée comme **N/D** / indisponible avec une explication
  (`limitation()`, `DataPoint.reason`), jamais inventée.
- Sur Java : pas de module qui agit à la place du joueur ou donne une information que le serveur ne lui donne pas.
  Les commandes (`/tp`…) passent par le serveur, qui décide.
- Aucun envoi de données sur Internet sans l'accord explicite de l'utilisateur.
