/* Regression coverage for the pinned MuPDF cleanup paths, with sanitizers on. */
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <stdio.h>
#include <stdlib.h>

/* More than 255 coordinates forces an open packed path (with pointer members).
 * A first grayscale fill puts it at a 4-byte offset in an unpadded display list,
 * reproducing the Type 3 glyph alignment failure without a private PDF.
 */
static void check_packed_paths(fz_context *ctx)
{
	fz_display_list *list = NULL;
	fz_device *device = NULL;
	fz_path *path = NULL;
	fz_pixmap *pixels = NULL;
	const fz_rect bounds = {0, 0, 64, 64};
	const float black = 0;
	fz_var(list);
	fz_var(device);
	fz_var(path);
	fz_var(pixels);
	fz_try(ctx)
	{
		list = fz_new_display_list(ctx, bounds);
		device = fz_new_list_device(ctx, list);
		path = fz_new_path(ctx);
		for (int i = 0; i < 100; ++i)
		{
			float x = 2 + (i % 10) * 6;
			float y = 2 + (i / 10) * 6;
			fz_rectto(ctx, path, x, y, x + 3, y + 3);
		}
		fz_fill_path(ctx, device, path, 0, fz_identity, fz_device_gray(ctx), &black, 1, fz_default_color_params);
		fz_drop_path(ctx, path);
		path = NULL;
		/* Follow the pointer-bearing path with a small flat packed path. */
		path = fz_new_path(ctx);
		fz_rectto(ctx, path, 61, 61, 63, 63);
		fz_fill_path(ctx, device, path, 0, fz_identity, fz_device_gray(ctx), &black, 1, fz_default_color_params);
		fz_close_device(ctx, device);
		fz_drop_device(ctx, device);
		device = NULL;
		pixels = fz_new_pixmap(ctx, fz_device_rgb(ctx), 64, 64, NULL, 0);
		fz_clear_pixmap_with_value(ctx, pixels, 255);
		device = fz_new_draw_device(ctx, fz_identity, pixels);
		fz_run_display_list(ctx, list, device, fz_identity, bounds, NULL);
		fz_close_device(ctx, device);
		const unsigned char *samples = fz_pixmap_samples(ctx, pixels);
		int stride = fz_pixmap_stride(ctx, pixels);
		int components = fz_pixmap_components(ctx, pixels);
		if (samples[0] != 255 || samples[3 * stride + 3 * components] != 0 ||
			samples[62 * stride + 62 * components] != 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "packed display-list paths replayed incorrectly");
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, device);
		fz_drop_path(ctx, path);
		fz_drop_pixmap(ctx, pixels);
		fz_drop_display_list(ctx, list);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

int main(int argc, char **argv)
{
	if (argc < 2 || argc > 3)
	{
		fprintf(stderr, "Usage: %s document.pdf [first-page search text; empty to skip]\n", argv[0]);
		return 1;
	}
	const char *needle = argc == 3 ? argv[2] : "sioyek";
	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	fz_archive *archive = NULL;
	fz_document *document = NULL;
	fz_pixmap *pixmap = NULL;
	fz_stext_page *text = NULL;
	fz_stext_options text_options = {0};
	int failed = 0;
	int pages = 0;
	int current_page = 0;
	if (!ctx)
		return 1;
	fz_var(archive);
	fz_var(document);
	fz_var(pixmap);
	fz_var(text);
	fz_var(failed);
	fz_var(pages);
	fz_var(current_page);
	fz_try(ctx)
	{
		check_packed_paths(ctx);
		text_options.flags = FZ_STEXT_PRESERVE_IMAGES;
#define CHECK_NULL(name) \
		if (fz_keep_##name(ctx, NULL) != NULL) \
			fz_throw(ctx, FZ_ERROR_GENERIC, "keeping null " #name " must return null"); \
		fz_drop_##name(ctx, NULL)
		CHECK_NULL(bitmap);
		CHECK_NULL(buffer);
		CHECK_NULL(compressed_buffer);
		CHECK_NULL(colorspace);
		CHECK_NULL(default_colorspaces);
		CHECK_NULL(device);
		CHECK_NULL(display_list);
		CHECK_NULL(document);
		CHECK_NULL(font);
		CHECK_NULL(halftone);
		CHECK_NULL(image);
		CHECK_NULL(link);
		CHECK_NULL(outline);
		CHECK_NULL(page);
		CHECK_NULL(path);
		CHECK_NULL(pixmap);
		CHECK_NULL(separations);
		CHECK_NULL(shade);
		CHECK_NULL(stext_page);
		CHECK_NULL(storable);
		CHECK_NULL(stream);
		CHECK_NULL(stroke_state);
		CHECK_NULL(string);
		CHECK_NULL(text);
#undef CHECK_NULL
		if (fz_keep_archive(ctx, NULL) != NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "keeping a null archive must return null");
		fz_drop_archive(ctx, NULL);
		archive = fz_new_tree_archive(ctx, NULL);
		fz_drop_archive(ctx, fz_keep_archive(ctx, archive));
		fz_drop_archive(ctx, archive);
		archive = NULL;
		fz_register_document_handlers(ctx);
		document = fz_open_document(ctx, argv[1]);
		(void) pdf_count_layers(ctx, pdf_specifics(ctx, document));
		pages = fz_count_pages(ctx, document);
		if (pages < 1)
			fz_throw(ctx, FZ_ERROR_GENERIC, "document has no pages");
		pixmap = fz_new_pixmap_from_page_number(ctx, document, 0, fz_scale(.5f, .5f),
			fz_device_rgb(ctx), 0);
		if (fz_pixmap_width(ctx, pixmap) < 1 || fz_pixmap_height(ctx, pixmap) < 1)
			fz_throw(ctx, FZ_ERROR_GENERIC, "document rendering is empty");
		text = fz_new_stext_page_from_page_number(ctx, document, 0, &text_options);
		fz_quad hits[16];
		if (needle[0] && fz_search_stext_page(ctx, text, needle, NULL, hits, 16) < 1)
			fz_throw(ctx, FZ_ERROR_GENERIC, "first-page search has no matches");
		for (int page = 1; page < pages; ++page)
		{
			current_page = page;
			if (getenv("SIOYEK_MUPDF_TEST_TRACE"))
				fprintf(stderr, "Rendering page %d/%d\n", current_page + 1, pages);
			fz_drop_pixmap(ctx, pixmap);
			pixmap = NULL;
			pixmap = fz_new_pixmap_from_page_number(ctx, document, page, fz_scale(.5f, .5f),
				fz_device_rgb(ctx), 0);
			fz_drop_stext_page(ctx, text);
			text = NULL;
			text = fz_new_stext_page_from_page_number(ctx, document, page, &text_options);
		}
	}
	fz_always(ctx)
	{
		fz_drop_archive(ctx, archive);
		fz_drop_stext_page(ctx, text);
		fz_drop_pixmap(ctx, pixmap);
		fz_drop_document(ctx, document);
	}
	fz_catch(ctx)
	{
		fprintf(stderr, "MuPDF runtime check (page %d): %s\n", current_page + 1, fz_caught_message(ctx));
		failed = 1;
	}
	fz_drop_context(ctx);
	if (!failed)
		printf("PASS: MuPDF packed paths, cleanup/reference counting, %d rendered/text-extracted pages; search %s\n",
			pages, needle[0] ? "matched" : "skipped (explicit empty query)");
	return failed;
}
