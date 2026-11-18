#pragma once

#include <cstdint>
#include <QImage>

/**
 * @file render_types.h
 * @brief Core types and CPU-side raster image registry for Sioyek's rendering subsystem.
 *
 * Sioyek completely decouples multi-threaded MuPDF CPU rasterization from Vulkan
 * GPU presentation. Worker threads render pages and page slices into CPU-side
 * QImage instances and register them into the RenderImages registry with a unique
 * RenderHandle. The GUI thread retrieves these images on demand and uploads them to
 * GPU textures via VulkanCanvas and QRhi, ensuring that worker threads never invoke
 * GPU or windowing system APIs.
 *
 * @see PageRenderParams
 * @see RenderRequest
 * @see OverviewState
 */

/**
 * @brief Stable application-level handle for GPU resources and CPU raster buffers.
 *
 * Represents an opaque identifier for textures, buffers, and shader programs.
 * Render handles remain stable independent of the underlying graphics context,
 * swapchain recreation, or device reset.
 */
using RenderHandle = std::uint32_t;

/**
 * @namespace RenderImages
 * @brief Central CPU-side pixmap registry bridging MuPDF worker threads and the Vulkan renderer.
 *
 * MuPDF worker threads rasterize PDF pages and slices into QImage buffers, registering
 * them here under unique RenderHandle identifiers. The GUI thread and VulkanCanvas
 * query and consume these images during scene composition without requiring direct
 * synchronization locks across the graphics driver or Vulkan command buffers.
 */
namespace RenderImages {

/**
 * @brief Insert a rasterized QImage into the global image registry.
 *
 * Associates the given image with a newly generated, unique RenderHandle.
 *
 * @param image The CPU-rendered raster image produced by a MuPDF worker.
 * @return A unique RenderHandle referring to the stored image.
 */
RenderHandle insert(const QImage& image);

/**
 * @brief Retrieve a rasterized image associated with the specified handle.
 *
 * @param handle The RenderHandle of the image to retrieve.
 * @return The stored QImage, or a null QImage if the handle is invalid or expired.
 */
QImage get(RenderHandle handle);

/**
 * @brief Remove and release the rasterized image associated with the specified handle.
 *
 * Frees the CPU memory associated with the image once it has been uploaded to the
 * Vulkan GPU texture cache or evicted from the view cache.
 *
 * @param handle The RenderHandle to remove.
 */
void remove(RenderHandle handle);

/**
 * @brief Check whether an image with the specified handle exists in the registry.
 *
 * @param handle The RenderHandle to verify.
 * @return True if the image is currently present; false otherwise.
 */
bool contains(RenderHandle handle);

} // namespace RenderImages
