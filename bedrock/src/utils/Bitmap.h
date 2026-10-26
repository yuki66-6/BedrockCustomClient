#pragma once

#include <cstdint>
#include <filesystem>

namespace bcc {

// Écrit une image BGRA à alpha prémultiplié dans un BMP 24 bits, composée sur
// un fond gris uni (permet de visualiser la transparence de l'overlay).
// Retourne false en cas d'échec d'écriture. `outOpaque` reçoit le nombre de
// pixels non transparents.
bool writePremultipliedBmp(const std::filesystem::path& file, const uint8_t* bgra, unsigned width, unsigned height,
                           unsigned rowPitch, unsigned* outOpaque = nullptr);

} // namespace bcc
