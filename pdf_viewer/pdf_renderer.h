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
#include <unordered_map>
#include <map>

#include <qobject.h>
#include <qtimer.h>

#include "book.h"


struct RenderRequest {
    std::wstring path;
    bool should_render_annotations = true;
    float gamma = 1.0f;
    int page;
    float zoom_level;
    float display_scale;
    int slice_index = -1;
    int num_h_slices = 1;
    int num_v_slices = 1;
    int64_t enqueue_us = 0;
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

struct SearchRequest {
    std::wstring path;
    int start_page;
    std::wstring search_term;
    std::vector<SearchResult>* search_results;
    std::mutex* search_results_mutex;
    float* percent_done = nullptr;
    bool* is_searching = nullptr;
    std::optional<std::pair<int, int>> range;
    bool is_regex = false;
    bool case_sensitive = false;
    bool smart_case = false;
};

struct RenderResponse {
    RenderRequest request;
    unsigned int last_access_time;
    int thread;
    fz_pixmap* pixmap = nullptr;
    int width = -1;
    int height = -1;
    RenderHandle texture = 0;
    bool invalid = false;
    bool pending = true;

    void fail() { invalid = true; pending = false; }
    static int find_pending_index(const std::vector<RenderResponse>& responses,
                                  const RenderRequest& request, int owner) {
        for (size_t i = 0; i < responses.size(); ++i) {
            const auto& response = responses[i];
            if (response.pending && !response.invalid && response.thread == owner && response.request == request)
                return static_cast<int>(i);
        }
        return -1;
    }

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

class PdfRenderer : public QObject {
    Q_OBJECT
        // A pointer to the mupdf context to clone.
        // Since the context should only be used from the thread that initialized it,
        // we can not simply clone the context in the initializer because the initializer
        // is called from the main thread. Instead, we just save a pointer to the context
        // in the initializer and then clone the context when run() is called in the worker
        //thread.
        fz_context* context_to_clone;

    std::vector<std::vector<fz_pixmap*>> pixmaps_to_drop;
    std::map<std::pair<int, std::wstring>, fz_document*> opened_documents;

    std::vector<RenderRequest> pending_render_requests;
    std::optional<SearchRequest> pending_search_request;
    std::vector<SearchResult>* active_search_results = nullptr;
    bool search_cancelled = false;
    std::vector<RenderResponse> cached_responses;
    std::vector<std::thread> worker_threads;
    std::thread search_thread;
    std::vector<bool> thread_busy_status;
    bool search_is_busy = false;

    std::mutex opened_documents_mutex;
    std::uint64_t document_epoch = 0; // protected by opened_documents_mutex
    std::mutex pending_requests_mutex;
    std::mutex search_request_mutex;
    std::condition_variable render_work_ready;
    std::condition_variable search_work_ready;
    std::mutex cached_response_mutex;
    std::vector<fz_context*> thread_contexts;
    int num_cached_pages = 5;

    std::mutex searching_mutex;
    std::vector<std::mutex> thread_rendering_mutex;

    QTimer garbage_collect_timer;

    std::atomic_bool stopping = false;
    bool are_documents_invalidated = false;

    int num_threads = 0;

    std::map<std::wstring, std::string> document_passwords;

    fz_context* init_context();
    // Returns an owned reference, independent of the cache's reference.
    fz_document* get_document_with_path(int thread_index, fz_context* mupdf_context, std::wstring path);
    RenderHandle try_closest_rendered_page(const RenderRequest& request, int* page_width, int* page_height);
    void delete_old_pixmaps(int thread_index, fz_context* mupdf_context);
    void queue_pixmap_drop(int thread_index, fz_pixmap* pixmap);
    bool should_cancel_search();
    void run(int thread_index);
    void run_search(int thread_index);

public:
    bool no_rerender = false;

    PdfRenderer(int num_threads, fz_context* context_to_clone);
    ~PdfRenderer();
    void clear_cache();

    void start_threads();
    void join_threads();
    void free_all_resources_for_document(std::wstring doc_path);

    bool is_busy();
    bool is_search_busy();
    // Wait until the worker releases this view's borrowed search state.
    void cancel_search(std::vector<SearchResult>* results);
    //should only be called from the main thread
    void add_request(std::wstring document_path, int page, bool should_render_annotations, float zoom_level, float display_scale, int index, int num_h_slices, int num_v_slices);
    void add_request(std::wstring document_path,
        int page,
        std::wstring term,
        bool is_regex,
        std::vector<SearchResult>* out,
        float* percent_done,
        bool* is_searching,
        std::mutex* mut,
        std::optional<std::pair<int,
        int>> range = {});

    RenderHandle find_rendered_page(std::wstring path, int page, bool should_render_annotations, int index, int num_h_slices, int num_v_slices, float zoom_level, float display_scale, int* page_width, int* page_height);
    void delete_old_pages(bool force_all = false, bool invalidate_all = false);
    void add_password(std::wstring path, std::string password);
    void debug();
    void set_num_cached_pages(int n_cached_pages);

signals:
    void render_advance();
    void search_advance();

};
