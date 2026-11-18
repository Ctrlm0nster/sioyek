#pragma once

#include <vector>
#include <string>
#include <mupdf/fitz.h>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <limits>
#include <condition_variable>
#include <variant>
#include <unordered_map>
#include <map>
#include <optional>
#include <iostream>
#include <functional>
#include <thread>

#include <qobject.h>
#include <qtimer.h>

#include "book.h"
#include "render_types.h"

/**
 * @file pdf_renderer.h
 * @brief Concurrency engine and multi-threaded MuPDF rasterization subsystem.
 *
 * PdfRenderer orchestrates asynchronous, multi-threaded document rasterization and
 * text searching using MuPDF 1.26.11. To ensure high rendering throughput and low latency:
 *
 * - **Context Cloning**: Each worker thread operates on its own cloned `fz_context`
 *   sharing master allocator and locks, eliminating thread contention inside MuPDF.
 * - **Condition Variable Synchronization**: Workers wait on `render_work_ready` and
 *   `search_work_ready` condition variables rather than busy-polling or sleeping,
 *   waking immediately when new requests arrive.
 * - **Speculative Pre-rendering**: Pages along the active reading direction are queued
 *   speculatively when workers are idle, but can be preempted or discarded if navigation changes.
 * - **Epoch-based Cache Invalidation**: Thread-safe document lifetime management uses
 *   `fz_keep_document`/`fz_drop_document` and a monotonic `document_epoch` counter to
 *   safely drop stale or cancelled raster jobs without race conditions.
 * - **Event-driven Readiness**: Completed raster passes emit `render_advance()` to notify
 *   the GUI thread and advance waiting IPC commands without periodic polling.
 */

/**
 * @struct RenderRequest
 * @brief Key identifying a specific page or slice rendering configuration.
 */
struct RenderRequest {
    std::wstring path;                    ///< Canonical filesystem path to the PDF document.
    bool should_render_annotations = true;///< Whether PDF annotations (highlights, stamps) are rasterized.
    float gamma = 1.0f;                   ///< Display gamma exponent for contrast adjustment.
    int page = 0;                         ///< Zero-based page number.
    float zoom_level = 1.0f;              ///< Target document zoom multiplier.
    float display_scale = 1.0f;           ///< Device pixel ratio / HiDPI scaling factor.
    int slice_index = -1;                 ///< Tile slice index (-1 for un-sliced full page).
    int num_h_slices = 1;                 ///< Total horizontal subdivisions.
    int num_v_slices = 1;                 ///< Total vertical subdivisions.
    int64_t enqueue_ns = 0;               ///< Nanosecond monotonic timestamp when enqueued.
    int64_t enqueue_us = 0;               ///< Microsecond monotonic timestamp when enqueued.
    bool speculative = false;             ///< True if enqueued as predictive lookahead.

    /**
     * @brief Compare two render requests for geometric and visual equality.
     * Note that speculative flag and timestamps are omitted from equality checks.
     */
    bool operator==(const RenderRequest& o) const {
        return path == o.path &&
               should_render_annotations == o.should_render_annotations &&
               gamma == o.gamma &&
               page == o.page &&
               zoom_level == o.zoom_level &&
               display_scale == o.display_scale &&
               slice_index == o.slice_index &&
               num_h_slices == o.num_h_slices &&
               num_v_slices == o.num_v_slices;
    }
};

/**
 * @struct SearchRequest
 * @brief Asynchronous text search query dispatched to the background search thread.
 */
struct SearchRequest {
    std::wstring path;                                     ///< Canonical path to target document.
    int start_page = 0;                                    ///< Page index where search begins.
    std::wstring search_term;                              ///< Text or regex pattern to locate.
    std::vector<SearchResult>* search_results = nullptr;   ///< Destination container for found matches.
    std::mutex* search_results_mutex = nullptr;            ///< Mutex guarding search_results writes.
    float* percent_done = nullptr;                         ///< Optional progress fraction (0.0 to 1.0).
    bool* is_searching = nullptr;                          ///< Atomic flag indicating search activity.
    std::optional<std::pair<int, int>> range;              ///< Optional [start_page, end_page] bounding interval.
    bool is_regex = false;                                 ///< True if pattern is a regular expression.
    bool case_sensitive = false;                           ///< True for strict case matching.
    bool smart_case = false;                               ///< True to match case only when uppercase letters are entered.
};

/**
 * @struct RenderResponse
 * @brief Cached rasterization result or in-flight job descriptor.
 */
struct RenderResponse {
    RenderRequest request;             ///< The request parameters associated with this raster.
    unsigned int last_access_time = 0; ///< Access timestamp used for LRU cache eviction.
    int thread = -1;                   ///< Worker thread index that produced or owns this response.
    fz_pixmap* pixmap = nullptr;       ///< MuPDF CPU pixmap buffer (retained until dropped).
    int width = -1;                    ///< Raster width in physical pixels.
    int height = -1;                   ///< Raster height in physical pixels.
    RenderHandle texture = 0;          ///< Unique RenderHandle registered in RenderImages.
    bool invalid = false;              ///< True if raster failed, expired, or was cancelled.
    bool pending = true;               ///< True while worker thread is actively rasterizing.

    /**
     * @brief Mark this response as failed and remove pending status.
     */
    void fail() { invalid = true; pending = false; }

    /**
     * @brief Find the index of an in-flight pending response matching the request and thread owner.
     * @param responses Response cache list.
     * @param request Target render request parameters.
     * @param owner Worker thread ID.
     * @return Index in responses, or -1 if no matching pending job exists.
     */
    static int find_pending_index(const std::vector<RenderResponse>& responses,
                                  const RenderRequest& request, int owner) {
        for (size_t i = 0; i < responses.size(); ++i) {
            const auto& response = responses[i];
            if (response.pending && !response.invalid && response.thread == owner && response.request == request)
                return static_cast<int>(i);
        }
        return -1;
    }

    /**
     * @brief Find the best existing rendered page with the closest zoom level for immediate placeholder preview.
     * @param responses Response cache list.
     * @param request Target render request parameters.
     * @return Index of closest match in responses, or -1 if none available.
     */
    static int find_closest_index(const std::vector<RenderResponse>& responses,
                                 const RenderRequest& request) {
        if (!std::isfinite(request.zoom_level) || request.zoom_level <= 0 ||
            !std::isfinite(request.display_scale) || request.display_scale <= 0)
            return -1;
        float distance = std::numeric_limits<float>::infinity();
        int best = -1;
        for (size_t i = 0; i < responses.size(); ++i) {
            const auto& response = responses[i];
            const auto& cached = response.request;
            if (response.pending || response.invalid || !response.texture ||
                response.width <= 0 || response.height <= 0 ||
                !std::isfinite(cached.zoom_level) || cached.zoom_level <= 0 ||
                cached.path != request.path || cached.page != request.page ||
                cached.slice_index != request.slice_index ||
                cached.num_h_slices != request.num_h_slices || cached.num_v_slices != request.num_v_slices ||
                cached.display_scale != request.display_scale || cached.gamma != request.gamma ||
                cached.should_render_annotations != request.should_render_annotations)
                continue;
            const float candidate_distance = std::abs(cached.zoom_level - request.zoom_level);
            if (candidate_distance < distance ||
                (candidate_distance == distance && (best < 0 || cached.zoom_level > responses[best].request.zoom_level))) {
                distance = candidate_distance;
                best = static_cast<int>(i);
            }
        }
        return best;
    }
};

/**
 * @class PdfRenderer
 * @brief Multi-threaded document rasterization, search manager, and tile cache engine.
 *
 * Manages a thread pool of MuPDF workers that render PDF pages into CPU image buffers,
 * synchronizing task distribution using condition variables and caching completed
 * pixmaps using LRU replacement.
 */
class PdfRenderer : public QObject {
    Q_OBJECT

    fz_context* context_to_clone;                       ///< Master MuPDF context used to clone thread-local contexts.
    std::vector<std::vector<fz_pixmap*>> pixmaps_to_drop;///< Per-worker deferred pixmap disposal lists.
    std::map<std::pair<int, std::wstring>, fz_document*> opened_documents; ///< Thread-cached fz_document references.

    std::vector<RenderRequest> pending_render_requests;        ///< Queue of visible page/slice render requests.
    std::vector<RenderRequest> pending_speculative_requests;   ///< Queue of predictive lookahead render requests.
    std::optional<SearchRequest> pending_search_request;       ///< Current pending text search request.
    std::vector<SearchResult>* active_search_results = nullptr;///< Search results target being populated.
    bool search_cancelled = false;                             ///< Cooperative search cancellation flag.
    std::vector<RenderResponse> cached_responses;              ///< LRU cache of rendered and in-flight responses.
    std::vector<std::thread> worker_threads;                   ///< Worker thread pool.
    std::thread search_thread;                                 ///< Dedicated background search thread.
    std::vector<bool> thread_busy_status;                      ///< Per-worker busy status flags.
    bool search_is_busy = false;                               ///< Search thread busy flag.

    std::mutex opened_documents_mutex;                         ///< Mutex guarding opened_documents and document_epoch.
    std::uint64_t document_epoch = 0;                          ///< Monotonic epoch counter for safe cache invalidation.
    std::mutex pending_requests_mutex;                         ///< Mutex guarding pending render request queues.
    std::mutex search_request_mutex;                           ///< Mutex guarding search requests.
    std::condition_variable render_work_ready;                 ///< Condition variable waking render workers.
    std::condition_variable search_work_ready;                 ///< Condition variable waking the search thread.
    std::mutex cached_response_mutex;                          ///< Mutex guarding cached_responses.
    std::vector<fz_context*> thread_contexts;                  ///< Thread-local cloned MuPDF contexts.
    int num_cached_pages = 5;                                  ///< Capacity limit for LRU page response cache.

    std::mutex searching_mutex;                                ///< Mutex synchronizing search execution.
    std::vector<std::mutex> thread_rendering_mutex;            ///< Mutexes per worker thread during raster pass.

    QTimer garbage_collect_timer;                             ///< Periodic timer to release unused cache resources.

    std::atomic_bool stopping = false;                         ///< Termination flag signaling worker threads to exit.
    bool are_documents_invalidated = false;                    ///< Flag indicating document reloads.

    int num_threads = 0;                                       ///< Total number of render worker threads.

    std::map<std::wstring, std::string> document_passwords;    ///< Cached passwords per document path.

    fz_context* init_context();
    fz_document* get_document_with_path(int thread_index, fz_context* mupdf_context, std::wstring path);
    RenderHandle try_closest_rendered_page(const RenderRequest& request, int* page_width, int* page_height);
    void delete_old_pixmaps(int thread_index, fz_context* mupdf_context);
    void queue_pixmap_drop(int thread_index, fz_pixmap* pixmap);
    bool should_cancel_search();
    void run(int thread_index);
    void run_search(int thread_index);

public:
    bool no_rerender = false;                                  ///< Debug/profiling flag to prevent re-rendering.

    /**
     * @brief Construct a new PdfRenderer concurrency engine.
     * @param num_threads Number of render worker threads to spawn.
     * @param context_to_clone Master MuPDF context with initialized font and allocator locks.
     */
    PdfRenderer(int num_threads, fz_context* context_to_clone);

    /**
     * @brief Destructor stopping all worker threads and releasing cached pixmaps.
     */
    ~PdfRenderer();

    /**
     * @brief Flush and clear all cached rendered page responses and textures.
     */
    void clear_cache();

    /**
     * @brief Start render worker threads and background search thread.
     */
    void start_threads();

    /**
     * @brief Signal all threads to terminate and wait for join completion.
     */
    void join_threads();

    /**
     * @brief Evict all cached documents, responses, and textures associated with the given path.
     * @param doc_path Path of document being closed or reloaded.
     */
    void free_all_resources_for_document(std::wstring doc_path);

    /**
     * @brief Check whether any render worker is actively rasterizing.
     */
    bool is_busy();

    /**
     * @brief Check whether the background search thread is actively executing.
     */
    bool is_search_busy();

    /**
     * @brief Check whether at least one render worker is idle and ready for work.
     */
    bool has_idle_worker();

    /**
     * @brief Check whether there are pending normal (non-speculative) render requests.
     */
    bool has_pending_normal_requests();

    /**
     * @brief Cancel all queued speculative (lookahead) render requests.
     */
    void cancel_speculative_requests();

    /**
     * @brief Check if a specific page or slice has already completed rasterization.
     */
    bool is_page_rendered(std::wstring path, int page, bool should_render_annotations, int index,
                          int num_h_slices, int num_v_slices, float zoom_level, float display_scale);

    /**
     * @brief Cancel any active background search and wait for the search worker to release state.
     * @param results Pointer to results buffer to unbind.
     */
    void cancel_search(std::vector<SearchResult>* results);

    /**
     * @brief Enqueue a high-priority render request for a visible page or slice.
     * Must be called from the main GUI thread.
     */
    void add_request(std::wstring document_path, int page, bool should_render_annotations,
                     float zoom_level, float display_scale, int index, int num_h_slices, int num_v_slices);

    /**
     * @brief Enqueue a low-priority predictive lookahead request for an upcoming page.
     * Must be called from the main GUI thread.
     */
    void add_speculative_request(std::wstring document_path, int page, bool should_render_annotations,
                                 float zoom_level, float display_scale, int index, int num_h_slices, int num_v_slices);

    /**
     * @brief Dispatch a text search request to the background search thread.
     */
    void add_request(std::wstring document_path,
                     int page,
                     std::wstring term,
                     bool is_regex,
                     std::vector<SearchResult>* out,
                     float* percent_done,
                     bool* is_searching,
                     std::mutex* mut,
                     std::optional<std::pair<int, int>> range = {});

    /**
     * @brief Look up a rendered page handle, returning a closest-zoom placeholder if still rasterizing.
     */
    RenderHandle find_rendered_page(std::wstring path, int page, bool should_render_annotations,
                                    int index, int num_h_slices, int num_v_slices, float zoom_level,
                                    float display_scale, int* page_width, int* page_height);

    /**
     * @brief Evict least-recently-used page responses exceeding the cache capacity.
     * @param force_all If true, clears the entire cache.
     * @param invalidate_all If true, marks all cached pages as invalid.
     */
    void delete_old_pages(bool force_all = false, bool invalidate_all = false);

    /**
     * @brief Register a decryption password for a password-protected PDF.
     */
    void add_password(std::wstring path, std::string password);

    /**
     * @brief Print debug diagnostics on active queues and thread states.
     */
    void debug();

    /**
     * @brief Configure the maximum number of pages retained in the LRU response cache.
     */
    void set_num_cached_pages(int n_cached_pages);

signals:
    /**
     * @brief Signal emitted on the GUI thread whenever a worker completes a render job.
     * Triggers screen invalidation and advances waiting IPC commands immediately.
     */
    void render_advance();

    /**
     * @brief Signal emitted whenever the background search worker produces incremental results.
     */
    void search_advance();
};
