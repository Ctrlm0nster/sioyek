#pragma once

#include <cstdint>
#include <QImage>

// Stable application handles, independent of any graphics device/context.
using RenderHandle = std::uint32_t;

// Page images are produced by MuPDF workers, copied on the GUI thread, and
// uploaded separately by each Vulkan widget. No GPU calls in PdfRenderer.
namespace RenderImages {
RenderHandle insert(const QImage& image);
QImage get(RenderHandle handle);
void remove(RenderHandle handle);
bool contains(RenderHandle handle);
}
