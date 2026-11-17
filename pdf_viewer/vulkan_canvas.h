#pragma once

#include <QRhiWidget>
#include <QPainter>
#include <memory>
#include <vector>
#include "render_types.h"

// The document layout code records geometry with this small command interface.
// VulkanCanvas snapshots each draw and submits a Vulkan render pass through QRhi.
// It deliberately exposes no OpenGL objects, contexts, or entry points.
namespace Gpu {
enum Value {
    ArrayBuffer, ElementBuffer, DynamicDraw, StaticDraw,
    Float, Int, UnsignedInt, Texture2D,
    Triangles, TriangleStrip, TriangleFan, Lines, LineLoop,
    Blend, StencilTest, DepthTest, CullFace, Multisample, PrimitiveRestart,
    Zero, One, SrcAlpha, OneMinusSrcAlpha, OneMinusDstColor,
    Never, Equal, NotEqual, Always,
    Keep, Replace, Increment, Decrement
};
enum Clear { ColorBuffer = 1, DepthBuffer = 2, StencilBuffer = 4 };
enum Program : RenderHandle {
    Page = 1, DarkPage, CustomPage, Placeholder, Highlight, Ruler,
    DarkRuler, Separator, Stencil, Freehand, CompiledLine, CompiledDot, Overlay
};
}

class VulkanCanvas : public QRhiWidget {
public:
    explicit VulkanCanvas(QWidget* parent = nullptr);
    ~VulkanCanvas() override;

protected:
    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;
    void releaseResources() override;
    virtual void initializeScene() = 0;
    virtual void resizeScene(int width, int height) = 0;
    virtual void paintScene() = 0;
    virtual void initializePainter(QPainter*) {}

    // Acquire only in painter code: this lazily creates the transparent layer
    // and marks it for composition at the next GPU boundary. Reacquire after
    // a boundary instead of retaining a pointer for subsequent painting.
    QPainter* overlayPainter();
    void beginGpuSection();
    void endGpuSection() {}
    void createBuffers(int count, RenderHandle* handles);
    void deleteBuffers(int count, const RenderHandle* handles);
    void createVertexLayouts(int count, RenderHandle* handles);
    void deleteVertexLayouts(int count, const RenderHandle* handles);
    void bindVertexLayout(RenderHandle handle);
    void bindBuffer(Gpu::Value target, RenderHandle handle);
    void bufferData(Gpu::Value target, std::size_t size, const void* data, Gpu::Value usage);
    void vertexAttribute(unsigned index, int size, Gpu::Value type, bool normalized,
                         int stride, const void* offset);
    void integerVertexAttribute(unsigned index, int size, Gpu::Value type,
                                int stride, const void* offset);
    void enableAttribute(unsigned index);
    void disableAttribute(unsigned index);
    void bindTexture(Gpu::Value target, RenderHandle handle);
    void useProgram(RenderHandle program);
    int uniformLocation(RenderHandle program, const char* name);
    void uniform1f(int location, float value);
    void uniform2fv(int location, int count, const float* value);
    void uniform3fv(int location, int count, const float* value);
    void uniform4fv(int location, int count, const float* value);
    void uniformMatrix4fv(int location, int count, bool transpose, const float* value);
    void enable(Gpu::Value capability);
    void disable(Gpu::Value capability);
    void blendFunc(Gpu::Value source, Gpu::Value destination);
    void blendFuncSeparate(Gpu::Value source, Gpu::Value destination,
                           Gpu::Value source_alpha, Gpu::Value destination_alpha);
    void stencilFunc(Gpu::Value compare, int reference, unsigned mask);
    void stencilOp(Gpu::Value fail, Gpu::Value depth_fail, Gpu::Value pass);
    void stencilMask(unsigned mask);
    void colorMask(bool red, bool green, bool blue, bool alpha);
    void clearColor(float red, float green, float blue, float alpha);
    void clear(unsigned buffers);
    void drawArrays(Gpu::Value topology, int first, int count);
    void drawElements(Gpu::Value topology, int count, Gpu::Value type, const void* offset);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    void recordDraw(Gpu::Value topology, const std::vector<std::uint32_t>& indices);
    void flushPainter();
};
