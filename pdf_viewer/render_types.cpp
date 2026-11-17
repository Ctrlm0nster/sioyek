#include "render_types.h"

#include <QHash>
#include <QThread>
#include <QCoreApplication>
#include <limits>
#include <stdexcept>

namespace {
QHash<RenderHandle, QImage> images;
RenderHandle next_image = 1;

inline void check_thread() {
    Q_ASSERT(!qApp || QThread::currentThread() == qApp->thread());
}
}

RenderHandle RenderImages::insert(const QImage& image) {
    check_thread();
    if (image.isNull()) throw std::runtime_error("Cannot cache an empty page image");
    if (next_image == std::numeric_limits<RenderHandle>::max())
        throw std::overflow_error("Page image handle space exhausted");
    const auto handle = next_image++;
    images.insert(handle, image);
    return handle;
}

QImage RenderImages::get(RenderHandle handle) {
    check_thread();
    return images.value(handle);
}

void RenderImages::remove(RenderHandle handle) {
    check_thread();
    images.remove(handle);
}

bool RenderImages::contains(RenderHandle handle) {
    check_thread();
    return images.contains(handle);
}
