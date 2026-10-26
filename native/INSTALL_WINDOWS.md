# Installer et lancer BedrockCustomClient sur Windows

## Recupérer le depot
Aucun besoin d'installer « tout GitHub » :
1. Ouvrir https://github.com/yuki66-6/BedrockCustomClient/tree/feature/bedrock-native-client
2. **Code > Download ZIP**, puis extraire le dossier; ou installer Git et lancer :
   `git clone -b feature/bedrock-native-client https://github.com/yuki66-6/BedrockCustomClient.git`.

## Construire et lancer le launcher natif
Installer Visual Studio 2022 avec **Desktop development with C++**, Windows SDK et CMake.

Dans un terminal développeur PowerShell à la racine du dépôt :
```powershell
cmake -S native -B native/build -A x64
cmake --build native/build --config Release
.\native\build\Release\BedrockLauncher.exe
```
Le bouton ouvre Minecraft via `minecraft://`. Le launcher ne charge pas automatiquement la DLL dans Minecraft.

## Construire l'ancien client overlay
```powershell
.\build.ps1
```
Ce client overlay reste le produit existant ; ne pas le confondre avec le futur client natif.

## Correctifs de carte / combat
Les archives des phases précédentes contiennent des changements qui doivent être **fusionnés et validés** avant publication dans cette branche. Les fichiers du dossier `native/` ne contiennent pas à ce stade AutoTotem actif, AutoCrystal actif, Aim Assist en jeu, télémetrie Minecraft ni carte live. Les correctifs de l'ancien overlay ne deviennent pas fonctionnels dans Minecraft simplement parce qu'ils sont compilés.

Ne pas supprimer `src/`, `external/` ou `CMakeLists.txt` tant que la migration n'est pas testée.
