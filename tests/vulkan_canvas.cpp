#include "vulkan_canvas.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QDebug>
#include <cmath>

bool LINEAR_TEXTURE_FILTERING = true;

class TestCanvas final : public VulkanCanvas {
public:
    RenderHandle image = 0;
    int frames = 0;
    int painter_initializations = 0;
    bool paint_labels = true;
    bool paint_edge_pixel = false;
    explicit TestCanvas() {
        QImage pixels(4, 4, QImage::Format_RGB888);
        pixels.fill(Qt::red);
        image = RenderImages::insert(pixels);
        resize(640, 480);
    }
    ~TestCanvas() override { RenderImages::remove(image); }

private:
    RenderHandle buffer = 0, uvs = 0, layout = 0, types = 0, indices = 0;
    void initializePainter(QPainter*) override { ++painter_initializations; }
    void initializeScene() override {
        createBuffers(1, &buffer);
        createBuffers(1, &uvs);
        createBuffers(1, &types);
        createBuffers(1, &indices);
        createVertexLayouts(1, &layout);
        bindVertexLayout(layout);
        bindBuffer(Gpu::ArrayBuffer, buffer);
        vertexAttribute(0, 2, Gpu::Float, false, 0, nullptr);
        enableAttribute(0);
        bindBuffer(Gpu::ArrayBuffer, uvs);
        const float uv[] = {0, 0, 1, 0, 0, 1, 1, 1};
        bufferData(Gpu::ArrayBuffer, sizeof(uv), uv, Gpu::StaticDraw);
        vertexAttribute(1, 2, Gpu::Float, false, 0, nullptr);
        enableAttribute(1);
        bindBuffer(Gpu::ArrayBuffer, types);
        const int type[] = {0, 0, 0, 0};
        bufferData(Gpu::ArrayBuffer, sizeof(type), type, Gpu::StaticDraw);
        bindBuffer(Gpu::ElementBuffer, indices);
        const std::uint32_t elements[] = {0, 1, 2, 3, UINT32_MAX, 0, 1, 2};
        bufferData(Gpu::ElementBuffer, sizeof(elements), elements, Gpu::StaticDraw);
    }
    void resizeScene(int, int) override {}
    void rectangle(float x0, float y0, float x1, float y1) {
        const float vertices[] = {x0, y1, x1, y1, x0, y0, x1, y0};
        bindBuffer(Gpu::ArrayBuffer, buffer);
        bufferData(Gpu::ArrayBuffer, sizeof(vertices), vertices, Gpu::DynamicDraw);
        drawArrays(Gpu::TriangleStrip, 0, 4);
    }
    void solid(float r, float g, float b, float opacity = 1) {
        useProgram(Gpu::Highlight);
        const float color[] = {r, g, b};
        uniform3fv(uniformLocation(Gpu::Highlight, "highlight_color"), 1, color);
        uniform1f(uniformLocation(Gpu::Highlight, "opacity"), opacity);
    }
    void paintScene() override {
        ++frames;
        beginGpuSection();
        bindVertexLayout(layout);
        disable(Gpu::StencilTest);
        disable(Gpu::DepthTest);
        disable(Gpu::Blend);
        colorMask(true, true, true, true);
        clearColor(1, 1, 1, 1);
        clear(Gpu::ColorBuffer | Gpu::StencilBuffer | Gpu::DepthBuffer);
        bindTexture(Gpu::Texture2D, image);
        useProgram(Gpu::Page);
        rectangle(-.9f, .5f, -.5f, .9f);
        useProgram(Gpu::DarkPage);
        uniform1f(uniformLocation(Gpu::DarkPage, "contrast"), 1);
        rectangle(-.4f, .5f, 0, .9f);
        useProgram(Gpu::CustomPage);
        const float transform[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                    0, 0, 0, 0, 0, 1, 0, 1};
        uniformMatrix4fv(uniformLocation(Gpu::CustomPage, "transform_matrix"), 1, false, transform);
        rectangle(.1f, .5f, .3f, .9f);
        const float row_major[16] = {0, 0, 0, 0, 0, 0, 0, 1,
                                    0, 0, 0, 0, 0, 0, 0, 1};
        uniformMatrix4fv(uniformLocation(Gpu::CustomPage, "transform_matrix"), 1, true, row_major);
        rectangle(.3f, .5f, .5f, .9f);

        solid(0, 0, 1, .5f);
        enable(Gpu::Blend);
        blendFuncSeparate(Gpu::SrcAlpha, Gpu::OneMinusSrcAlpha, Gpu::One, Gpu::OneMinusSrcAlpha);
        rectangle(-.9f, -.1f, -.5f, .3f);
        disable(Gpu::Blend);
        // Invisible stencil write followed by an equal test.
        enable(Gpu::StencilTest);
        stencilFunc(Gpu::Never, 1, 255);
        stencilOp(Gpu::Replace, Gpu::Replace, Gpu::Replace);
        rectangle(-.4f, -.1f, 0, .3f);
        stencilFunc(Gpu::Equal, 1, 255);
        stencilOp(Gpu::Keep, Gpu::Keep, Gpu::Keep);
        solid(1, 0, 0);
        rectangle(-.45f, -.2f, .05f, .4f);
        // Clear only stencil: previous color must survive, next green draw must fail.
        clear(Gpu::StencilBuffer);
        solid(0, 1, 0);
        rectangle(-.45f, -.2f, .05f, .4f);
        disable(Gpu::StencilTest);

        useProgram(Gpu::DarkRuler);
        const float line[] = {.2f, .4f, .6f, 1};
        uniform4fv(uniformLocation(Gpu::DarkRuler, "line_color"), 1, line);
        rectangle(.1f, -.1f, .5f, .3f);
        // Depth clear must preserve color and allow another drawing at z=0.
        enable(Gpu::DepthTest);
        clear(Gpu::DepthBuffer);
        solid(1, 0, 0);
        rectangle(-.9f, -.8f, -.5f, -.4f);
        solid(0, 1, 0);
        rectangle(-.9f, -.8f, -.5f, -.4f); // rejected at the same depth
        clear(Gpu::DepthBuffer);
        solid(0, 0, 1);
        rectangle(-.85f, -.75f, -.55f, -.45f);
        disable(Gpu::DepthTest);
        // The scratchpad's indexed line/dot paths include primitive restarts
        // and integer color attributes, unlike ordinary page rectangles.
        const float positions[] = {.6f, -.9f, .9f, -.9f, .6f, -.5f, .9f, -.5f};
        bindBuffer(Gpu::ArrayBuffer, buffer);
        bufferData(Gpu::ArrayBuffer, sizeof(positions), positions, Gpu::DynamicDraw);
        bindBuffer(Gpu::ArrayBuffer, types);
        integerVertexAttribute(1, 1, Gpu::Int, 0, nullptr);
        useProgram(Gpu::CompiledLine);
        float palette[78] = {.25f, .5f, .75f};
        uniform3fv(uniformLocation(Gpu::CompiledLine, "type_colors"), 26, palette);
        drawElements(Gpu::TriangleStrip, 8, Gpu::UnsignedInt, nullptr);
        const float dots[] = {.6f, -.3f, .9f, -.3f, .6f, .1f, .9f, .1f};
        bindBuffer(Gpu::ArrayBuffer, buffer);
        bufferData(Gpu::ArrayBuffer, sizeof(dots), dots, Gpu::DynamicDraw);
        bindBuffer(Gpu::ArrayBuffer, uvs);
        vertexAttribute(1, 2, Gpu::Float, false, 0, nullptr);
        bindBuffer(Gpu::ArrayBuffer, types);
        integerVertexAttribute(2, 1, Gpu::Int, 0, nullptr);
        enableAttribute(2);
        useProgram(Gpu::CompiledDot);
        uniform3fv(uniformLocation(Gpu::CompiledDot, "type_colors"), 26, palette);
        enable(Gpu::Blend);
        drawElements(Gpu::TriangleStrip, 5, Gpu::UnsignedInt, nullptr);
        disable(Gpu::Blend);
        useProgram(Gpu::Freehand);
        const float freehand[] = {.9f, .5f, .1f, 1};
        uniform4fv(uniformLocation(Gpu::Freehand, "line_color"), 1, freehand);
        rectangle(.6f, -.8f, .9f, -.4f);
        // Solid geometry has no UV input, even when the previous page's UV
        // attribute is still enabled and has fewer entries than this mesh.
        const float outside[12] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
        bindBuffer(Gpu::ArrayBuffer, buffer);
        bufferData(Gpu::ArrayBuffer, sizeof(outside), outside, Gpu::DynamicDraw);
        drawArrays(Gpu::Triangles, 0, 6);
        endGpuSection();

        // Painter content before GPU draws, plus a final painter overlay.
        if (paint_labels)
            overlayPainter()->fillRect(QRectF(width() * .55, height() * .70, width() * .2, height() * .2), Qt::green);
        beginGpuSection();
        solid(1, 0, 0);
        rectangle(.1f, -.8f, .3f, -.4f);
        endGpuSection();
        if (paint_labels)
            overlayPainter()->fillRect(QRectF(width() * .56, height() * .72, width() * .05, height() * .05), QColor(0, 0, 255, 128));
        if (paint_edge_pixel)
            overlayPainter()->fillRect(QRectF(width() - 1, height() - 1, 1, 1), Qt::magenta);
    }
};

// Painter state and clipping must survive a GPU boundary, including image/text
// operations. A subsequent GPU-only frame must have no stale overlay pixels.
class PainterStateCanvas final : public VulkanCanvas {
public:
    int frames = 0;
    bool paint_content = true;
    PainterStateCanvas() { resize(640, 480); }
private:
    void initializeScene() override {}
    void resizeScene(int, int) override {}
    void paintScene() override {
        ++frames;
        beginGpuSection();
        clearColor(1, 1, 1, 1);
        clear(Gpu::ColorBuffer);
        if (!paint_content) return;
        overlayPainter()->save();
        overlayPainter()->translate(width() * .25, height() * .25);
        overlayPainter()->setClipRect(QRectF(0, 0, width() * .25, height() * .25));
        overlayPainter()->fillRect(QRectF(-width(), -height(), width() * 3, height() * 3), Qt::green);
        beginGpuSection();
        overlayPainter()->fillRect(QRectF(0, 0, width() * .125, height() * .125), Qt::blue);
        QImage pixels(2, 2, QImage::Format_RGBA8888_Premultiplied);
        pixels.fill(Qt::cyan);
        overlayPainter()->drawImage(QRectF(width() * .15, height() * .15, width() * .05, height() * .05), pixels);
        overlayPainter()->restore();
        overlayPainter()->setPen(Qt::black);
        overlayPainter()->drawText(QPointF(width() * .6, height() * .2), QStringLiteral("Overlay text"));
    }
};

template<typename Canvas>
void waitForFrame(Canvas& widget) {
    const int previous = widget.frames;
    widget.update();
    QElapsedTimer timer;
    timer.start();
    while (widget.frames <= previous && timer.elapsed() < 10000) {
        QApplication::processEvents();
        QThread::msleep(10);
    }
    if (widget.frames <= previous) qFatal("Vulkan widget did not render a frame");
}
void checkPixel(const QImage& image, float x, float y, QColor expected) {
    const QColor actual = image.pixelColor(int((x + 1) * .5 * image.width()),
                                          int((1 - y) * .5 * image.height()));
    if (std::abs(actual.red() - expected.red()) > 4 ||
        std::abs(actual.green() - expected.green()) > 4 ||
        std::abs(actual.blue() - expected.blue()) > 4) {
        image.save("/tmp/sioyek-vulkan-test-failure.png");
        qFatal("Pixel (%f,%f): got %d,%d,%d expected %d,%d,%d", double(x), double(y),
               actual.red(), actual.green(), actual.blue(), expected.red(), expected.green(), expected.blue());
    }
}
void testRendering() {
    TestCanvas widget;
    widget.show();
    for (const QSize size : {QSize(640, 480), QSize(800, 600)}) {
        widget.resize(size);
        waitForFrame(widget);
        const auto image = widget.grabFramebuffer();
        if (image.isNull()) qFatal("Vulkan framebuffer readback failed");
        checkPixel(image, -.7f, .7f, Qt::red);
        checkPixel(image, -.2f, .7f, Qt::red); // inverted red + hue compensation
        checkPixel(image, .2f, .7f, Qt::green);
        checkPixel(image, .4f, .7f, Qt::green);
        checkPixel(image, -.7f, .1f, QColor(128, 128, 255));
        checkPixel(image, -.2f, .1f, Qt::red);
        checkPixel(image, .02f, .1f, Qt::white); // outside the stencil mask
        checkPixel(image, .3f, .1f, QColor(204, 153, 102));
        checkPixel(image, -.7f, -.6f, Qt::blue);
        checkPixel(image, -.88f, -.6f, Qt::red);
        checkPixel(image, .2f, -.6f, Qt::red);
        checkPixel(image, .4f, -.6f, Qt::green);
        checkPixel(image, .16f, -.48f, QColor(127, 0, 128));
        checkPixel(image, .75f, .7f, QColor(64, 128, 191));
        checkPixel(image, .75f, .1f, QColor(64, 128, 191));
        checkPixel(image, .75f, -.6f, QColor(230, 128, 26));
    }
    LINEAR_TEXTURE_FILTERING = false;
    waitForFrame(widget);
    checkPixel(widget.grabFramebuffer(), -.7f, .7f, Qt::red);
    widget.paint_labels = false;
    const int painter_count = widget.painter_initializations;
    waitForFrame(widget);
    checkPixel(widget.grabFramebuffer(), .4f, -.6f, Qt::white);
    widget.resize(720, 540);
    waitForFrame(widget);
    checkPixel(widget.grabFramebuffer(), .4f, -.6f, Qt::white);
    if (widget.painter_initializations != painter_count)
        qFatal("GPU-only frames must not initialize a painter, including after resize");
    widget.paint_labels = true;
    widget.paint_edge_pixel = true;
    waitForFrame(widget);
    const auto edge = widget.grabFramebuffer();
    checkPixel(edge, .4f, -.6f, Qt::green);
    checkPixel(edge, 1.0f - 1.0f / edge.width(), -1.0f + 1.0f / edge.height(), Qt::magenta);
    // A second top-level window has independent device resources but shares
    // the same application image cache, as the helper/portal window does.
    TestCanvas helper;
    RenderImages::remove(helper.image);
    helper.image = widget.image;
    helper.show();
    waitForFrame(helper);
    checkPixel(helper.grabFramebuffer(), -.7f, .7f, Qt::red);
    helper.close();
    PainterStateCanvas state;
    state.show();
    waitForFrame(state);
    const auto painted = state.grabFramebuffer();
    checkPixel(painted, -.4f, .4f, Qt::blue);
    checkPixel(painted, -.05f, .05f, Qt::green);
    checkPixel(painted, -.15f, .15f, Qt::cyan);
    checkPixel(painted, -.6f, .4f, Qt::white);
    bool has_text = false;
    for (int y = painted.height() / 10; y < painted.height() / 4; ++y)
        for (int x = painted.width() * 3 / 5; x < painted.width() * 9 / 10; ++x)
            has_text |= painted.pixelColor(x, y).red() < 128;
    if (!has_text) qFatal("Painter text disappeared");
    state.paint_content = false;
    waitForFrame(state);
    const auto empty = state.grabFramebuffer();
    QImage white(empty.size(), empty.format());
    white.fill(Qt::white);
    if (empty != white)
        qFatal("GPU-only frame retained painter content");
    state.close();
    widget.close();
    QGuiApplication::sync();
    qInfo("PASS: Vulkan pages, color modes, blend, stencil/depth clears, painter order, resize, filtering, indexed drawings, multiple windows");
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTimer::singleShot(0, &app, [&app] {
        testRendering();
        app.quit();
    });
    return app.exec();
}
