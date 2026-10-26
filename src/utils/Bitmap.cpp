#include "utils/Bitmap.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace bcc {

bool writePremultipliedBmp(const std::filesystem::path& file, const uint8_t* bgra, unsigned width, unsigned height,
                           unsigned rowPitch, unsigned* outOpaque) {
    const unsigned bg[3] = {58, 52, 46}; // BGR
    unsigned rowBytes = (width * 3 + 3) & ~3u;
    std::vector<uint8_t> bmp(static_cast<size_t>(rowBytes) * height);
    unsigned opaque = 0;
    for (unsigned y = 0; y < height; ++y) {
        const uint8_t* src = bgra + static_cast<size_t>(y) * rowPitch;
        uint8_t* dst = &bmp[static_cast<size_t>(height - 1 - y) * rowBytes];
        for (unsigned x = 0; x < width; ++x) {
            const uint8_t* p = src + x * 4;
            unsigned a = p[3];
            if (a > 8) ++opaque;
            for (int c = 0; c < 3; ++c)
                dst[x * 3 + c] = static_cast<uint8_t>(std::min(255u, p[c] + bg[c] * (255 - a) / 255));
        }
    }
    if (outOpaque) *outOpaque = opaque;

    BITMAPFILEHEADER fh{};
    BITMAPINFOHEADER ih{};
    ih.biSize = sizeof(ih);
    ih.biWidth = static_cast<LONG>(width);
    ih.biHeight = static_cast<LONG>(height);
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = static_cast<DWORD>(bmp.size());
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + ih.biSizeImage;
    FILE* f = nullptr;
    if (_wfopen_s(&f, file.c_str(), L"wb") != 0 || !f) return false;
    bool ok = fwrite(&fh, sizeof(fh), 1, f) == 1 && fwrite(&ih, sizeof(ih), 1, f) == 1 &&
              fwrite(bmp.data(), 1, bmp.size(), f) == bmp.size();
    fclose(f);
    return ok;
}

} // namespace bcc
