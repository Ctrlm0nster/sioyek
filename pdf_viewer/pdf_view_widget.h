#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <optional>
#include <utility>
#include <array>

#include "vulkan_canvas.h"


#include <qpainter.h>

//#include "document_view.h"
#include "coordinates.h"
#include "book.h"
#include "path.h"

class DocumentView;
class PdfRenderer;
class ConfigManager;
class ScratchPad;

struct MarkedDataRect {
    DocumentRect rect;
    int type;
};

enum HighlightRenderFlags
{
    HRF_FILL = 1 << 0,
    HRF_BORDER = 1 << 1,
    HRF_UNDERLINE = 1 << 2,
    HRF_STRIKE = 1 << 3,
    HRF_INVERTED = 1 << 4
};



struct SceneResources {
    RenderHandle vertex_buffer_object = 0;
    RenderHandle uv_buffer_object = 0;
    RenderHandle line_points_buffer_object = 0;
    RenderHandle rendered_program = 0;
    RenderHandle rendered_dark_program = 0;
    RenderHandle custom_color_program = 0;
    RenderHandle unrendered_program = 0;
    RenderHandle highlight_program = 0;
    RenderHandle vertical_line_program = 0;
    RenderHandle vertical_line_dark_program = 0;
    RenderHandle separator_program = 0;
    RenderHandle stencil_program = 0;
    RenderHandle line_program = 0;
    RenderHandle compiled_drawing_program = 0;
    RenderHandle compiled_dots_program = 0;

    int dark_mode_contrast_uniform_location = 0;
    int highlight_color_uniform_location = 0;
    int highlight_opacity_uniform_location = 0;
    int line_color_uniform_location = 0;
    int line_time_uniform_location = 0;

    int compiled_drawing_offset_uniform_location = 0;
    int compiled_drawing_scale_uniform_location = 0;
    int compiled_drawing_colors_uniform_location = 0;
    int compiled_dot_color_uniform_location = 0;
    int compiled_dot_offset_uniform_location = 0;
    int compiled_dot_scale_uniform_location = 0;

    //int gamma_uniform_location = 0;

    int custom_color_transform_uniform_location = 0;

    int separator_background_color_uniform_location = 0;
    int freehand_line_color_uniform_location = 0;

    RenderHandle compiled_points_vertex_buffer_object = 0;
    RenderHandle compiled_points_index_buffer_object = 0;

    //RenderHandle test_vao;
    //RenderHandle test_vbo;
    //RenderHandle test_ibo;

    bool is_initialized = false;
};


class PdfViewWidget : public VulkanCanvas {
public:

    enum ColorPalette {
        Normal,
        Dark,
        Custom,
        None
    };

    ColorPalette get_current_color_mode();
private:
    SceneResources scene_resources;

    RenderHandle vertex_array_object;
    DocumentView* document_view = nullptr;
    ScratchPad* scratchpad = nullptr;
    PdfRenderer* pdf_renderer = nullptr;
    ConfigManager* config_manager = nullptr;
    std::vector<SearchResult> search_results;
    int current_search_result_index = -1;
    std::mutex search_results_mutex;
    bool is_search_cancelled = true;
    bool is_searching = false;
    bool should_highlight_links = false;
    bool should_highlight_words = false;
    bool should_show_numbers = false;
    bool should_show_rect_hints = false;
    ColorPalette color_mode = ColorPalette::Normal;
    bool is_helper = false;
    float percent_done = 0.0f;
    std::string tag_prefix = "";
    std::vector<std::string> highlighted_tags;

    std::optional<AbsoluteRect> pending_portal_rect = {};

    QIcon bookmark_icon;
    QIcon portal_icon;
    QIcon bookmark_icon_white;
    QIcon portal_icon_white;
    QIcon hourglass_icon;

    std::optional<AbsoluteRect> character_highlight_rect = {};
    std::optional<AbsoluteRect> wrong_character_rect = {};
    bool show_control_rect;

    std::optional<AbsoluteDocumentPos> underline = {};
    std::vector<DocumentRect> overview_highlights;

    int rotation_index = 0;
    bool is_dragging = false;
    bool fastread_mode = false;
    int selected_highlight_index = -1;
    int selected_bookmark_index = -1;

    int last_mouse_down_window_x = 0;
    int last_mouse_down_window_y = 0;

    float last_mouse_down_document_offset_x = 0;
    float last_mouse_down_document_offset_y = 0;
    std::vector<AbsoluteRect> pending_download_portals;

    //float vertical_line_location;
    //bool should_draw_vertical_line = false;
    QDateTime creation_time;

    std::optional<std::function<void(const OpenedBookState&)>> on_link_edit = {};
    std::optional<OverviewState> overview_page = {};

    float overview_half_width = 0.8f;
    float overview_half_height = 0.4f;

    float overview_offset_x = 0.0f;
    float overview_offset_y = 0.0f;

    std::optional<AbsoluteRect> selected_rectangle = {};

protected:
    void initializeScene() override;
    void resizeScene(int w, int h) override;
    void render_highlight_window(RenderHandle program, NormalizedWindowRect window_rect, int flags, int line_width_in_pixels=-1);
    void render_highlight_absolute(RenderHandle program, AbsoluteRect absolute_document_rect, int flags);
    void render_line_window(RenderHandle program, float vertical_pos, std::optional<NormalizedWindowRect> ruler_rect = {});
    void render_highlight_document(RenderHandle program, DocumentRect doc_rect, int flags=HRF_FILL | HRF_BORDER);
    void paintScene() override;
    void initializePainter(QPainter* painter) override;
    void my_render();
    void render_scratchpad();
    void add_coordinates_for_window_point(DocumentView* dv, float window_x, float window_y, float r, int point_polygon_vertices, std::vector<float>& out_coordinates);
    void render_drawings(DocumentView* dv, const std::vector<FreehandDrawing>& drawings, bool highlighted = false);
    void render_compiled_drawings();
    void render_line(DocumentView* dv, FreehandDrawing drawing);
    std::vector<std::pair<QRect, QString>> get_hint_rect_and_texts();

    void enable_stencil();
    void write_to_stencil();
    void draw_stencil_rects(const std::vector<NormalizedWindowRect>& window_rects);
    void draw_stencil_rects(int page, const std::vector<PagelessDocumentRect>& rects, bool in_overview = false);
    void use_stencil_to_write(bool eq);
    void disable_stencil();

    void render_transparent_background();

public:
    bool is_helper_waiting_for_render = false;
    std::optional<QImage> cached_framebuffer;
    int last_cache_num_drawings = -1;
    float last_cache_offset_x = -1;
    float last_cache_offset_y = -1;
    float last_cache_zoom_level = -1;
    QDateTime last_scratchpad_update_datetime;

    bool visible_drawing_mask[26];
    FreehandDrawing current_drawing;
    std::vector<FreehandDrawing> moving_drawings;
    std::vector<PixmapDrawing> moving_pixmaps;

#ifndef NDEBUG
    // properties for visualizing selected blocks, used only for debug
    std::optional<int> last_selected_block_page = {};
    std::optional<PagelessDocumentRect> last_selected_block = {};
#endif

    std::vector<DocumentRect> word_rects;
    std::vector<DocumentRect> synctex_highlights;
    QTime synctex_highlight_time;
    std::vector<MarkedDataRect> marked_data_rects;

    PdfViewWidget(DocumentView* document_view, PdfRenderer* pdf_renderer, ConfigManager* config_manager, bool is_helper, QWidget* parent = nullptr);
    ~PdfViewWidget();

    //void set_vertical_line_pos(float pos);
    //float get_vertical_line_pos();
    //void set_should_draw_vertical_line(bool val);
    //bool get_should_draw_vertical_line();
    void handle_escape();

    std::map<int, std::vector<MarkedDataRect>> get_marked_data_rect_map();
    void toggle_highlight_links();
    void set_highlight_links(bool should_highlight_links, bool should_show_numbers);
    void toggle_highlight_words();
    void set_highlight_words(std::vector<DocumentRect>& rects);
    void set_should_highlight_words(bool should_highlight);
    std::vector<DocumentRect> get_highlight_word_rects();

    bool on_vertical_scroll();
    int get_num_search_results();
    int get_current_search_result_index();
    bool valid_document();
    std::optional<SearchResult> set_search_result_offset(int offset);
    std::optional<SearchResult> get_current_search_result();
    void goto_search_result(int offset, bool overview = false);
    void render_overview(OverviewState overview);
    void render_page(int page_number, bool in_overview=false, ColorPalette forced_palette=ColorPalette::None, bool stencils_allowed=true);
    bool are_visible_pages_rendered();
    bool get_is_searching(float* prog);
    void search_text(const std::wstring& text, SearchCaseSensitivity case_sensitive = SearchCaseSensitivity::CaseInsensitive, bool regex = false, std::optional<std::pair<int, int>> range = {});
    void set_dark_mode(bool mode);
    void toggle_dark_mode();
    void set_custom_color_mode(bool mode);
    void toggle_custom_color_mode();
    void set_synctex_highlights(std::vector<DocumentRect> highlights);
    bool should_show_synxtex_highlights();
    bool has_synctex_timed_out();
    void on_document_view_reset();
    void mouseMoveEvent(QMouseEvent* mouse_event) override;
    void mousePressEvent(QMouseEvent* mevent) override;
    void mouseReleaseEvent(QMouseEvent* mevent) override;
    void wheelEvent(QWheelEvent* wevent) override;
    void register_on_link_edit_listener(std::function<void(const OpenedBookState&)> listener);
    void set_overview_page(std::optional<OverviewState> overview_page);
    std::optional<OverviewState> get_overview_page();
    void draw_empty_helper_message(QString message);
    NormalizedWindowRect get_overview_rect();
    NormalizedWindowRect get_overview_rect_pixel_perfect(int widget_width, int widget_height, int view_width, int view_height);
    std::vector<NormalizedWindowRect> get_overview_border_rects();
    bool is_window_point_in_overview(NormalizedWindowPos window_point);
    bool is_window_point_in_overview_border(NormalizedWindowPos window_point, OverviewSide* which_border);
    Document* doc(bool overview=false);

    void get_overview_offsets(float* offset_x, float* offset_y);
    void get_overview_size(float* width, float* height);

    float get_overview_side_pos(int index);
    void set_overview_side_pos(OverviewSide index, NormalizedWindowRect original_rect, fvec2 diff);
    void set_overview_rect(NormalizedWindowRect rect);

    void set_overview_offsets(float offset_x, float offset_y);
    void set_overview_offsets(fvec2 offsets);

    void bind_program(ColorPalette forced_palette=ColorPalette::None);
    void cancel_search();
    //void window_pos_to_overview_pos(float window_x, float window_y, float* doc_offset_x, float* doc_offset_y, int* doc_page);
    DocumentPos window_pos_to_overview_pos(NormalizedWindowPos window_pos);
    void rotate_clockwise();
    void rotate_counterclockwise();

    bool is_rotated();
    void toggle_fastread_mode();
    void setup_text_painter();
    void get_overview_window_vertices(float out_vertices[2 * 4]);

    void set_selected_rectangle(AbsoluteRect selected);
    void clear_selected_rectangle();
    void clear_all_selections();

    std::optional<AbsoluteRect> get_selected_rectangle();

    void set_typing_rect(DocumentRect rect, std::optional<DocumentRect> wrong_rect);

    Document* get_current_overview_document();
    NormalizedWindowPos document_to_overview_pos(DocumentPos pos);
    NormalizedWindowRect document_to_overview_rect(DocumentRect doc_rect);
    std::vector<int> get_visible_search_results(std::vector<int>& visible_pages);
    int find_search_index_for_visible_pages(std::vector<int>& visible_pages);
    int find_search_index_for_visible_page(int page, int breakpoint);
    int find_search_result_for_page_range(int page, int range_begin, int range_end);
    int find_search_results_breakpoint();
    int find_search_results_breakpoint_helper(int begin_index, int end_index);
    void get_custom_color_transform_matrix(float matrix_data[16]);
    void get_background_color(float out_background[3]);
    void set_underline(AbsoluteDocumentPos abspos);
    void clear_underline();
    void bind_points(const std::vector<float>& points);
    void bind_default();
    bool is_normalized_y_in_window(float y);
    bool is_normalized_y_range_in_window(float y0, float y1);
    void render_portal_rect(AbsoluteRect portal_absolute_rect, bool is_pending);
    void set_pending_download_portals(std::vector<AbsoluteRect>&& portal_rects);
    void show_rect_hints();
    void hide_rect_hints();
    bool is_showing_rect_hints();
    void get_color_for_current_mode(const float* input_color, float* output_color);
    void render_ui_icon_for_current_color_mode(const QIcon& icon_black, const QIcon& icon_white, QRect rect, bool is_highlighted=false);
    void render_text_highlights();
    void render_highlight_annotations();
    std::array<float, 3> cc3(const float* input_color);
    std::array<float, 4> cc4(const float* input_color);
    QColor qcc3(const float* input_color);
    QColor qcc4(const float* input_color);
    void set_overview_highlights(const std::vector<DocumentRect>& rects);
    bool needs_stencil_buffer();
    void draw_overview_background();
    void draw_overview_border();
    float get_overview_zoom_level();
    void zoom_overview(float scale);
    void zoom_in_overview();
    void zoom_out_overview();
    void render_selected_rectangle();
    void set_scratchpad(ScratchPad* pad);
    ScratchPad* get_scratchpad();
    bool can_use_cached_scratchpad_framebuffer();
    //void update_framebuffer_cache();
    void compile_drawings(DocumentView* dv, const std::vector<FreehandDrawing>& drawings);
    void clear_background_color();
    CompiledDrawingData compile_drawings_into_vertex_and_index_buffers(const std::vector<float>& line_coordinates,
        const std::vector<unsigned int>& indices,
        const std::vector<int>& line_type_indices,
        const std::vector<float>& dot_coordinates,
        const std::vector<unsigned int>& dot_indices,
        const std::vector<int>& dot_type_indices);
    void set_tag_prefix(std::wstring prefix);
    void clear_tag_prefix();
    void set_selected_highlight_index(int index);
    void set_selected_bookmark_index(int index);
    void set_highlighted_tags(std::vector<std::string> tags);
    bool is_tag_highlighted(const std::string& tag);
    void set_pending_portal_position(std::optional<AbsoluteRect> rect);
};
