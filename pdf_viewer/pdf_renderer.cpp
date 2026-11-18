#include "pdf_renderer.h"
#include "utils.h"
#include <qdatetime.h>
#include <algorithm>

extern bool LINEAR_TEXTURE_FILTERING;
extern int NUM_V_SLICES;
extern int NUM_H_SLICES;
extern bool TOUCH_MODE;
//extern bool AUTO_EMBED_ANNOTATIONS;
extern bool CASE_SENSITIVE_SEARCH;
extern bool SMARTCASE_SEARCH;
extern float GAMMA;
extern float DARK_MODE_CONTRAST;
extern float CUSTOM_BACKGROUND_COLOR[3];
extern float CUSTOM_TEXT_COLOR[3];
extern float CUSTOM_COLOR_CONTRAST;
extern int MAX_PENDING_REQUESTS;
extern unsigned int CACHE_INVALID_MILIES;

PdfRenderer::PdfRenderer(int num_threads, fz_context* context_to_clone) : context_to_clone(context_to_clone),
pixmaps_to_drop(num_threads),
thread_rendering_mutex(num_threads),
thread_contexts(num_threads + 1, nullptr),
num_threads(num_threads)
{

    // this interval must be less than cache invalidation time
    garbage_collect_timer.setInterval(1000);
    garbage_collect_timer.start();

    for (int i = 0; i < num_threads; i++) {
        thread_busy_status.push_back(false);
    }
    QObject::connect(&garbage_collect_timer, &QTimer::timeout, [&]() {
        delete_old_pages();
        });
}

fz_context* PdfRenderer::init_context() {
    return fz_clone_context(context_to_clone);
}


void PdfRenderer::start_threads() {

    for (int i = 0; i < num_threads; i++) {
        worker_threads.push_back(std::thread([&, i]() {
            run(i);
            }));
    }
    search_thread = std::thread([&]() {
        run_search(num_threads);
        });
}

void PdfRenderer::join_threads()
{
    // Change the predicate under both wait mutexes: a notification cannot be
    // lost between a worker checking it and entering its condition-variable wait.
    {
        std::scoped_lock lock(pending_requests_mutex, search_request_mutex);
        stopping = true;
    }
    render_work_ready.notify_all();
    search_work_ready.notify_all();
    for (auto& worker : worker_threads) {
        if (worker.joinable()) worker.join();
    }
    if (search_thread.joinable()) search_thread.join();
}


void PdfRenderer::add_request(std::wstring document_path, int page, bool should_render_annotations, float zoom_level, float display_scale, int index, int num_h_slices, int num_v_slices) {
    //fz_document* doc = get_document_with_path(document_path);
    if (document_path.size() > 0) {
        RenderRequest req;
        req.path = document_path;
        req.gamma = GAMMA;
        req.page = page;
        req.zoom_level = zoom_level;
        req.slice_index = index;
        req.num_h_slices = num_h_slices;
        req.num_v_slices = num_v_slices;
        req.display_scale = display_scale;
        req.should_render_annotations = should_render_annotations;
        req.enqueue_ns = timing_diag_now_ns();
        req.enqueue_us = req.enqueue_ns / 1000;
        if (is_timing_diag_enabled()) {
            std::ostringstream ss;
            ss << "enqueue page=" << req.page
               << " slice=" << req.slice_index
               << " t_ns=" << req.enqueue_ns
               << " t_us=" << req.enqueue_us;
            log_timing_diag(ss.str());
        }

        bool should_add = true;
        {
            std::lock_guard lock(pending_requests_mutex);
            // Promote speculative request to normal priority if already pending
            for (auto it = pending_speculative_requests.begin(); it != pending_speculative_requests.end(); ) {
                if (*it == req) {
                    it = pending_speculative_requests.erase(it);
                } else {
                    ++it;
                }
            }
            // if the zoom level has changed, there is no point in previous requests with a different zoom level
            for (int i = pending_render_requests.size() - 1; i >= 0; i--) {
                if (pending_render_requests[i].path == req.path && pending_render_requests[i].page == req.page && (pending_render_requests[i].zoom_level != zoom_level)) {
                    pending_render_requests.erase(pending_render_requests.begin() + i);
                }
            }
            for (int i = pending_speculative_requests.size() - 1; i >= 0; i--) {
                if (pending_speculative_requests[i].path == req.path && pending_speculative_requests[i].page == req.page && (pending_speculative_requests[i].zoom_level != zoom_level)) {
                    pending_speculative_requests.erase(pending_speculative_requests.begin() + i);
                }
            }
            for (size_t i = 0; i < pending_render_requests.size(); i++) {
                if (pending_render_requests[i] == req) {
                    should_add = false;
                    break;
                }
            }
            if (should_add) {
                pending_render_requests.push_back(req);
            }
            if (pending_render_requests.size() > (size_t) MAX_PENDING_REQUESTS) {
                pending_render_requests.erase(pending_render_requests.begin());
            }
        }
        if (should_add) render_work_ready.notify_one();
    }
    else {
        std::wcout << "Error: could not find document" << std::endl;
    }
}

void PdfRenderer::add_speculative_request(std::wstring document_path, int page, bool should_render_annotations, float zoom_level, float display_scale, int index, int num_h_slices, int num_v_slices) {
    if (document_path.size() > 0) {
        RenderRequest req;
        req.path = document_path;
        req.gamma = GAMMA;
        req.page = page;
        req.zoom_level = zoom_level;
        req.slice_index = index;
        req.num_h_slices = num_h_slices;
        req.num_v_slices = num_v_slices;
        req.display_scale = display_scale;
        req.should_render_annotations = should_render_annotations;
        req.speculative = true;
        req.enqueue_ns = timing_diag_now_ns();
        req.enqueue_us = req.enqueue_ns / 1000;

        bool should_add = true;
        {
            std::lock_guard lock(pending_requests_mutex);
            for (const auto& r : pending_render_requests) {
                if (r == req) {
                    should_add = false;
                    break;
                }
            }
            if (should_add) {
                for (const auto& r : pending_speculative_requests) {
                    if (r == req) {
                        should_add = false;
                        break;
                    }
                }
            }
            if (should_add) {
                pending_speculative_requests.push_back(req);
                if (pending_speculative_requests.size() > 16) {
                    pending_speculative_requests.erase(pending_speculative_requests.begin());
                }
            }
        }
        if (should_add) {
            if (is_timing_diag_enabled()) {
                std::ostringstream ss;
                ss << "speculative_enqueue page=" << req.page
                   << " slice=" << req.slice_index
                   << " t_ns=" << req.enqueue_ns
                   << " t_us=" << req.enqueue_us;
                log_timing_diag(ss.str());
            }
            render_work_ready.notify_one();
        }
    }
}
void PdfRenderer::add_request(std::wstring document_path,
    int page,
    std::wstring term,
    bool is_regex,
    std::vector<SearchResult>* out,
    float* percent_done,
    bool* is_searching,
    std::mutex* mut,
    std::optional<std::pair<int, int>> range) {

    //fz_document* doc = get_document_with_path(document_path);
    if (document_path.size() > 0) {

        SearchRequest req;
        req.path = document_path;
        req.start_page = page;
        req.search_term = term;
        req.search_results_mutex = mut;
        req.search_results = out;
        req.percent_done = percent_done;
        req.is_searching = is_searching;
        req.range = range;
        req.is_regex = is_regex;
        req.case_sensitive = CASE_SENSITIVE_SEARCH;
        req.smart_case = SMARTCASE_SEARCH;

        {
            std::lock_guard lock(search_request_mutex);
            pending_search_request = req;
        }
        search_work_ready.notify_one();
    }
    else {
        std::wcout << "Error: could not find document" << std::endl;
    }
}

//should only be called from the main thread

RenderHandle PdfRenderer::find_rendered_page(std::wstring path, int page, bool should_render_annotations, int index, int num_h_slices, int num_v_slices, float zoom_level, float display_scale, int* page_width, int* page_height) {
    //fz_document* doc = get_document_with_path(path);
    if (path.size() > 0) {
        RenderRequest req;
        req.path = path;
        req.gamma = GAMMA;
        req.page = page;
        req.zoom_level = zoom_level;
        req.slice_index = index;
        req.num_h_slices = num_h_slices;
        req.num_v_slices = num_v_slices;
        req.display_scale = display_scale;
        req.should_render_annotations = should_render_annotations;
        RenderHandle result = 0;
        {
            std::lock_guard lock(cached_response_mutex);
            for (auto& cached_resp : cached_responses) {
                if (cached_resp.pending) continue;

                if ((cached_resp.request == req) && (cached_resp.invalid == false)) {
                    cached_resp.last_access_time = QDateTime::currentMSecsSinceEpoch();

                    if (page_width) *page_width = cached_resp.width;
                    if (page_height) *page_height = cached_resp.height;

                    if (cached_resp.texture != 0) {
                        result = cached_resp.texture;
                    }
                    else {
                        // Own the pixels before returning them to the MuPDF worker.
                        // Vulkan uploads happen in the receiving widget's render pass.
                        const auto* pixmap = cached_resp.pixmap;
                        QImage image(pixmap->samples, pixmap->w, pixmap->h,
                                     pixmap->stride, QImage::Format_RGB888);
                        result = RenderImages::insert(image.copy());

                        // don't need the pixmap anymore
                        queue_pixmap_drop(cached_resp.thread, cached_resp.pixmap);
                        cached_resp.texture = result;

                    }
                    break;
                }
            }
        }
        if (result == 0) {
            if (TOUCH_MODE) {
                if (!no_rerender) {
                    add_request(path, page, should_render_annotations, zoom_level, display_scale, index, num_h_slices, num_v_slices);
                }
            }
            else {
                add_request(path, page, should_render_annotations, zoom_level, display_scale, index, num_h_slices, num_v_slices);
            }
            return try_closest_rendered_page(req, page_width, page_height);
        }
        return result;
    }
    return 0;
}

RenderHandle PdfRenderer::try_closest_rendered_page(const RenderRequest& request, int* page_width, int* page_height) {
    std::lock_guard lock(cached_response_mutex);
    const int index = RenderResponse::find_closest_index(cached_responses, request);
    if (index < 0) return 0;
    const auto& response = cached_responses[index];
    const double scale = static_cast<double>(request.zoom_level) / response.request.zoom_level;
    const double width = response.width * scale;
    const double height = response.height * scale;
    if (width > std::numeric_limits<int>::max() || height > std::numeric_limits<int>::max()) return 0;
    if (page_width) *page_width = static_cast<int>(width);
    if (page_height) *page_height = static_cast<int>(height);
    return response.texture;
}

void PdfRenderer::delete_old_pages(bool force_all, bool invalidate_all) {
    /*
    Deletes old cached pages. This function should only be called from the main thread.
    Cached images are released immediately but pixmaps should be freed from the thread
    that created them, so we add old pixmaps to pixmaps_to_delete and delete them later from the worker thread
    */

    if (no_rerender) {
        // don't release while resizing, can cause a blank screen
        return;
    }

    cached_response_mutex.lock();
    std::vector<int> indices_to_delete;
    unsigned int now = QDateTime::currentMSecsSinceEpoch();
    std::vector<int> cached_response_times;

    for (size_t i = 0; i < cached_responses.size(); i++) {
        cached_response_times.push_back(now - cached_responses[i].last_access_time);
    }

    const size_t N = static_cast<size_t>(std::max(0, num_cached_pages)) *
                     std::max(1, NUM_V_SLICES) * std::max(1, NUM_H_SLICES);

    if (invalidate_all || force_all) {
        cancel_speculative_requests();
    }

    if (invalidate_all) {
        for (size_t i = 0; i < cached_responses.size(); i++) {
            cached_responses[i].invalid = true;
        }
        are_documents_invalidated = true;
    }

    if (force_all) {
        for (size_t i = 0; i < cached_responses.size(); i++) {
            indices_to_delete.push_back(i);
        }
        are_documents_invalidated = true;
    }
    else if (cached_response_times.size() > (size_t) N) {
        // we never delete N most recent pages
        // todo: make this configurable
        unsigned int time_threshold = now;
        if (N > 0) {
            std::nth_element(cached_response_times.begin(), cached_response_times.begin() + N - 1, cached_response_times.end());
            time_threshold = now - cached_response_times[N - 1];
        }

        for (size_t i = 0; i < cached_responses.size(); i++) {
            if ((N == 0 || cached_responses[i].last_access_time < time_threshold)
                && ((now - cached_responses[i].last_access_time) > CACHE_INVALID_MILIES)) {
                indices_to_delete.push_back(i);
            }
        }
    }

    // We erase from back to front so that erasing one element does not change
    // the index of other elements
    for (int j = indices_to_delete.size() - 1; j >= 0; j--) {
        int index_to_delete = indices_to_delete[j];
        RenderResponse resp = cached_responses[index_to_delete];

        if (resp.texture == 0) {
            queue_pixmap_drop(resp.thread, resp.pixmap);
        }

        if (resp.texture != 0) {
            RenderImages::remove(resp.texture);
        }

        cached_responses.erase(cached_responses.begin() + index_to_delete);
    }
    cached_response_mutex.unlock();
}

void PdfRenderer::run_search(int thread_index)
{
    fz_context* mupdf_context = init_context();
    thread_contexts[thread_index] = mupdf_context;

    while (true) {
        SearchRequest req;
        {
            std::unique_lock lock(search_request_mutex);
            search_work_ready.wait(lock, [this] { return stopping || pending_search_request.has_value(); });
            if (stopping) break;
            req = std::move(*pending_search_request);
            pending_search_request.reset();
            search_is_busy = true;
            active_search_results = req.search_results;
            search_cancelled = false;
        }

        SearchCaseSensitivity search_case_sensitivity = SearchCaseSensitivity::CaseInsensitive;
        if (req.case_sensitive) search_case_sensitivity = SearchCaseSensitivity::CaseSensitive;
        if (req.smart_case) search_case_sensitivity = SearchCaseSensitivity::SmartCase;

        std::unique_lock search_lock(searching_mutex);
        fz_document* doc = nullptr;
        fz_stext_page* stext_page = nullptr;
        fz_var(doc);
        fz_var(stext_page);
        fz_try(mupdf_context) {
            doc = get_document_with_path(thread_index, mupdf_context, req.path);
            if (!doc) fz_throw(mupdf_context, FZ_ERROR_GENERIC, "search document could not be opened");

            int num_pages_in_document = fz_count_pages(mupdf_context, doc);

            int page_begin = 0;
            int page_end = num_pages_in_document - 1;

            if (req.range) {
                page_begin = req.range.value().first - 1;
                page_end = req.range.value().second - 1;
            }
            // make sure page range is valid {
            if (page_begin < 0) page_begin = 0;
            if (page_begin > num_pages_in_document - 1) page_begin = num_pages_in_document - 1;
            if (page_end < 0) page_end = 0;
            if (page_end > num_pages_in_document - 1) page_end = num_pages_in_document - 1;
            //}

            int num_pages = page_end - page_begin + 1;

            req.search_results_mutex->lock();
            req.search_results->clear();
            *req.is_searching = true;
            req.search_results_mutex->unlock();

            if (req.start_page > page_end || req.start_page < page_begin) {
                req.start_page = page_begin;
            }

            int num_handled_pages = 0;
            int i = req.start_page;
            while (num_handled_pages < num_pages && !should_cancel_search()) {
                num_handled_pages++;

                stext_page = fz_new_stext_page_from_page_number(mupdf_context, doc, i, nullptr);

                std::vector<fz_stext_char*> flat_chars;
                get_flat_chars_from_stext_page(stext_page, flat_chars, false);
                std::wstring page_text;
                std::vector<int> page_begin_indices;
                flat_char_prism2(flat_chars, i, page_text, page_begin_indices);
                std::vector<SearchResult> page_results;
                if (req.is_regex == false) {
                    page_results = search_text_with_index(page_text, page_begin_indices, req.search_term, search_case_sensitivity, 0, 0, 0);
                }
                else {
                    page_results = search_regex_with_index(page_text, page_begin_indices, req.search_term, search_case_sensitivity, 0, 0, 0);
                }

                for (auto& pr : page_results) {
                    pr.page = i;
                }

                {
                    std::lock_guard lock(*req.search_results_mutex);
                    req.search_results->insert(req.search_results->end(), page_results.begin(), page_results.end());
                    *req.percent_done = static_cast<float>(num_handled_pages) / num_pages;
                }
                if (num_handled_pages % 16 == 0) emit search_advance();

                fz_drop_stext_page(mupdf_context, stext_page);
                stext_page = nullptr;


                i++;
                if (i > page_end) {
                    i = page_begin;
                }
            }
        }
        fz_always(mupdf_context) {
            fz_drop_stext_page(mupdf_context, stext_page);
            fz_drop_document(mupdf_context, doc);
        }
        fz_catch(mupdf_context) {
            std::cerr << "Error: could not search document" << std::endl;
        }
        search_lock.unlock();
        req.search_results_mutex->lock();
        *req.is_searching = false;
        //*invalidate_pointer = true;
        //if (on_search_invalidate) {
        //	on_search_invalidate();
        //}
        emit search_advance();
        req.search_results_mutex->unlock();
        {
            std::lock_guard lock(search_request_mutex);
            search_is_busy = false;
            active_search_results = nullptr;
        }
        search_work_ready.notify_all();
    }
}

bool PdfRenderer::should_cancel_search() {
    std::lock_guard lock(search_request_mutex);
    return stopping || search_cancelled || pending_search_request.has_value();
}

void PdfRenderer::cancel_search(std::vector<SearchResult>* results) {
    std::unique_lock lock(search_request_mutex);
    if (pending_search_request && pending_search_request->search_results == results)
        pending_search_request.reset();
    if (active_search_results == results) {
        search_cancelled = true;
        search_work_ready.wait(lock, [this, results] { return active_search_results != results; });
    }
}

PdfRenderer::~PdfRenderer() {
    join_threads();
    // main joins all workers before destroying the renderer. Their cloned
    // contexts share the allocator/store locks with context_to_clone.
    garbage_collect_timer.stop();
    for (const auto& response : cached_responses) {
        if (response.texture)
            RenderImages::remove(response.texture);
        else
            fz_drop_pixmap(context_to_clone, response.pixmap);
    }
    for (const auto& deferred : pixmaps_to_drop)
        for (auto* pixmap : deferred)
            fz_drop_pixmap(context_to_clone, pixmap);
    for (const auto& [key, document] : opened_documents)
        fz_drop_document(context_to_clone, document);
    for (auto* context : thread_contexts)
        fz_drop_context(context);
}

fz_document* PdfRenderer::get_document_with_path(int thread_index, fz_context* mupdf_context, std::wstring path) {
    const auto document_id = std::make_pair(thread_index, path);
    std::uint64_t epoch;
    std::string password;
    {
        std::lock_guard lock(opened_documents_mutex);
        auto found = opened_documents.find(document_id);
        if (found != opened_documents.end())
            return fz_keep_document(mupdf_context, found->second);
        epoch = document_epoch;
        auto secret = document_passwords.find(path);
        if (secret != document_passwords.end()) password = secret->second;
    }

    fz_document* document = nullptr;
    fz_var(document);
    fz_try(mupdf_context) {
        document = open_document_with_file_name(mupdf_context, path);
        if (fz_needs_password(mupdf_context, document) && !password.empty())
            fz_authenticate_password(mupdf_context, document, password.c_str());
    }
    fz_catch(mupdf_context) {
        fz_drop_document(mupdf_context, document);
        std::cerr << "Error: could not open document" << std::endl;
        return nullptr;
    }

    {
        std::lock_guard lock(opened_documents_mutex);
        // An invalidation during I/O must not repopulate the cache with an old
        // document. The caller still owns its independent in-flight reference.
        if (epoch == document_epoch)
            opened_documents.emplace(document_id, fz_keep_document(mupdf_context, document));
    }
    return document;
}

void PdfRenderer::queue_pixmap_drop(int thread_index, fz_pixmap* pixmap) {
    if (!pixmap) return;
    {
        std::lock_guard lock(pending_requests_mutex);
        pixmaps_to_drop[thread_index].push_back(pixmap);
    }
    // The owning worker must wake even if no PDF render is queued. A shared
    // condition variable needs notify_all here so the right owner sees its work.
    render_work_ready.notify_all();
}

void PdfRenderer::delete_old_pixmaps(int thread_index, fz_context* mupdf_context) {
    std::vector<fz_pixmap*> deferred;
    {
        std::lock_guard lock(pending_requests_mutex);
        deferred.swap(pixmaps_to_drop[thread_index]);
    }
    for (auto* pixmap : deferred) {
        fz_try(mupdf_context) {
            fz_drop_pixmap(mupdf_context, pixmap);
        }
        fz_catch(mupdf_context) {
            std::wcout << "Error: could not drop pixmap" << std::endl;
        }
    }
}
void PdfRenderer::clear_cache() {
    delete_old_pages(false, true);
}

void PdfRenderer::run(int thread_index) {
    fz_context* mupdf_context = init_context();
    thread_contexts[thread_index] = mupdf_context;

    while (true) {
        RenderRequest req;
        bool has_request = false;
        {
            std::unique_lock lock(pending_requests_mutex);
            thread_busy_status[thread_index] = false;
            render_work_ready.wait(lock, [this, thread_index] {
                return stopping || !pending_render_requests.empty() ||
                       !pending_speculative_requests.empty() ||
                       !pixmaps_to_drop[thread_index].empty();
            });
            if (stopping) break;
            if (!pending_render_requests.empty()) {
                req = std::move(pending_render_requests.back());
                pending_render_requests.pop_back();
                thread_busy_status[thread_index] = true;
                has_request = true;
            } else if (!pending_speculative_requests.empty()) {
                req = std::move(pending_speculative_requests.back());
                pending_speculative_requests.pop_back();
                thread_busy_status[thread_index] = true;
                has_request = true;
            }
        }
        delete_old_pixmaps(thread_index, mupdf_context);
        if (!has_request) continue;

        int64_t worker_start_ns = timing_diag_now_ns();
        int64_t worker_start_us = worker_start_ns / 1000;
        if (is_timing_diag_enabled()) {
            std::ostringstream ss;
            ss << "worker_start page=" << req.page
               << " slice=" << req.slice_index
               << " thread=" << thread_index
               << (req.speculative ? " speculative=1" : "")
               << " t_ns=" << worker_start_ns
               << " t_us=" << worker_start_us
               << " queue_wait_ms=" << ((worker_start_ns - req.enqueue_ns) / 1000000.0);
            log_timing_diag(ss.str());
        }

        // if the request is already rendered, just return the previous result
        cached_response_mutex.lock();

        if (are_documents_invalidated) {
            std::lock_guard<std::mutex> lock(opened_documents_mutex);
            for (auto [_, document] : opened_documents) {
                fz_drop_document(mupdf_context, document);
            }
            opened_documents.clear();
            ++document_epoch;
            are_documents_invalidated = false;
        }

        bool is_already_rendered = false;
        for (const auto& cached_rep : cached_responses) {
            if ((cached_rep.request == req) && (cached_rep.invalid == false)) is_already_rendered = true;
        }

        if (!is_already_rendered) {
            RenderResponse resp;
            resp.thread = thread_index;
            resp.request = req;
            resp.last_access_time = QDateTime::currentMSecsSinceEpoch();
            resp.texture = 0;
            resp.invalid = false;
            resp.pending = true;
            cached_responses.push_back(resp);
        }

        cached_response_mutex.unlock();
        thread_rendering_mutex[thread_index].lock();
        if (!is_already_rendered) {

            fz_document* doc = nullptr;
            fz_pixmap* rendered_pixmap = nullptr;
            fz_page* page = nullptr;
            fz_device* draw_device = nullptr;
            fz_var(doc);
            fz_var(rendered_pixmap);
            fz_var(page);
            fz_var(draw_device);
            fz_try(mupdf_context) {
                fz_matrix transform_matrix = fz_pre_scale(fz_identity, req.zoom_level * req.display_scale, req.zoom_level * req.display_scale);
                doc = get_document_with_path(thread_index, mupdf_context, req.path);
                if (!doc) fz_throw(mupdf_context, FZ_ERROR_GENERIC, "render document could not be opened");

                //if (AUTO_EMBED_ANNOTATIONS) {
                //	page = fz_load_page(mupdf_context, doc, req.page);
                //	rendered_pixmap = fz_new_pixmap_from_page_contents(mupdf_context, page, transform_matrix, fz_device_rgb(mupdf_context), 0);
                //	fz_drop_page(mupdf_context, page);
                //}
                //else {
                if (req.slice_index == -1) {
                    if (req.should_render_annotations) {
                        rendered_pixmap = fz_new_pixmap_from_page_number(mupdf_context, doc, req.page, transform_matrix, fz_device_rgb(mupdf_context), 0);
                    }
                    else {
                        page = fz_load_page(mupdf_context, doc, req.page);
                        rendered_pixmap = fz_new_pixmap_from_page_contents(mupdf_context, page, transform_matrix, fz_device_rgb(mupdf_context), 0);
                    }
                }
                else {
                    page = fz_load_page(mupdf_context, doc, req.page);
                    fz_rect rect = fz_bound_page(mupdf_context, page);

                    fz_irect bbox = get_index_irect(rect, req.slice_index, transform_matrix, req.num_h_slices, req.num_v_slices);

                    rendered_pixmap = fz_new_pixmap_with_bbox(mupdf_context, fz_device_rgb(mupdf_context), bbox, nullptr, 0); // todo: use alpha

                    fz_clear_pixmap_with_value(mupdf_context, rendered_pixmap, 0xFF);
                    draw_device = fz_new_draw_device(mupdf_context, transform_matrix, rendered_pixmap);

                    if (req.should_render_annotations) {
                        fz_run_page(mupdf_context, page, draw_device, fz_identity, nullptr); // todo: use cookie to report progress
                    }
                    else {
                        fz_run_page_contents(mupdf_context, page, draw_device, fz_identity, nullptr); // todo: use cookie to report progress
                    }

                    fz_close_device(mupdf_context, draw_device);

                }

                if (req.gamma != 1.0f) {
                    fz_gamma_pixmap(mupdf_context, rendered_pixmap, req.gamma);
                }

                RenderResponse resp;
                resp.thread = thread_index;
                resp.request = req;
                resp.texture = 0;
                resp.invalid = false;

                cached_response_mutex.lock();
                int index = RenderResponse::find_pending_index(cached_responses, req, thread_index);
                if (index >= 0) {
                    cached_responses[index].last_access_time = QDateTime::currentMSecsSinceEpoch();
                    cached_responses[index].pixmap = rendered_pixmap;
                    cached_responses[index].width = rendered_pixmap->w;
                    cached_responses[index].height = rendered_pixmap->h;
                    cached_responses[index].pending = false;
                    rendered_pixmap = nullptr; // ownership transferred to the response
                }

                cached_response_mutex.unlock();

                int64_t raster_done_ns = timing_diag_now_ns();
                int64_t raster_done_us = raster_done_ns / 1000;
                if (is_timing_diag_enabled()) {
                    std::ostringstream ss;
                    ss << "raster_done page=" << req.page
                       << " slice=" << req.slice_index
                       << " thread=" << thread_index
                       << " t_ns=" << raster_done_ns
                       << " t_us=" << raster_done_us
                       << " raster_ms=" << ((raster_done_ns - worker_start_ns) / 1000000.0);
                    log_timing_diag(ss.str());
                }

                emit render_advance();

            }
            fz_always(mupdf_context) {
                fz_drop_device(mupdf_context, draw_device);
                fz_drop_page(mupdf_context, page);
                fz_drop_pixmap(mupdf_context, rendered_pixmap);
                fz_drop_document(mupdf_context, doc);
            }
            fz_catch(mupdf_context) {
                std::cerr << "Error: could not render page" << std::endl;
                std::lock_guard lock(cached_response_mutex);
                const int index = RenderResponse::find_pending_index(cached_responses, req, thread_index);
                if (index >= 0) cached_responses[index].fail();
            }
        }
        thread_rendering_mutex[thread_index].unlock();
        {
            std::lock_guard lock(pending_requests_mutex);
            thread_busy_status[thread_index] = false;
        }
    }
}

void PdfRenderer::add_password(std::wstring path, std::string password) {
    {
        std::lock_guard lock(opened_documents_mutex);
        document_passwords[path] = std::move(password);
    }
    delete_old_pages(true, false);
}

bool PdfRenderer::is_search_busy() {
    std::lock_guard lock(search_request_mutex);
    return pending_search_request.has_value() || search_is_busy;
}

bool PdfRenderer::is_busy() {
    std::lock_guard lock(pending_requests_mutex);
    for (bool busy : thread_busy_status) {
        if (busy) return true;
    }
    return !pending_render_requests.empty();
}

bool PdfRenderer::has_idle_worker() {
    std::lock_guard lock(pending_requests_mutex);
    if (!pending_render_requests.empty()) return false;
    for (bool busy : thread_busy_status) {
        if (!busy) return true;
    }
    return false;
}

bool PdfRenderer::has_pending_normal_requests() {
    std::lock_guard lock(pending_requests_mutex);
    return !pending_render_requests.empty();
}

void PdfRenderer::cancel_speculative_requests() {
    std::lock_guard lock(pending_requests_mutex);
    pending_speculative_requests.clear();
}

bool PdfRenderer::is_page_rendered(std::wstring path, int page, bool should_render_annotations, int index, int num_h_slices, int num_v_slices, float zoom_level, float display_scale) {
    if (path.empty()) return true;

    RenderRequest req;
    req.path = path;
    req.gamma = GAMMA;
    req.page = page;
    req.zoom_level = zoom_level;
    req.slice_index = index;
    req.num_h_slices = num_h_slices;
    req.num_v_slices = num_v_slices;
    req.display_scale = display_scale;
    req.should_render_annotations = should_render_annotations;

    std::lock_guard lock(cached_response_mutex);
    for (const auto& cached_resp : cached_responses) {
        if (!cached_resp.pending && !cached_resp.invalid && (cached_resp.request == req)) {
            return true;
        }
    }
    return false;
}

void PdfRenderer::free_all_resources_for_document(std::wstring doc_path) {
    searching_mutex.lock();
    for (int i = 0; i < num_threads; i++) {
        thread_rendering_mutex[i].lock();
    }

    delete_old_pages(true, true); // todo: this is overkill, just delete the pixmaps for the document

    {
        std::lock_guard lock(pending_requests_mutex);
        for (auto it = pending_speculative_requests.begin(); it != pending_speculative_requests.end(); ) {
            if (it->path == doc_path) {
                it = pending_speculative_requests.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = pending_render_requests.begin(); it != pending_render_requests.end(); ) {
            if (it->path == doc_path) {
                it = pending_render_requests.erase(it);
            } else {
                ++it;
            }
        }
    }

    {
        std::lock_guard lock(opened_documents_mutex);
        ++document_epoch;
        // Include the search worker. All worker-operation locks are held here.
        for (int i = 0; i <= num_threads; ++i) {
            auto found = opened_documents.find(std::make_pair(i, doc_path));
            if (found != opened_documents.end()) {
                fz_drop_document(context_to_clone, found->second);
                opened_documents.erase(found);
            }
        }
    }

    for (int i = 0; i < num_threads; i++) {
        thread_rendering_mutex[i].unlock();
    }
    searching_mutex.unlock();
}

void PdfRenderer::debug() {
    cached_response_mutex.lock();
    for (auto resp : cached_responses) {
        std::wcout << resp.request.path << L" " << resp.request.page << " " << resp.request.slice_index << L" " << resp.request.zoom_level << "(" << resp.width << "*" << resp.height << ")" << std::endl;
    }
    std::wcout << "________________________________________\n";
    cached_response_mutex.unlock();
}

void PdfRenderer::set_num_cached_pages(int n_cached_pages) {
    num_cached_pages = n_cached_pages;
}
