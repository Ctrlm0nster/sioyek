#pragma once

#include <QRhiWidget>
#include <QPainter>
#include <memory>
#include <vector>
#include "render_types.h"

/**
 * @file vulkan_canvas.h
 * @brief Vulkan hardware-accelerated canvas based on Qt 6 QRhi (Rendering Hardware Interface).
 *
 * VulkanCanvas replaces legacy OpenGL with Qt 6's QRhi targeting the Vulkan graphics
 * backend. Vertex and fragment shaders are compiled ahead-of-time from GLSL into
 * SPIR-V bytecode bundles via Qt's `qsb` tool and embedded directly into the binary
 * as Qt resources (`:/shaders/vulkan/*.qsb`). No runtime GLSL compilation or driver-level
 * shader parsing occurs at runtime.
 *
 * It provides a decoupled GPU abstraction layer (Gpu namespace) that enables the
 * document view to record draw operations, stencil masks, and shader uniforms without
 * exposing raw Vulkan or OpenGL handles. Transparent UI elements (text, hints, borders)
 * are rasterized via QPainter into an ephemeral pre-multiplied alpha image and composited
 * over the Vulkan swapchain using hardware blending.
 */

/**
 * @namespace Gpu
 * @brief Discrete graphics state and topology enumeration for QRhi pipeline commands.
 */
namespace Gpu {

/**
 * @brief Enumeration of graphics pipeline states, blend modes, buffer targets, and data types.
 */
enum Value {
    ArrayBuffer, ElementBuffer, DynamicDraw, StaticDraw,
    Float, Int, UnsignedInt, Texture2D,
    Triangles, TriangleStrip, TriangleFan, Lines, LineLoop,
    Blend, StencilTest, DepthTest, CullFace, Multisample, PrimitiveRestart,
    Zero, One, SrcAlpha, OneMinusSrcAlpha, OneMinusDstColor,
    Never, Equal, NotEqual, Always,
    Keep, Replace, Increment, Decrement
};

/**
 * @brief Buffer clear mask bitflags.
 */
enum Clear {
    ColorBuffer = 1,   ///< Clear the color target attachments.
    DepthBuffer = 2,   ///< Clear the depth attachment.
    StencilBuffer = 4  ///< Clear the stencil buffer attachment.
};

/**
 * @brief Pre-compiled SPIR-V shader programs managed by VulkanCanvas.
 */
enum Program : RenderHandle {
    Page = 1,          ///< Standard textured page rendering with gamma and zoom scaling.
    DarkPage,          ///< Inverted/dark-mode page rendering with configurable contrast.
    CustomPage,        ///< Custom color palette transformation matrix shader.
    Placeholder,       ///< Low-resolution or loading placeholder tile rendering.
    Highlight,         ///< Text highlight boxes and search match highlights.
    Ruler,             ///< Visual reading ruler overlay shader.
    DarkRuler,         ///< Inverted reading ruler for dark mode.
    Separator,         ///< Multi-page separation lines.
    Stencil,           ///< Stencil mask writing program for portal clipping.
    Freehand,          ///< Vector freehand drawings and annotations.
    CompiledLine,      ///< Batch-compiled line buffer drawing program.
    CompiledDot,       ///< Batch-compiled point/dot drawing program.
    Overlay            ///< Transparent QPainter texture composition with premultiplied alpha.
};

} // namespace Gpu

/**
 * @class VulkanCanvas
 * @brief Abstract Qt 6 QRhiWidget providing a hardware-accelerated Vulkan presentation canvas.
 *
 * Implements QRhiWidget's rendering lifecycle (`initialize`, `render`, `releaseResources`)
 * and provides stateful GPU recording primitives for textures, geometry buffers, and
 * stencil tests.
 */
class VulkanCanvas : public QRhiWidget {
public:
    /**
     * @brief Construct a new VulkanCanvas widget.
     * @param parent Optional parent QWidget.
     */
    explicit VulkanCanvas(QWidget* parent = nullptr);

    /**
     * @brief Destructor releasing any active QRhi graphics pipelines and swapchain resources.
     */
    ~VulkanCanvas() override;

protected:
    /**
     * @brief Initialize QRhi pipelines, texture samplers, and SPIR-V shader state.
     * @param cb Active QRhiCommandBuffer for device setup.
     */
    void initialize(QRhiCommandBuffer* cb) override;

    /**
     * @brief Execute the frame render pass and submit recorded draw commands.
     * @param cb Active QRhiCommandBuffer for this frame.
     */
    void render(QRhiCommandBuffer* cb) override;

    /**
     * @brief Release all GPU buffers, pipelines, textures, and render targets.
     */
    void releaseResources() override;

    /**
     * @brief Pure virtual hook invoked during GPU initialization to build widget-specific scene resources.
     */
    virtual void initializeScene() = 0;

    /**
     * @brief Pure virtual hook invoked when the widget surface dimensions change.
     * @param width New surface width in physical pixels.
     * @param height New surface height in physical pixels.
     */
    virtual void resizeScene(int width, int height) = 0;

    /**
     * @brief Pure virtual hook called to record high-level page and document draw calls.
     */
    virtual void paintScene() = 0;

    /**
     * @brief Hook for subclasses to configure font engines and anti-aliasing on the QPainter overlay.
     * @param painter The QPainter targeting the transparent overlay surface.
     */
    virtual void initializePainter(QPainter* painter) {}

    /**
     * @brief Lazily allocate and return a QPainter targeting the transparent overlay surface.
     *
     * The overlay is scanned and uploaded as a minimal bounding-box texture during flushPainter().
     * Must be re-acquired after any GPU draw section.
     *
     * @return Pointer to active QPainter for UI decoration rendering.
     */
    QPainter* overlayPainter();

    /**
     * @brief Begin a discrete GPU drawing pass, flushing pending QPainter overlay commands first.
     */
    void beginGpuSection();

    /**
     * @brief Conclude a GPU drawing section.
     */
    void endGpuSection() {}

    /**
     * @brief Allocate discrete GPU buffer handles.
     * @param count Number of buffers to allocate.
     * @param handles Output array receiving new RenderHandle identifiers.
     */
    void createBuffers(int count, RenderHandle* handles);

    /**
     * @brief Destroy allocated GPU buffers.
     * @param count Number of buffers to free.
     * @param handles Array of RenderHandle identifiers to release.
     */
    void deleteBuffers(int count, const RenderHandle* handles);

    /**
     * @brief Create vertex layout descriptor objects.
     * @param count Number of layouts to create.
     * @param handles Output array receiving layout handles.
     */
    void createVertexLayouts(int count, RenderHandle* handles);

    /**
     * @brief Delete vertex layout descriptor objects.
     * @param count Number of layouts to delete.
     * @param handles Array of layout handles to release.
     */
    void deleteVertexLayouts(int count, const RenderHandle* handles);

    /**
     * @brief Bind an active vertex layout descriptor for subsequent vertex inputs.
     * @param handle Vertex layout handle.
     */
    void bindVertexLayout(RenderHandle handle);

    /**
     * @brief Bind a vertex or element index buffer.
     * @param target Buffer target (e.g. Gpu::ArrayBuffer, Gpu::ElementBuffer).
     * @param handle RenderHandle of the buffer.
     */
    void bindBuffer(Gpu::Value target, RenderHandle handle);

    /**
     * @brief Upload raw vertex or index data into the currently bound buffer.
     * @param target Target buffer type.
     * @param size Size of the data in bytes.
     * @param data Pointer to host data buffer.
     * @param usage Usage hint (Gpu::StaticDraw or Gpu::DynamicDraw).
     */
    void bufferData(Gpu::Value target, std::size_t size, const void* data, Gpu::Value usage);

    /**
     * @brief Configure floating-point vertex attribute pointer layout.
     * @param index Attribute location index.
     * @param size Number of components per vertex (1, 2, 3, or 4).
     * @param type Component data type (e.g. Gpu::Float).
     * @param normalized Whether fixed-point values should be normalized.
     * @param stride Byte offset between consecutive attributes.
     * @param offset Byte offset of the first component in the buffer.
     */
    void vertexAttribute(unsigned index, int size, Gpu::Value type, bool normalized,
                         int stride, const void* offset);

    /**
     * @brief Configure integer vertex attribute layout without floating-point conversion.
     * @param index Attribute location index.
     * @param size Number of components.
     * @param type Integer data type (e.g. Gpu::Int, Gpu::UnsignedInt).
     * @param stride Byte stride between elements.
     * @param offset Byte offset of first element.
     */
    void integerVertexAttribute(unsigned index, int size, Gpu::Value type,
                                int stride, const void* offset);

    /**
     * @brief Enable the specified vertex attribute array location.
     * @param index Attribute index.
     */
    void enableAttribute(unsigned index);

    /**
     * @brief Disable the specified vertex attribute array location.
     * @param index Attribute index.
     */
    void disableAttribute(unsigned index);

    /**
     * @brief Bind a 2D texture handle into the active sampler slot.
     * @param target Texture target (Gpu::Texture2D).
     * @param handle Texture RenderHandle from RenderImages.
     */
    void bindTexture(Gpu::Value target, RenderHandle handle);

    /**
     * @brief Select the active SPIR-V shader program for subsequent draw commands.
     * @param program Program enum value (e.g. Gpu::Page, Gpu::Highlight).
     */
    void useProgram(RenderHandle program);

    /**
     * @brief Query the uniform location index for a named shader parameter.
     * @param program Target program handle.
     * @param name Uniform identifier string.
     * @return Integer uniform index, or -1 if not found.
     */
    int uniformLocation(RenderHandle program, const char* name);

    /**
     * @brief Set a scalar float uniform.
     * @param location Uniform index.
     * @param value Float value.
     */
    void uniform1f(int location, float value);

    /**
     * @brief Set a 2D float vector uniform.
     * @param location Uniform index.
     * @param count Number of 2-element vectors.
     * @param value Pointer to array of float values.
     */
    void uniform2fv(int location, int count, const float* value);

    /**
     * @brief Set a 3D float vector uniform.
     * @param location Uniform index.
     * @param count Number of 3-element vectors.
     * @param value Pointer to array of float values.
     */
    void uniform3fv(int location, int count, const float* value);

    /**
     * @brief Set a 4D float vector uniform.
     * @param location Uniform index.
     * @param count Number of 4-element vectors.
     * @param value Pointer to array of float values.
     */
    void uniform4fv(int location, int count, const float* value);

    /**
     * @brief Set a 4x4 float matrix uniform.
     * @param location Uniform index.
     * @param count Number of matrices.
     * @param transpose Whether matrix is row-major.
     * @param value Pointer to 16-element float array.
     */
    void uniformMatrix4fv(int location, int count, bool transpose, const float* value);

    /**
     * @brief Enable a hardware graphics capability (e.g. Blend, StencilTest).
     * @param capability Capability flag.
     */
    void enable(Gpu::Value capability);

    /**
     * @brief Disable a hardware graphics capability.
     * @param capability Capability flag.
     */
    void disable(Gpu::Value capability);

    /**
     * @brief Set color blending factors for source and destination.
     * @param source Source blend factor.
     * @param destination Destination blend factor.
     */
    void blendFunc(Gpu::Value source, Gpu::Value destination);

    /**
     * @brief Set separate color and alpha blending factors.
     * @param source RGB source factor.
     * @param destination RGB destination factor.
     * @param source_alpha Alpha source factor.
     * @param destination_alpha Alpha destination factor.
     */
    void blendFuncSeparate(Gpu::Value source, Gpu::Value destination,
                           Gpu::Value source_alpha, Gpu::Value destination_alpha);

    /**
     * @brief Configure stencil comparison function, reference value, and bitmask.
     * @param compare Comparison operation (e.g. Gpu::Equal, Gpu::Always).
     * @param reference Reference integer value for stencil test.
     * @param mask Bitmask applied to both reference and stencil values.
     */
    void stencilFunc(Gpu::Value compare, int reference, unsigned mask);

    /**
     * @brief Set actions to take on stencil and depth test pass or fail.
     * @param fail Stencil test failed action.
     * @param depth_fail Stencil passed but depth test failed action.
     * @param pass Both tests passed action.
     */
    void stencilOp(Gpu::Value fail, Gpu::Value depth_fail, Gpu::Value pass);

    /**
     * @brief Control the writing of individual bits in the stencil planes.
     * @param mask Bitmask enabling or disabling stencil bit writes.
     */
    void stencilMask(unsigned mask);

    /**
     * @brief Enable or disable writing into color buffer channels.
     * @param red Enable red channel writes.
     * @param green Enable green channel writes.
     * @param blue Enable blue channel writes.
     * @param alpha Enable alpha channel writes.
     */
    void colorMask(bool red, bool green, bool blue, bool alpha);

    /**
     * @brief Set clear color for the swapchain color attachment.
     * @param red Red component (0.0 - 1.0).
     * @param green Green component (0.0 - 1.0).
     * @param blue Blue component (0.0 - 1.0).
     * @param alpha Alpha component (0.0 - 1.0).
     */
    void clearColor(float red, float green, float blue, float alpha);

    /**
     * @brief Clear specified frame buffer attachments.
     * @param buffers Bitwise OR of Gpu::Clear flags (e.g. ColorBuffer | StencilBuffer).
     */
    void clear(unsigned buffers);

    /**
     * @brief Render non-indexed geometric primitives from bound vertex arrays.
     * @param topology Primitive topology (e.g. Gpu::Triangles, Gpu::TriangleStrip).
     * @param first Starting vertex index.
     * @param count Number of vertices to draw.
     */
    void drawArrays(Gpu::Value topology, int first, int count);

    /**
     * @brief Render indexed geometric primitives using the bound element index buffer.
     * @param topology Primitive topology.
     * @param count Number of indices to draw.
     * @param type Index type (Gpu::UnsignedInt).
     * @param offset Byte offset in index buffer.
     */
    void drawElements(Gpu::Value topology, int count, Gpu::Value type, const void* offset);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    void recordDraw(Gpu::Value topology, const std::vector<std::uint32_t>& indices);
    void flushPainter();
};
