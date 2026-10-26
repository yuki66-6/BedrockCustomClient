#pragma once

namespace bcc {

class ModuleManager;

// Enregistre tous les modules du client. Pour ajouter un module : créer sa
// classe (voir docs/EXTENDING.md) puis l'ajouter ici — rien d'autre à modifier.
void registerAllModules(ModuleManager& modules);

} // namespace bcc
