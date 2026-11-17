#include "vulkan_canvas.h"

#include <rhi/qrhi.h>
#include <rhi/qshader.h>
#include <QFile>
#include <QMatrix4x4>
#include <QDebug>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <map>
#include <numeric>
#include <tuple>
#include <vector>

extern bool LINEAR_TEXTURE_FILTERING;

namespace {
enum Uniform { Contrast, HighlightColor, Opacity, LineColor, Time, Transform,
               BackgroundColor, Offset, Scale, TypeColors };

struct Vertex {
    float position[2] = {};
    float uv[2] = {};
    float color[4] = {1, 1, 1, 1};
};
struct alignas(16) ShaderUniforms {
    float correction[16] = {};
    float transform[16] = {};
    float color[4] = {1, 1, 1, 1};
    float params[4] = {}; // shader mode, contrast, unused, depth
};
static_assert(sizeof(ShaderUniforms) == 160);

struct ProgramUniforms {
    QMatrix4x4 transform;
    std::array<float, 4> color = {1, 1, 1, 1};
    std::array<float, 2> offset = {0, 0};
    std::array<float, 2> scale = {1, 1};
    std::array<float, 78> type_colors = {};
    float contrast = 1;
};
struct Attribute {
    RenderHandle buffer = 0;
    int components = 2;
    Gpu::Value type = Gpu::Float;
    std::size_t offset = 0;
    int stride = 0;
    bool enabled = false;
};
struct Layout {
    std::array<Attribute, 3> attributes;
    RenderHandle index_buffer = 0;
};
struct PipelineKey {
    bool lines = false;
    bool blend = false;
    bool depth = false;
    bool clear_depth = false;
    bool stencil = false;
    unsigned color_mask = 15;
    unsigned stencil_read = 255;
    unsigned stencil_write = 255;
    Gpu::Value src = Gpu::One, dst = Gpu::Zero;
    Gpu::Value src_alpha = Gpu::One, dst_alpha = Gpu::Zero;
    Gpu::Value compare = Gpu::Always;
    Gpu::Value fail = Gpu::Keep, depth_fail = Gpu::Keep, pass = Gpu::Keep;
    auto tuple() const {
        return std::tie(lines, blend, depth, clear_depth, stencil, color_mask,
                        stencil_read, stencil_write, src, dst, src_alpha, dst_alpha,
                        compare, fail, depth_fail, pass);
    }
    bool operator<(const PipelineKey& other) const { return tuple() < other.tuple(); }
};
struct Draw {
    PipelineKey pipeline;
    ShaderUniforms uniforms;
    std::vector<Vertex> vertices;
    RenderHandle image = 0;
    unsigned stencil_ref = 1;
};

QShader loadShader(const char* path) {
    QFile file(QString::fromLatin1(path));
    if (!file.open(QIODevice::ReadOnly)) qFatal("Cannot open embedded Vulkan shader %s", path);
    auto shader = QShader::fromSerialized(file.readAll());
    if (!shader.isValid()) qFatal("Invalid embedded Vulkan shader %s", path);
    const auto variants = shader.availableShaders();
    if (variants.isEmpty() || std::any_of(variants.begin(), variants.end(), [](const QShaderKey& key) {
            return key.source() != QShader::SpirvShader;
        }))
        qFatal("Embedded Vulkan shader must contain only SPIR-V: %s", path);
    return shader;
}
template<typename T> void createResource(const std::unique_ptr<T>& resource) {
    if (!resource || !resource->create()) qFatal("Failed to create a Vulkan rendering resource");
}
QRhiGraphicsPipeline::BlendFactor blendFactor(Gpu::Value value) {
    switch (value) {
    case Gpu::Zero: return QRhiGraphicsPipeline::Zero;
    case Gpu::One: return QRhiGraphicsPipeline::One;
    case Gpu::SrcAlpha: return QRhiGraphicsPipeline::SrcAlpha;
    case Gpu::OneMinusSrcAlpha: return QRhiGraphicsPipeline::OneMinusSrcAlpha;
    case Gpu::OneMinusDstColor: return QRhiGraphicsPipeline::OneMinusDstColor;
    default: qFatal("Unsupported blend factor");
    }
}
QRhiGraphicsPipeline::CompareOp compareOp(Gpu::Value value) {
    switch (value) {
    case Gpu::Never: return QRhiGraphicsPipeline::Never;
    case Gpu::Equal: return QRhiGraphicsPipeline::Equal;
    case Gpu::NotEqual: return QRhiGraphicsPipeline::NotEqual;
    case Gpu::Always: return QRhiGraphicsPipeline::Always;
    default: qFatal("Unsupported stencil comparison");
    }
}
QRhiGraphicsPipeline::StencilOp stencilOperation(Gpu::Value value) {
    switch (value) {
    case Gpu::Keep: return QRhiGraphicsPipeline::Keep;
    case Gpu::Replace: return QRhiGraphicsPipeline::Replace;
    case Gpu::Increment: return QRhiGraphicsPipeline::IncrementAndClamp;
    case Gpu::Decrement: return QRhiGraphicsPipeline::DecrementAndClamp;
    default: qFatal("Unsupported stencil operation");
    }
}
}

struct VulkanCanvas::Impl {
    QRhi* device = nullptr;
    bool scene_initialized = false;
    QSize logical_size;
    QImage overlay;
    std::unique_ptr<QPainter> painter;
    bool painter_dirty = false;
    RenderHandle next_handle = 1;
    RenderHandle layout = 0, array_buffer = 0, texture = 0, program = Gpu::Page;
    std::map<RenderHandle, QByteArray> buffers;
    std::map<RenderHandle, Layout> layouts;
    std::array<ProgramUniforms, Gpu::Overlay + 1> programs;
    PipelineKey state;
    unsigned stencil_ref = 1;
    QColor background = Qt::black;
    std::vector<Draw> draws;
    std::vector<RenderHandle> overlay_images;
    QShader vertex_shader, fragment_shader;
    std::unique_ptr<QRhiBuffer> vertex_buffer, uniform_buffer;
    std::unique_ptr<QRhiSampler> sampler;
    int vertex_capacity = 0, uniform_capacity = 0;
    bool linear_filtering = true;
    struct Texture {
        std::unique_ptr<QRhiTexture> texture;
        std::unique_ptr<QRhiShaderResourceBindings> bindings;
    };
    std::map<RenderHandle, Texture> textures;
    std::map<PipelineKey, std::unique_ptr<QRhiGraphicsPipeline>> pipelines;

    Layout& currentLayout() { return layouts[layout]; }
    RenderHandle currentBuffer(Gpu::Value target) {
        return target == Gpu::ArrayBuffer ? array_buffer : currentLayout().index_buffer;
    }
    ProgramUniforms& uniforms() { return programs.at(program); }
    float component(const Attribute& attr, std::uint32_t index, int component) const {
        if (!attr.enabled || !attr.buffer) return 0;
        const auto& bytes = buffers.at(attr.buffer);
        const std::size_t stride = attr.stride ? attr.stride : attr.components * sizeof(float);
        const std::size_t offset = attr.offset + index * stride + component * sizeof(float);
        if (offset > std::size_t(bytes.size()) || sizeof(float) > std::size_t(bytes.size()) - offset)
            qFatal("Vertex attribute exceeds its geometry buffer");
        if (attr.type == Gpu::Int) {
            std::int32_t value;
            std::memcpy(&value, bytes.constData() + offset, sizeof(value));
            return float(value);
        }
        float value;
        std::memcpy(&value, bytes.constData() + offset, sizeof(value));
        return value;
    }
};

VulkanCanvas::VulkanCanvas(QWidget* parent) : QRhiWidget(parent), impl(std::make_unique<Impl>()) {
    setApi(Api::Vulkan);
    setSampleCount(4);
    // Enable with SIOYEK_VULKAN_VALIDATION=1 when the optional validation layer is installed.
    setDebugLayerEnabled(qEnvironmentVariableIntValue("SIOYEK_VULKAN_VALIDATION") != 0);
    connect(this, &QRhiWidget::renderFailed, this, [] {
        qFatal("Vulkan initialization/rendering failed. Check your Vulkan ICD and GPU driver.");
    });
}
VulkanCanvas::~VulkanCanvas() { VulkanCanvas::releaseResources(); }

void VulkanCanvas::releaseResources() {
    impl->pipelines.clear();
    impl->textures.clear();
    impl->sampler.reset();
    impl->uniform_buffer.reset();
    impl->vertex_buffer.reset();
    impl->vertex_capacity = impl->uniform_capacity = 0;
    impl->device = nullptr;
}

void VulkanCanvas::initialize(QRhiCommandBuffer*) {
    if (impl->device != rhi()) {
        releaseResources();
        impl->device = rhi();
        if (!rhi() || rhi()->backend() != QRhi::Vulkan)
            qFatal("Sioyek requires the Vulkan rendering backend");
        impl->vertex_shader = loadShader(":/sioyek/vulkan/canvas.vert.qsb");
        impl->fragment_shader = loadShader(":/sioyek/vulkan/canvas.frag.qsb");
        qInfo() << "Sioyek renderer: Vulkan, device:" << rhi()->driverInfo().deviceName;
    }
    // A render target can change on resize/reparent even with the same device.
    impl->pipelines.clear();
    if (!impl->scene_initialized) {
        initializeScene();
        impl->scene_initialized = true;
    }
    impl->logical_size = size();
    resizeScene(width(), height());
}

void VulkanCanvas::render(QRhiCommandBuffer* cb) {
    if (impl->logical_size != size()) {
        impl->logical_size = size();
        resizeScene(width(), height());
    }
    impl->draws.clear();
    const QSize pixel_size = renderTarget()->pixelSize();
    paintScene();
    impl->painter.reset();
    flushPainter();

    const int alignment = rhi()->ubufAligned(sizeof(ShaderUniforms));
    std::size_t total_vertices = 0;
    for (const auto& draw : impl->draws) total_vertices += draw.vertices.size();
    const auto vertex_bytes = total_vertices * sizeof(Vertex);
    const auto uniform_bytes = impl->draws.size() * alignment;
    if (vertex_bytes > std::size_t(std::numeric_limits<int>::max()) ||
        uniform_bytes > std::size_t(std::numeric_limits<int>::max()))
        qFatal("Frame geometry exceeds Vulkan buffer limits");
    auto ensureBuffer = [&](auto& buffer, int& capacity, int needed, QRhiBuffer::UsageFlag usage) {
        if (capacity >= needed && buffer) return;
        capacity = std::max(needed, 4096);
        buffer.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, usage, capacity));
        createResource(buffer);
    };
    if (int(uniform_bytes) > impl->uniform_capacity) {
        // Bindings reference the old uniform buffer; invalidate them before replacement.
        for (auto& [id, texture] : impl->textures) texture.bindings.reset();
        impl->pipelines.clear();
    }
    ensureBuffer(impl->vertex_buffer, impl->vertex_capacity, int(vertex_bytes), QRhiBuffer::VertexBuffer);
    ensureBuffer(impl->uniform_buffer, impl->uniform_capacity, int(uniform_bytes), QRhiBuffer::UniformBuffer);
    if (!impl->sampler || impl->linear_filtering != LINEAR_TEXTURE_FILTERING) {
        for (auto& [id, texture] : impl->textures) texture.bindings.reset();
        impl->linear_filtering = LINEAR_TEXTURE_FILTERING;
        const auto filter = LINEAR_TEXTURE_FILTERING ? QRhiSampler::Linear : QRhiSampler::Nearest;
        impl->sampler.reset(rhi()->newSampler(filter, filter, QRhiSampler::None,
                                             QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        createResource(impl->sampler);
    }
    // Device-local textures belong to this widget/device. Retain only live CPU images.
    for (auto it = impl->textures.begin(); it != impl->textures.end();) {
        if (it->first && !RenderImages::contains(it->first)) it = impl->textures.erase(it);
        else ++it;
    }
    auto* updates = rhi()->nextResourceUpdateBatch();
    auto bindingsFor = [&](RenderHandle id) -> QRhiShaderResourceBindings* {
        auto& entry = impl->textures[id];
        if (!entry.texture) {
            QImage image;
            if (id) image = RenderImages::get(id);
            else {
                image = QImage(1, 1, QImage::Format_RGBA8888);
                image.fill(Qt::white);
            }
            if (image.isNull()) qFatal("Missing cached page image");
            // Keep premultiplied painter pixels: overlay blending uses One.
            if (image.format() == QImage::Format_RGBA8888_Premultiplied)
                image.reinterpretAsFormat(QImage::Format_RGBA8888);
            else image = image.convertToFormat(QImage::Format_RGBA8888);
            entry.texture.reset(rhi()->newTexture(QRhiTexture::RGBA8, image.size()));
            createResource(entry.texture);
            updates->uploadTexture(entry.texture.get(), image);
        }
        if (!entry.bindings) {
            entry.bindings.reset(rhi()->newShaderResourceBindings());
            entry.bindings->setBindings({
                QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
                    0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                    impl->uniform_buffer.get(), sizeof(ShaderUniforms)),
                QRhiShaderResourceBinding::sampledTexture(
                    1, QRhiShaderResourceBinding::FragmentStage, entry.texture.get(), impl->sampler.get())
            });
            createResource(entry.bindings);
        }
        return entry.bindings.get();
    };
    std::vector<QRhiShaderResourceBindings*> bindings;
    std::vector<QRhiGraphicsPipeline*> pipelines;
    bindings.reserve(impl->draws.size());
    pipelines.reserve(impl->draws.size());
    quint32 vertex_offset = 0, uniform_offset = 0;
    const QMatrix4x4 correction = rhi()->clipSpaceCorrMatrix();
    for (auto& draw : impl->draws) {
        std::memcpy(draw.uniforms.correction, correction.constData(), 64);
        const auto bytes = quint32(draw.vertices.size() * sizeof(Vertex));
        updates->updateDynamicBuffer(impl->vertex_buffer.get(), vertex_offset, bytes, draw.vertices.data());
        updates->updateDynamicBuffer(impl->uniform_buffer.get(), uniform_offset, sizeof(ShaderUniforms), &draw.uniforms);
        vertex_offset += bytes;
        uniform_offset += alignment;
        auto* binding = bindingsFor(draw.image);
        bindings.push_back(binding);
        auto& pipeline = impl->pipelines[draw.pipeline];
        if (!pipeline) {
            pipeline.reset(rhi()->newGraphicsPipeline());
            const auto& key = draw.pipeline;
            pipeline->setShaderStages({{QRhiShaderStage::Vertex, impl->vertex_shader},
                                       {QRhiShaderStage::Fragment, impl->fragment_shader}});
            QRhiVertexInputLayout input;
            input.setBindings({{sizeof(Vertex)}});
            input.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, offsetof(Vertex, position)},
                                 {0, 1, QRhiVertexInputAttribute::Float2, offsetof(Vertex, uv)},
                                 {0, 2, QRhiVertexInputAttribute::Float4, offsetof(Vertex, color)}});
            pipeline->setVertexInputLayout(input);
            pipeline->setTopology(key.lines ? QRhiGraphicsPipeline::Lines : QRhiGraphicsPipeline::Triangles);
            pipeline->setCullMode(QRhiGraphicsPipeline::None);
            pipeline->setSampleCount(renderTarget()->sampleCount());
            QRhiGraphicsPipeline::TargetBlend blend;
            blend.enable = key.blend;
            blend.colorWrite = QRhiGraphicsPipeline::ColorMask(key.color_mask);
            blend.srcColor = blendFactor(key.src);
            blend.dstColor = blendFactor(key.dst);
            blend.srcAlpha = blendFactor(key.src_alpha);
            blend.dstAlpha = blendFactor(key.dst_alpha);
            pipeline->setTargetBlends({blend});
            pipeline->setDepthTest(key.depth || key.clear_depth);
            pipeline->setDepthWrite(key.depth || key.clear_depth);
            pipeline->setDepthOp(key.clear_depth ? QRhiGraphicsPipeline::Always : QRhiGraphicsPipeline::Less);
            pipeline->setStencilTest(key.stencil);
            QRhiGraphicsPipeline::StencilOpState stencil;
            stencil.compareOp = compareOp(key.compare);
            stencil.failOp = stencilOperation(key.fail);
            stencil.depthFailOp = stencilOperation(key.depth_fail);
            stencil.passOp = stencilOperation(key.pass);
            pipeline->setStencilFront(stencil);
            pipeline->setStencilBack(stencil);
            pipeline->setStencilReadMask(key.stencil_read);
            pipeline->setStencilWriteMask(key.stencil_write);
            pipeline->setFlags(QRhiGraphicsPipeline::UsesStencilRef | QRhiGraphicsPipeline::UsesScissor);
            pipeline->setShaderResourceBindings(binding);
            pipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
            createResource(pipeline);
        }
        pipelines.push_back(pipeline.get());
    }
    cb->beginPass(renderTarget(), impl->background, {1.0f, 0}, updates);
    vertex_offset = uniform_offset = 0;
    for (std::size_t i = 0; i < impl->draws.size(); ++i) {
        const auto& draw = impl->draws[i];
        cb->setGraphicsPipeline(pipelines[i]);
        cb->setViewport(QRhiViewport(0, 0, pixel_size.width(), pixel_size.height()));
        cb->setScissor(QRhiScissor(0, 0, pixel_size.width(), pixel_size.height()));
        const QRhiCommandBuffer::DynamicOffset offset(0, uniform_offset);
        cb->setShaderResources(bindings[i], 1, &offset);
        cb->setStencilRef(draw.stencil_ref);
        const QRhiCommandBuffer::VertexInput input(impl->vertex_buffer.get(), vertex_offset);
        cb->setVertexInput(0, 1, &input);
        cb->draw(quint32(draw.vertices.size()));
        vertex_offset += quint32(draw.vertices.size() * sizeof(Vertex));
        uniform_offset += alignment;
    }
    cb->endPass();
    for (const auto image : impl->overlay_images) RenderImages::remove(image);
    impl->overlay_images.clear();
}

QPainter* VulkanCanvas::overlayPainter() {
    if (!impl->painter) {
        const QSize pixel_size = renderTarget()->pixelSize();
        if (impl->overlay.size() != pixel_size) {
            impl->overlay = QImage(pixel_size, QImage::Format_RGBA8888_Premultiplied);
            impl->overlay.fill(Qt::transparent);
        }
        impl->overlay.setDevicePixelRatio(devicePixelRatioF());
        impl->painter = std::make_unique<QPainter>(&impl->overlay);
        initializePainter(impl->painter.get());
    }
    impl->painter_dirty = true;
    return impl->painter.get();
}

void VulkanCanvas::beginGpuSection() { flushPainter(); }
void VulkanCanvas::flushPainter() {
    if (!impl->painter_dirty) return;
    impl->painter_dirty = false;
    // Rasterized labels/icons are composed at their original position in the
    // command stream, between GPU page/highlight/drawing passes. Most PDF
    // frames contain no painter content: do not copy/upload transparent layers.
    const QImage& overlay = impl->overlay;
    int left = overlay.width(), top = overlay.height(), right = -1, bottom = -1;
    constexpr quint32 alpha_mask = Q_BYTE_ORDER == Q_LITTLE_ENDIAN ? 0xff000000u : 0x000000ffu;
    for (int y = 0; y < overlay.height(); ++y) {
        const auto* row = reinterpret_cast<const quint32*>(overlay.constScanLine(y));
        const auto* end = row + overlay.width();
        const auto* first = std::find_if(row, end, [](quint32 pixel) { return pixel & alpha_mask; });
        if (first == end) continue;
        const auto last = std::find_if(std::make_reverse_iterator(end),
                                       std::make_reverse_iterator(first),
                                       [](quint32 pixel) { return pixel & alpha_mask; });
        left = std::min(left, int(first - row));
        right = std::max(right, int(last.base() - row) - 1);
        top = std::min(top, y);
        bottom = y;
    }
    if (right < left) return;

    const QRect bounds(QPoint(left, top), QPoint(right, bottom));
    QImage pixels = overlay.copy(bounds);
    // This is an upload-only image. Change the tag while the pixels are unique;
    // reinterpreting a shared RenderImages entry would detach/copy them again.
    pixels.reinterpretAsFormat(QImage::Format_RGBA8888);
    Draw draw;
    draw.pipeline.blend = true;
    draw.pipeline.src = draw.pipeline.src_alpha = Gpu::One;
    draw.pipeline.dst = draw.pipeline.dst_alpha = Gpu::OneMinusSrcAlpha;
    draw.uniforms.params[0] = Gpu::Overlay;
    draw.image = RenderImages::insert(pixels);
    impl->overlay_images.push_back(draw.image);
    // Bounds are physical pixels; use the full render-target size for NDC,
    // independent of logical widget size and fractional display scaling.
    const float x0 = 2.0f * left / overlay.width() - 1.0f;
    const float x1 = 2.0f * (right + 1) / overlay.width() - 1.0f;
    const float y0 = 1.0f - 2.0f * top / overlay.height();
    const float y1 = 1.0f - 2.0f * (bottom + 1) / overlay.height();
    draw.vertices = {{{x0, y0}, {0, 0}}, {{x0, y1}, {0, 1}}, {{x1, y0}, {1, 0}},
                     {{x1, y0}, {1, 0}}, {{x0, y1}, {0, 1}}, {{x1, y1}, {1, 1}}};
    impl->draws.push_back(std::move(draw));
    // The rest of the image is already transparent. Keep painter state intact
    // across GPU boundaries while clearing only the pixels just composed.
    for (int y = top; y <= bottom; ++y)
        std::memset(impl->overlay.scanLine(y) + left * 4, 0, bounds.width() * 4);
}

void VulkanCanvas::createBuffers(int count, RenderHandle* handles) {
    for (int i = 0; i < count; ++i) {
        handles[i] = impl->next_handle++;
        impl->buffers.emplace(handles[i], QByteArray());
    }
}
void VulkanCanvas::deleteBuffers(int count, const RenderHandle* handles) {
    for (int i = 0; i < count; ++i) impl->buffers.erase(handles[i]);
}
void VulkanCanvas::createVertexLayouts(int count, RenderHandle* handles) {
    for (int i = 0; i < count; ++i) handles[i] = impl->next_handle++;
}
void VulkanCanvas::deleteVertexLayouts(int count, const RenderHandle* handles) {
    for (int i = 0; i < count; ++i) impl->layouts.erase(handles[i]);
}
void VulkanCanvas::bindVertexLayout(RenderHandle handle) { impl->layout = handle; }
void VulkanCanvas::bindBuffer(Gpu::Value target, RenderHandle handle) {
    if (target == Gpu::ArrayBuffer) impl->array_buffer = handle;
    else impl->currentLayout().index_buffer = handle;
}
void VulkanCanvas::bufferData(Gpu::Value target, std::size_t size, const void* data, Gpu::Value) {
    if (size > std::size_t(std::numeric_limits<qsizetype>::max())) qFatal("Geometry buffer too large");
    impl->buffers.at(impl->currentBuffer(target)) = QByteArray(static_cast<const char*>(data), qsizetype(size));
}
void VulkanCanvas::vertexAttribute(unsigned index, int size, Gpu::Value type, bool, int stride, const void* offset) {
    auto& attr = impl->currentLayout().attributes.at(index);
    attr.buffer = impl->array_buffer;
    attr.components = size;
    attr.type = type;
    attr.stride = stride;
    attr.offset = reinterpret_cast<std::uintptr_t>(offset);
}
void VulkanCanvas::integerVertexAttribute(unsigned index, int size, Gpu::Value type, int stride, const void* offset) {
    vertexAttribute(index, size, type, false, stride, offset);
}
void VulkanCanvas::enableAttribute(unsigned index) { impl->currentLayout().attributes.at(index).enabled = true; }
void VulkanCanvas::disableAttribute(unsigned index) { impl->currentLayout().attributes.at(index).enabled = false; }
void VulkanCanvas::bindTexture(Gpu::Value, RenderHandle handle) { impl->texture = handle; }
void VulkanCanvas::useProgram(RenderHandle program) { impl->program = program; }
int VulkanCanvas::uniformLocation(RenderHandle, const char* name) {
    const std::map<std::string, int> names = {{"contrast", Contrast}, {"highlight_color", HighlightColor},
        {"opacity", Opacity}, {"line_color", LineColor}, {"time", Time}, {"transform_matrix", Transform},
        {"background_color", BackgroundColor}, {"offset", Offset}, {"scale", Scale}, {"type_colors", TypeColors}};
    return names.at(name);
}
void VulkanCanvas::uniform1f(int location, float value) {
    if (location == Contrast) impl->uniforms().contrast = value;
    else if (location == Opacity) impl->uniforms().color[3] = value;
}
void VulkanCanvas::uniform2fv(int location, int count, const float* value) {
    if (count != 1) qFatal("Invalid vector uniform count");
    auto& destination = location == Offset ? impl->uniforms().offset : impl->uniforms().scale;
    std::copy_n(value, 2, destination.begin());
}
void VulkanCanvas::uniform3fv(int location, int count, const float* value) {
    if (location == TypeColors) {
        if (count != 26) qFatal("Invalid drawing palette size");
        std::copy_n(value, 78, impl->uniforms().type_colors.begin());
    } else {
        if (count != 1) qFatal("Invalid color uniform count");
        std::copy_n(value, 3, impl->uniforms().color.begin());
    }
}
void VulkanCanvas::uniform4fv(int, int count, const float* value) {
    if (count != 1) qFatal("Invalid color uniform count");
    std::copy_n(value, 4, impl->uniforms().color.begin());
}
void VulkanCanvas::uniformMatrix4fv(int, int count, bool transpose, const float* value) {
    if (count != 1) qFatal("Invalid color transform");
    // Document color transforms are row-major; shader uniforms are column-major.
    if (transpose) impl->uniforms().transform = QMatrix4x4(value);
    else std::copy_n(value, 16, impl->uniforms().transform.data());
}
void VulkanCanvas::enable(Gpu::Value capability) {
    if (capability == Gpu::Blend) impl->state.blend = true;
    if (capability == Gpu::StencilTest) impl->state.stencil = true;
    if (capability == Gpu::DepthTest) impl->state.depth = true;
}
void VulkanCanvas::disable(Gpu::Value capability) {
    if (capability == Gpu::Blend) impl->state.blend = false;
    if (capability == Gpu::StencilTest) impl->state.stencil = false;
    if (capability == Gpu::DepthTest) impl->state.depth = false;
}
void VulkanCanvas::blendFunc(Gpu::Value source, Gpu::Value destination) {
    blendFuncSeparate(source, destination, source, destination);
}
void VulkanCanvas::blendFuncSeparate(Gpu::Value source, Gpu::Value destination,
                                    Gpu::Value source_alpha, Gpu::Value destination_alpha) {
    impl->state.src = source; impl->state.dst = destination;
    impl->state.src_alpha = source_alpha; impl->state.dst_alpha = destination_alpha;
}
void VulkanCanvas::stencilFunc(Gpu::Value compare, int reference, unsigned mask) {
    impl->state.compare = compare; impl->state.stencil_read = mask; impl->stencil_ref = reference;
}
void VulkanCanvas::stencilOp(Gpu::Value fail, Gpu::Value depth_fail, Gpu::Value pass) {
    impl->state.fail = fail; impl->state.depth_fail = depth_fail; impl->state.pass = pass;
}
void VulkanCanvas::stencilMask(unsigned mask) { impl->state.stencil_write = mask; }
void VulkanCanvas::colorMask(bool red, bool green, bool blue, bool alpha) {
    impl->state.color_mask = unsigned(red) | (unsigned(green) << 1) | (unsigned(blue) << 2) | (unsigned(alpha) << 3);
}
void VulkanCanvas::clearColor(float red, float green, float blue, float alpha) {
    impl->background = QColor::fromRgbF(red, green, blue, alpha);
}
void VulkanCanvas::clear(unsigned buffers) {
    // Mid-frame stencil/depth clears use a full-screen masked Vulkan draw so
    // color contents survive. This also handles nested overview/image masks.
    Draw draw;
    draw.vertices = {{{-1, -1}}, {{3, -1}}, {{-1, 3}}};
    draw.pipeline.color_mask = (buffers & Gpu::ColorBuffer) ? 15 : 0;
    draw.pipeline.clear_depth = buffers & Gpu::DepthBuffer;
    draw.pipeline.stencil = buffers & Gpu::StencilBuffer;
    draw.pipeline.pass = Gpu::Replace;
    draw.stencil_ref = 0;
    draw.uniforms.params[0] = Gpu::Highlight;
    draw.uniforms.params[3] = (buffers & Gpu::DepthBuffer) ? 1 : 0;
    draw.uniforms.color[0] = impl->background.redF();
    draw.uniforms.color[1] = impl->background.greenF();
    draw.uniforms.color[2] = impl->background.blueF();
    draw.uniforms.color[3] = impl->background.alphaF();
    impl->draws.push_back(std::move(draw));
}
void VulkanCanvas::drawArrays(Gpu::Value topology, int first, int count) {
    if (first < 0 || count < 0) qFatal("Invalid vertex range");
    std::vector<std::uint32_t> indices(count);
    std::iota(indices.begin(), indices.end(), first);
    recordDraw(topology, indices);
}
void VulkanCanvas::drawElements(Gpu::Value topology, int count, Gpu::Value type, const void* offset) {
    if (type != Gpu::UnsignedInt || count < 0) qFatal("Invalid index format");
    if (count == 0) return;
    const auto& bytes = impl->buffers.at(impl->currentLayout().index_buffer);
    const auto start = reinterpret_cast<std::uintptr_t>(offset);
    const auto size = std::size_t(count) * sizeof(std::uint32_t);
    if (start > std::size_t(bytes.size()) || size > std::size_t(bytes.size()) - start)
        qFatal("Index range exceeds geometry buffer");
    std::vector<std::uint32_t> indices(count);
    std::memcpy(indices.data(), bytes.constData() + start, size);
    recordDraw(topology, indices);
}

void VulkanCanvas::recordDraw(Gpu::Value topology, const std::vector<std::uint32_t>& indices) {
    Draw draw;
    draw.pipeline = impl->state;
    draw.pipeline.lines = topology == Gpu::Lines || topology == Gpu::LineLoop;
    draw.stencil_ref = impl->stencil_ref;
    const auto& uniforms = impl->uniforms();
    std::copy_n(uniforms.transform.constData(), 16, draw.uniforms.transform);
    std::copy(uniforms.color.begin(), uniforms.color.end(), draw.uniforms.color);
    draw.uniforms.params[0] = impl->program;
    draw.uniforms.params[1] = uniforms.contrast;
    if (impl->program == Gpu::Page || impl->program == Gpu::DarkPage || impl->program == Gpu::CustomPage)
        draw.image = impl->texture;
    const auto& attributes = impl->currentLayout().attributes;
    auto vertex = [&](std::uint32_t index) {
        Vertex result;
        for (int k = 0; k < 2; ++k) result.position[k] = impl->component(attributes[0], index, k);
        if (impl->program == Gpu::CompiledLine || impl->program == Gpu::CompiledDot) {
            result.position[0] = result.position[0] * uniforms.scale[0] + uniforms.offset[0];
            result.position[1] = -result.position[1] * uniforms.scale[1] + uniforms.offset[1];
            const int type = int(impl->component(attributes[impl->program == Gpu::CompiledLine ? 1 : 2], index, 0));
            if (type < 0 || type >= 26) qFatal("Invalid freehand drawing color index");
            std::copy_n(uniforms.type_colors.data() + type * 3, 3, result.color);
        }
        const bool needs_uv = impl->program == Gpu::Page || impl->program == Gpu::DarkPage ||
            impl->program == Gpu::CustomPage || impl->program == Gpu::Placeholder ||
            impl->program == Gpu::CompiledDot;
        if (needs_uv && attributes[1].components == 2) {
            for (int k = 0; k < 2; ++k) result.uv[k] = impl->component(attributes[1], index, k);
        }
        return result;
    };
    // Expand fans/strips/primitive restarts into portable triangle/line lists.
    // This preserves the original geometry on drivers without triangle fans.
    std::vector<std::uint32_t> segment;
    auto emitSegment = [&] {
        auto appendVertex = [&](std::size_t i) { draw.vertices.push_back(vertex(segment[i])); };
        const auto n = segment.size();
        if (topology == Gpu::TriangleStrip || topology == Gpu::TriangleFan) {
            for (std::size_t i = 2; i < n; ++i) {
                appendVertex(topology == Gpu::TriangleFan ? 0 : i - 2);
                appendVertex(i - 1); appendVertex(i);
            }
        } else if (topology == Gpu::LineLoop) {
            for (std::size_t i = 0; i < n; ++i) { appendVertex(i); appendVertex((i + 1) % n); }
        } else {
            for (std::size_t i = 0; i < n; ++i) appendVertex(i);
        }
        segment.clear();
    };
    for (auto index : indices) {
        if (index == std::numeric_limits<std::uint32_t>::max()) emitSegment();
        else segment.push_back(index);
    }
    emitSegment();
    if (!draw.vertices.empty()) impl->draws.push_back(std::move(draw));
}
