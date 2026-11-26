#include "pdf_renderer.h"
#include <algorithm>
#include <iostream>
#include <cstdlib>

static void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

int main() {
    RenderRequest request{};
    request.path = L"retry.pdf";
    request.zoom_level = request.display_scale = 1;
    RenderResponse failed{};
    failed.request = request;
    failed.thread = 0;
    failed.fail();
    require(failed.invalid && !failed.pending, "A failed response must leave the pending state");

    RenderResponse retry{};
    retry.request = request;
    retry.thread = 0;
    std::vector<RenderResponse> responses{failed, retry};
    require(RenderResponse::find_pending_index(responses, request, 0) == 1,
            "A retry must complete its own response, not the failed slot");
    // Cache invalidation may also mark an in-flight response invalid.
    responses[0].pending = true;
    require(RenderResponse::find_pending_index(responses, request, 0) == 1,
            "Invalidated in-flight responses must never receive completed pixels");
    require(RenderResponse::find_pending_index(responses, request, 1) == -1,
            "Completion must match the owning worker");
    responses[1].pending = false;
    require(RenderResponse::find_pending_index(responses, request, 0) == -1,
            "A completed retry must leave no eligible pending response");
    auto changed = request;
    changed.gamma = 1.2f;
    require(changed != request, "Gamma snapshots must participate in cache identity");

    request.zoom_level = 2;
    auto cached = [&](float zoom) {
        RenderResponse response{};
        response.request = request;
        response.request.zoom_level = zoom;
        response.pending = false;
        response.width = 612;
        response.height = 792;
        response.texture = 1;
        return response;
    };
    responses = {cached(1), cached(1.9f)};
    require(RenderResponse::find_closest_index(responses, request) == 1, "Choose the nearest zoom below the request");
    responses = {cached(3), cached(2.1f)};
    require(RenderResponse::find_closest_index(responses, request) == 1, "Choose the nearest zoom above the request");
    responses = {cached(1.5f), cached(2.5f)};
    require(RenderResponse::find_closest_index(responses, request) == 1, "Prefer higher resolution for equal distances");
    std::reverse(responses.begin(), responses.end());
    require(RenderResponse::find_closest_index(responses, request) == 0, "Ties must not depend on insertion order");
    responses = {cached(2)};
    for (int field = 0; field < 13; ++field) {
        auto incompatible = responses;
        auto& r = incompatible[0];
        switch (field) {
        case 0: r.invalid = true; break;
        case 1: r.pending = true; break;
        case 2: r.texture = 0; break;
        case 3: r.request.gamma += 1; break;
        case 4: r.request.display_scale += 1; break;
        case 5: r.request.should_render_annotations = !r.request.should_render_annotations; break;
        case 6: r.request.path = L"other.pdf"; break;
        case 7: ++r.request.page; break;
        case 8: ++r.request.slice_index; break;
        case 9: ++r.request.num_h_slices; break;
        case 10: ++r.request.num_v_slices; break;
        case 11: r.request.zoom_level = 0; break;
        case 12: r.request.zoom_level = std::numeric_limits<float>::infinity(); break;
        }
        require(RenderResponse::find_closest_index(incompatible, request) == -1, "Reject an incompatible fallback");
    }
    require(RenderResponse::find_closest_index({}, request) == -1, "Handle an empty cache");
    request.zoom_level = std::numeric_limits<float>::quiet_NaN();
    require(RenderResponse::find_closest_index(responses, request) == -1, "Reject non-finite requested zoom");
    std::cout << "PASS: render failure/retry, identity and compatible closest-zoom selection\n";
}
