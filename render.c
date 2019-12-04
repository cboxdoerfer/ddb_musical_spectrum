#include <math.h>
#include <gdk/gdk.h>
#include <stdint.h>
#include <fftw3.h>
#include <pango/pangocairo.h>
#include "render.h"
#include "config.h"
#include "utils.h"
#include "draw_utils.h"
#include "spectrum.h"

#define FONT_SIZE 9
#define FONT_PADDING_HORIZONTAL 8
#define FONT_PADDING_VERTICAL 0
#define NUM_NOTES_FOR_OCTAVE 12

struct spectrum_render_ctx_t {
    int num_bands;
    double band_width;
    double note_width;
    // Spectrum rectangle
    cairo_rectangle_t r_s;
    // Left labels rectangle
    cairo_rectangle_t r_l;
    // Right labels rectangle
    cairo_rectangle_t r_r;
    // Top labels rectangle
    cairo_rectangle_t r_t;
    // Bottom labels rectangle
    cairo_rectangle_t r_b;
    // Font layout
    PangoLayout *font_layout;
};

void
spectrum_data_free (struct spectrum_data_t *data)
{
    if (!data) {
        return;
    }
    if (data->samples) {
        free (data->samples);
        data->samples = NULL;
    }
    if (data->spectrum) {
        free (data->spectrum);
        data->spectrum = NULL;
    }
    if (data->window) {
        free (data->window);
        data->window = NULL;
    }
    if (data->frequency) {
        free (data->frequency);
        data->frequency = NULL;
    }
    if (data->keys) {
        free (data->keys);
        data->keys = NULL;
    }
    if (data->low_res_indices) {
        free (data->low_res_indices);
        data->low_res_indices = NULL;
    }
    if (data->fft_in) {
        fftw_free (data->fft_in);
        data->fft_in = NULL;
    }
    if (data->fft_out) {
        fftw_free (data->fft_out);
        data->fft_out = NULL;
    }
    if (data->fft_plan) {
        fftw_destroy_plan (data->fft_plan);
        data->fft_plan = NULL;
    }
    if (data->mutex) {
        deadbeef->mutex_free (data->mutex);
        data->mutex = 0;
    }
    free (data);
    data = NULL;
}

void
spectrum_render_free (struct spectrum_render_t *render)
{
    if (!render) {
        return;
    }
    if (render->bars) {
        free (render->bars);
        render->bars = NULL;
    }
    if (render->bars_peak) {
        free (render->bars_peak);
        render->bars_peak = NULL;
    }
    if (render->peaks) {
        free (render->peaks);
        render->peaks = NULL;
    }
    if (render->v_bars) {
        free (render->v_bars);
        render->v_bars = NULL;
    }
    if (render->v_peaks) {
        free (render->v_peaks);
        render->v_peaks = NULL;
    }
    if (render->delay_bars) {
        free (render->delay_bars);
        render->delay_bars = NULL;
    }
    if (render->delay_peaks) {
        free (render->delay_peaks);
        render->delay_peaks = NULL;
    }
    free (render);
    render = NULL;
}

struct spectrum_data_t *
spectrum_data_new (void)
{
    struct spectrum_data_t *s_data = calloc (1, sizeof (struct spectrum_data_t));
    s_data->samples = calloc (MAX_FFT_SIZE * DDB_FREQ_MAX_CHANNELS, sizeof (double));
    s_data->spectrum = calloc (MAX_FFT_SIZE, sizeof (double));
    s_data->window = calloc (MAX_FFT_SIZE, sizeof (double));
    s_data->frequency = calloc (MAX_FFT_SIZE, sizeof (double));
    s_data->keys = calloc (MAX_FFT_SIZE, sizeof (int));
    s_data->low_res_indices = calloc (MAX_FFT_SIZE, sizeof (int));
    s_data->fft_in = fftw_alloc_real (MAX_FFT_SIZE);
    s_data->fft_out = fftw_alloc_complex (MAX_FFT_SIZE);
    s_data->fft_plan = fftw_plan_dft_r2c_1d (CLAMP (CONFIG_FFT_SIZE, 512, MAX_FFT_SIZE), s_data->fft_in, s_data->fft_out, FFTW_ESTIMATE);
    s_data->mutex = deadbeef->mutex_create ();
    return s_data;
}

struct spectrum_render_t *
spectrum_render_new (void)
{
    struct spectrum_render_t *render = calloc (1, sizeof (struct spectrum_render_t));
    render->bars = calloc (MAX_FFT_SIZE, sizeof (double));
    render->bars_peak = calloc (MAX_FFT_SIZE, sizeof (double));
    render->peaks = calloc (MAX_FFT_SIZE, sizeof (double));
    render->v_bars = calloc (MAX_FFT_SIZE, sizeof (double));
    render->v_peaks = calloc (MAX_FFT_SIZE, sizeof (double));
    render->delay_bars = calloc (MAX_FFT_SIZE, sizeof (int));
    render->delay_peaks = calloc (MAX_FFT_SIZE, sizeof (int));
    return render;
}

static int
get_db_range ()
{
    return CONFIG_AMPLITUDE_MAX - CONFIG_AMPLITUDE_MIN;
}

static int
get_align_pos (int width, int spectrum_width)
{
    int left = 0;
    switch (CONFIG_ALIGNMENT) {
        case LEFT:
            left = 0;
            break;
        case RIGHT:
            left = MIN (width, width - spectrum_width);
            break;
        case CENTER:
            left = MAX (0, (width - spectrum_width)/2);
            break;
        default:
            left = 0;
            break;
    }
    return left;
}

static void
do_fft (struct spectrum_data_t *s)
{
    if (!s->samples || !s->fft_plan) {
        return;
    }

    deadbeef->mutex_lock (s->mutex);
    for (int i = 0; i < MAX_FFT_SIZE; ++i) {
        s->spectrum[i] = -DBL_MAX;
    }

    for (int ch = 0; ch < s->num_channels; ++ch) {
        for (int i = 0; i < CONFIG_FFT_SIZE; i++) {
            s->fft_in[i] = s->samples[i * s->num_channels + ch] * s->window[i];
        }

        fftw_execute (s->fft_plan);
        for (int i = 0; i < CONFIG_FFT_SIZE/2; i++) {
            const double real = s->fft_out[i][0];
            const double imag = s->fft_out[i][1];
            const double mag = 10.0 * log10 (4.0 * (real*real + imag*imag)/ (double)(CONFIG_FFT_SIZE*CONFIG_FFT_SIZE));
            s->spectrum[i] = MAX (mag, s->spectrum[i]);
        }
    }
    deadbeef->mutex_unlock (s->mutex);
}

static inline double
spectrum_get_value (w_spectrum_t *w, int band, int num_bands)
{
    band = MAX (band, 1);
    double k0 = w->data->keys[MAX(band - 1, w->data->low_res_end)];
    double k1 = w->data->keys[band];
    double k2 = w->data->keys[MIN(band + 1, num_bands -1)];

    int start = ceil((k1 - k0)/2.0 + k0);
    int end = ceil((k2 - k1)/2.0 + k1);

    if (start >= end) {
        return w->data->spectrum[end];
    }
    double value = -DBL_MAX;
    for (int i = start; i < end; i++) {
        value = MAX (w->data->spectrum[i] ,value);
    }
    return value;
}

static void
spectrum_band_gravity (double *peaks,
                       double *bars,
                       double *velocities,
                       double d_velocity,
                       int *delays,
                       int delay,
                       int band)
{
    if (peaks[band] > bars[band]) {
        if (delays[band] < 0) {
            peaks[band] -= velocities[band] * CONFIG_REFRESH_INTERVAL;
            velocities[band] += d_velocity;
        }
        else {
            delays[band]--;
        }
    }

    if (peaks[band] <= bars[band]) {
        peaks[band] = bars[band];
        delays[band] = delay;
        velocities[band] = 0;
    }
}

static void
spectrum_band_set (struct spectrum_render_t *r, int playback_status, double amplitude, int band)
{
    r->bars[band] = CLAMP (amplitude - CONFIG_AMPLITUDE_MIN, -DBL_MAX, get_db_range ());

    const int state = deadbeef->get_output ()->state ();
    if (state == OUTPUT_STATE_PLAYING) {
        if (CONFIG_ENABLE_PEAKS && CONFIG_PEAK_FALLOFF != -1) {
            // peak gravity
            spectrum_band_gravity (r->peaks, r->bars, r->v_peaks, r->peak_velocity, r->delay_peaks, r->peak_delay, band);
        }
        if (CONFIG_ENABLE_AMPLITUDES && CONFIG_BAR_FALLOFF != -1) {
            // bar gravity
            spectrum_band_gravity (r->bars_peak, r->bars, r->v_bars, r->bar_velocity, r->delay_bars, r->bar_delay, band);
            r->bars[band] = r->bars_peak[band];
        }
    }
}

static void
spectrum_bands_fill (w_spectrum_t *w, int num_bands, int playback_status)
{
    const int low_res_end = w->data->low_res_indices_num;

    int *x = w->data->low_res_indices;
    double y[low_res_end + 1];

    for (int i = 0; i < low_res_end; i++) {
        y[i] = w->data->spectrum[w->data->keys[x[i]]];
    }

    int xx = 0;
    // Interpolate
    for (int i = 0; i < low_res_end; i++) {
        int i_end = MIN (i + 1, low_res_end - 1);
        for (xx = x[i]; xx < x[i_end]; xx++) {
            const double mu = (double)(xx - x[i]) / (double)(x[i_end] - x[i]);
            const double amp = hermite_interpolate (y, mu, i-1, 0.35, 0);
            spectrum_band_set (w->render, playback_status, amp, xx);
        }
    }
    // Fill the rest of the bands which don't need to be interpolated
    for (int i = xx; i < num_bands; ++i) {
        const double amp = spectrum_get_value (w, i, num_bands);
        spectrum_band_set (w->render, playback_status, amp, i);
    }
}

static void
spectrum_render (w_spectrum_t *w, int num_bands)
{
    const int state = deadbeef->get_output()->state();
    if (state != OUTPUT_STATE_STOPPED) {
        if (state == OUTPUT_STATE_PAUSED) {
            spectrum_remove_refresh_interval (w);
        }
        do_fft (w->data);
        spectrum_bands_fill (w, num_bands, w->playback_status);
    }
    else {
        spectrum_remove_refresh_interval (w);
        struct spectrum_render_t *r = w->render;
        for (int i = 0; i < num_bands; i++) {
                r->bars[i] = -DBL_MAX;
                r->v_bars[i] = 0;
                r->delay_bars[i] = 0;
                r->bars_peak[i] = 0;
                r->peaks[i] = 0;
                r->v_peaks[i] = 0;
                r->delay_peaks[i] = 0;
        }
    }
}

static int
is_full_step (int r)
{
    switch (r) {
        case 0:
        case 2:
        case 4:
        case 5:
        case 7:
        case 9:
        case 11:
            return 1;
        default:
            return 0;
    }
}

static void
spectrum_draw_cairo_static (w_spectrum_t *w, cairo_t *cr, double barw, int bands, cairo_rectangle_t *r)
{
    cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
    cairo_set_line_width (cr, 1);

    double y = r->y;
    double x = r->x;
    double height = r->height;
    if (CONFIG_ENABLE_WHITE_KEYS || CONFIG_ENABLE_BLACK_KEYS) {
        int num_notes = get_num_notes ();
        for (int i = 0; i < num_notes; i++, x += barw) {
            int r = (CONFIG_NOTE_MIN + i) % 12;
            if (is_full_step (r)) {
                if (!CONFIG_ENABLE_WHITE_KEYS) {
                    continue;
                }
                gdk_cairo_set_source_color (cr, &CONFIG_COLOR_WHITE_KEYS);
            }
            else {
                if (!CONFIG_ENABLE_BLACK_KEYS) {
                    continue;
                }
                gdk_cairo_set_source_color (cr, &CONFIG_COLOR_BLACK_KEYS);
            }
            cairo_rectangle (cr, x, y, barw - 1, height);
            cairo_fill (cr);
        }
    }

    const double octave_width = barw * NUM_NOTES_FOR_OCTAVE;

    // draw octave grid
    if (CONFIG_ENABLE_VGRID) {
        int offset = CONFIG_NOTE_MIN % NUM_NOTES_FOR_OCTAVE;
        gdk_cairo_set_source_color (cr, &CONFIG_COLOR_VGRID);
        double x = r->x + (NUM_NOTES_FOR_OCTAVE - offset) * barw;
        while (x < r->x + r->width) {
            cairo_move_to (cr, x, r->y);
            cairo_rel_line_to (cr, 0, r->height);
            x += octave_width;
        }
        cairo_stroke (cr);
    }

    // draw horizontal grid
    const int hgrid_num = get_db_range()/10;
    if (CONFIG_ENABLE_HGRID && r->height > 2*hgrid_num && r->width > 1) {
        gdk_cairo_set_source_color (cr, &CONFIG_COLOR_HGRID);
        for (int i = 0; i <= hgrid_num; i++) {
            cairo_move_to (cr, r->x - 3, r->y + floor (i/(double)hgrid_num * r->height));
            cairo_rel_line_to (cr, r->width + 6, 0);
        }
        cairo_stroke (cr);
    }

    // draw octave grid on hover
    if (CONFIG_ENABLE_OGRID && w->motion_ctx.entered) {
        int dx = (int)(w->motion_ctx.x - r->x);
        if (dx >= 0 && dx <= r->width) {
            int octave_offset = dx % (int)octave_width;
            int band_offset = dx % (int)barw;
            gdk_cairo_set_source_color (cr, &CONFIG_COLOR_OGRID);
            double x = octave_offset + r->x - band_offset;
            while (x <= r->width + r->x) {
                cairo_move_to (cr, x, r->y);
                cairo_rel_line_to (cr, 0, r->height);
                x += octave_width;
            }
            cairo_stroke (cr);
        }
    }
}

static void
spectrum_background_draw (cairo_t *cr, double width, double height)
{
    gdk_cairo_set_source_color (cr, &CONFIG_COLOR_BG);
    cairo_rectangle (cr, 0, 0, width, height);
    cairo_fill (cr);
}

static void
spectrum_draw_cairo_bars (struct spectrum_render_t *render, cairo_t *cr, int num_bars, int barw, cairo_rectangle_t *r)
{
    if (r->height <= 0) {
        return;
    }
    cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
    cairo_set_line_width (cr, 1);
    const double base_s = (r->height / (double)get_db_range ());

    // draw spectrum

    // draw bars
    spectrum_gradient_set (cr, CONFIG_GRADIENT_COLORS, r->width, r->height);


    double x = r->x;
    double bar_width = barw;
    if (CONFIG_GAPS && barw > 1) {
        bar_width -= 1;
    }
    double bar_offset = 0;
    if (CONFIG_SPACING && bar_width > 4) {
        bar_offset = 1;
        bar_width -= 2;
    }

    if (CONFIG_ENABLE_BAR_MODE) { 
        for (int i = 0; i < num_bars; i++, x += barw) {
            int y0 = r->y + r->height;
            for (int y = y0; y > y0 - render->bars[i] * base_s; y -= 2) {
                cairo_move_to (cr, x + bar_offset, y); 
                cairo_rel_line_to (cr, bar_width, 0);
            }
            cairo_stroke (cr);
        }
    }
    else {
        for (int i = 0; i < num_bars; i++, x += barw) {
            if (render->bars[i] <= 0) {
                continue;
            }
            cairo_rectangle (cr, x + bar_offset, r->y + r->height - 1, bar_width, - render->bars[i] * base_s);
        }

        if (CONFIG_FILL_SPECTRUM) {
            cairo_fill (cr);
        }
        else {
            cairo_stroke (cr);
        }
    }

    // draw peaks
    if (CONFIG_ENABLE_PEAKS && CONFIG_PEAK_FALLOFF >= 0) {
        if (CONFIG_ENABLE_PEAKS_COLOR) {
            gdk_cairo_set_source_color (cr, &CONFIG_COLOR_PEAKS);
        }
        x = r->x;
        for (gint i = 0; i < num_bars; i++, x += barw) {
            if (render->peaks[i] <= 0) {
                continue;
            }
            const double y = r->y + CLAMP (r->height - render->peaks[i] * base_s, 0, r->height - 1);
            cairo_move_to (cr, x + bar_offset, y); 
            cairo_rel_line_to (cr, bar_width, 0);
        }
        cairo_stroke (cr);
    }

}

static void
spectrum_draw_cairo (struct spectrum_render_t *render, cairo_t *cr, int bands, cairo_rectangle_t *r)
{
    if (r->height <= 0) {
        return;
    }
    const double base_s = (r->height / (double)get_db_range ());
    const int barw = CLAMP (floor(r->width / bands), 2, 20) - 1;

    // draw spectrum
    spectrum_gradient_set (cr, CONFIG_GRADIENT_COLORS, r->width, r->height);

    cairo_set_antialias (cr, CAIRO_ANTIALIAS_BEST);
    cairo_set_line_width (cr, 1);
    cairo_move_to (cr, r->x, r->height);
    double py = r->height - base_s * render->bars[0];
    cairo_rel_line_to (cr, 0, py);
    for (gint i = 0; i < bands; i++)
    {
        const double x = r->x + barw * i;
        const double y = r->height - base_s * MAX (render->bars[i],0);

        if (!CONFIG_FILL_SPECTRUM) {
            cairo_move_to (cr, x - 0.5, py);
        }
        cairo_line_to (cr, x + 0.5, y);
        py = y;
    }
    if (CONFIG_FILL_SPECTRUM) {
        cairo_line_to (cr, r->x + r->width, r->height);
        cairo_rel_line_to (cr, r->width, 0);

        cairo_close_path (cr);
        cairo_fill (cr);
    }
    else {
        cairo_stroke (cr);
    }

    if (CONFIG_ENABLE_PEAKS && CONFIG_PEAK_FALLOFF >= 0) {
        if (CONFIG_ENABLE_PEAKS_COLOR) {
            gdk_cairo_set_source_color (cr, &CONFIG_COLOR_PEAKS);
        }
        for (int i = 0; i < bands; i += 2) {
            const double x = r->x + barw * i;
            const double y = CLAMP (r->height - render->peaks[i] * base_s, 0, r->height);
            cairo_move_to (cr, x, y); 
            cairo_rel_line_to (cr, barw, 0);
        }
        cairo_stroke (cr);
    }

}

static double
spectrum_pango_font_height (PangoLayout *layout, const char *text)
{
    int f_h = 0;
    pango_layout_set_text (layout, text, -1);
    pango_layout_get_size (layout, NULL, &f_h);
    return (double)f_h/PANGO_SCALE;
}

static double
spectrum_pango_font_width (PangoLayout *layout, const char *text)
{
    int f_w = 0;
    pango_layout_set_text (layout, text, -1);
    pango_layout_get_size (layout, &f_w, NULL);
    return (double)f_w/PANGO_SCALE;
}

static double
spectrum_font_height_max (PangoLayout *layout)
{
    return spectrum_pango_font_height (layout, "C#11");
}

static double
spectrum_font_width_max (PangoLayout *layout)
{
    const int hgrid_num = get_db_range ()/10;
    char s[100] = "";
    double w_max = 0;
    for (int i = 1; i < hgrid_num; i++) {
        snprintf (s, sizeof (s), "%ddB", CONFIG_AMPLITUDE_MAX - i * 10);
        w_max = MAX (w_max, spectrum_pango_font_width (layout, s));
    }

    return w_max;
}

static void
spectrum_draw_labels_freq (cairo_t *cr, struct spectrum_render_ctx_t *r_ctx, cairo_rectangle_t *r)
{
    const double note_width = r_ctx->note_width;
    const double f_w_full = spectrum_pango_font_width (r_ctx->font_layout, "D") + 2;
    const double f_w_half = spectrum_pango_font_width (r_ctx->font_layout, "C#") + 2;
    const double y = r->y + FONT_PADDING_VERTICAL/2;
    cairo_move_to (cr, r->x, y);
    char s[100] = "";

    char *note_array[] = {
        "C","C#","D","D#","E","F","F#","G","G#","A","A#","B",
    };


    double x = r->x + note_width/2;
    double x_end = r->x + r->width;

    int show_full_steps = note_width > f_w_full ? 1 : 0;
    int show_half_steps = note_width > f_w_half ? 1 : 0;
    for (int i = CONFIG_NOTE_MIN; i <= CONFIG_NOTE_MAX && x < x_end; i++, x += note_width) {
        int r = i % 12; 
        if (i == 0 || r == 0) {
            snprintf (s, sizeof (s), "%s", spectrum_notes[i]);
            gdk_cairo_set_source_color (cr, &CONFIG_COLOR_TEXT);
        }
        else if (show_full_steps && is_full_step (r)) {
            snprintf (s, sizeof (s), "%s", note_array[r]);
            cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, 1);
        }
        else if (show_half_steps) {
            snprintf (s, sizeof (s), "%s", note_array[r]);
            cairo_set_source_rgba (cr, 0.3, 0.3, 0.3, 1);
        }
        else {
            continue;
        }

        double font_width = spectrum_pango_font_width (r_ctx->font_layout, s);
        double xx = x - font_width/2;
        cairo_move_to (cr, xx, y);
        pango_layout_set_text (r_ctx->font_layout, s, -1);
        pango_cairo_show_layout (cr, r_ctx->font_layout);
    }
}

static void
spectrum_draw_labels_db (cairo_t *cr, struct spectrum_render_ctx_t *r_ctx, cairo_rectangle_t *r)
{
    const int hgrid_num = get_db_range ()/10;
    pango_layout_set_text (r_ctx->font_layout, "-100dB", -1);
    double font_height = spectrum_pango_font_height (r_ctx->font_layout, "-100dB");
    if (CONFIG_ENABLE_HGRID && r->height > 2*hgrid_num && r->width > 1) {
        gdk_cairo_set_source_color (cr, &CONFIG_COLOR_TEXT);
        cairo_move_to (cr, r->x, r->y);
        char s[100] = "";
        for (int i = 0; i <= hgrid_num; i++) {
            snprintf (s, sizeof (s), "%ddB", CONFIG_AMPLITUDE_MAX - i * 10);
            cairo_move_to (cr, r->x + FONT_PADDING_HORIZONTAL/2, r->y + i/(double)hgrid_num * r->height - font_height/2);
            pango_layout_set_text (r_ctx->font_layout, s, -1);
            pango_cairo_show_layout (cr, r_ctx->font_layout);
        }
    }
}

static int
spectrum_bar_width_get (int num_bands, double width)
{
    int barw = 0;
    if (CONFIG_DRAW_STYLE) {
        barw = 1;
    }
    //else if (CONFIG_GAPS || CONFIG_BAR_W > 1) {
    //    barw = CLAMP (width / num_bands, 2, 100);
    //}
    else {
        barw = CLAMP (width / num_bands, 2, 100);
    }
    return barw;
}

static int
spectrum_width_max (PangoLayout *layout, double width)
{
    const double font_width = spectrum_font_width_max (layout);
    const double label_width = font_width + FONT_PADDING_HORIZONTAL;
    const double labels_width = 2 * label_width;
    return MAX (width - labels_width, 0);
}

static void
spectrum_draw_tooltip (struct spectrum_render_t *render,
                       struct spectrum_data_t *data,
                       cairo_t *cr,
                       struct spectrum_render_ctx_t *r_ctx,
                       struct motion_context *m_ctx)
{
    cairo_rectangle_t *r = &r_ctx->r_s;

    const double band_width = r_ctx->band_width;
    const double note_width = r_ctx->note_width;

    const double x_bar = m_ctx->x - r->x;
    const int pos = (int)(x_bar/note_width + CONFIG_NOTE_MIN);
    const int freq_pos = (int)(x_bar/band_width);

    if (pos < CONFIG_NOTE_MIN || pos > CONFIG_NOTE_MAX
        || freq_pos < 0 || freq_pos >= r_ctx->num_bands) {
        return;
    }

    char t1[100];
    const double amp = render->bars[pos];
    if (amp > -1000 && amp < 1000) {
        snprintf (t1, sizeof (t1), "%.0f Hz (%s)\n%3.2f dB", data->frequency[freq_pos], spectrum_notes[pos], render->bars[pos] + CONFIG_AMPLITUDE_MIN);
    }
    else {
        snprintf (t1, sizeof (t1), "%.0f Hz (%s)\n-inf dB", data->frequency[freq_pos], spectrum_notes[pos]);
    }

    cairo_save (cr);

    double text_width = spectrum_pango_font_width (r_ctx->font_layout, t1);
    double text_height = spectrum_pango_font_height (r_ctx->font_layout, t1);

    const double padding = 5;
    double x = m_ctx->x + 20;
    double y = m_ctx->y + 20;
    const double w_rect = text_width + 2 * padding;
    const double h_rect = text_height + 2 * padding;
    y = CLAMP (y, r->y, r->y + r->height - h_rect);
    x = CLAMP (x, r->x, r->x + r->width - w_rect);
    const double x_rect = x - padding;
    const double y_rect = y - padding;

    cairo_set_source_rgba (cr, 0, 0, 0, 1);
    cairo_rectangle (cr, x_rect, y_rect, w_rect, h_rect);
    cairo_fill (cr);

    cairo_set_source_rgba (cr, 0.9, 0.9, 0.9, 1);
    cairo_set_line_width (cr, 2.0);
    cairo_rectangle (cr, x_rect, y_rect, w_rect, h_rect);
    cairo_stroke (cr);

    gdk_cairo_set_source_color (cr, &CONFIG_COLOR_TEXT);
    cairo_move_to (cr, x, y);
    pango_layout_set_text (r_ctx->font_layout, t1, -1);
    pango_cairo_show_layout (cr, r_ctx->font_layout);

    cairo_restore (cr);
}

struct spectrum_render_ctx_t
spectrum_get_render_ctx (cairo_t *cr, double width, double height)
{
    PangoLayout *layout = pango_cairo_create_layout (cr);

    PangoFontDescription *desc = pango_font_description_from_string (CONFIG_FONT);
    pango_layout_set_font_description (layout, desc);
    pango_font_description_free (desc);

    const double font_width = spectrum_font_width_max (layout);
    const double font_height = spectrum_font_height_max (layout);
    const double label_height = font_height + FONT_PADDING_VERTICAL;
    const double label_width = font_width + FONT_PADDING_HORIZONTAL;

    const int num_bands = !CONFIG_DRAW_STYLE ? get_num_notes () : get_num_bars ();
    double labels_width = (CONFIG_ENABLE_RIGHT_LABELS + CONFIG_ENABLE_LEFT_LABELS) * label_width;
    double labels_height = (CONFIG_ENABLE_TOP_LABELS + CONFIG_ENABLE_BOTTOM_LABELS) * label_height;
    int bar_width = spectrum_bar_width_get (num_bands, width - labels_width);
    double spectrum_width = bar_width * num_bands;
    double spectrum_height = height - labels_height;;
    const int x0 = get_align_pos (width, spectrum_width + labels_width);

    cairo_rectangle_t r_l, r_r, r_t, r_b, r_s;

    r_l.x = x0;
    r_l.width = label_width * CONFIG_ENABLE_LEFT_LABELS;

    r_s.x = r_l.x + r_l.width;
    r_s.width = spectrum_width;
    r_s.height = spectrum_height;

    r_r.x = r_s.x + r_s.width;
    r_r.width = label_width * CONFIG_ENABLE_RIGHT_LABELS;

    r_t.x = r_s.x;
    r_t.width = r_s.width;

    r_b.x = r_t.x;
    r_b.width = r_t.width;

    r_t.y = 0;
    r_t.height = label_height * CONFIG_ENABLE_TOP_LABELS;

    r_s.y = r_t.y + r_t.height;

    r_b.y = r_s.y + r_s.height;
    r_b.height = r_t.height;

    r_l.y = r_s.y;
    r_l.height = r_s.height;

    r_r.y = r_s.y;
    r_r.height = r_s.height;

    struct spectrum_render_ctx_t r_ctx = {
        .num_bands = num_bands,
        .band_width = (double)spectrum_width / num_bands,
        .note_width = (double)spectrum_width / (double)(get_num_notes ()),
        .r_r = r_r,
        .r_l = r_l,
        .r_t = r_t,
        .r_b = r_b,
        .r_s = r_s,
        .font_layout = layout,
    };

    return r_ctx;
}

int
draw_labels (cairo_t *cr, int width, int height)
{
    double x1, y1, x2, y2;
    cairo_clip_extents (cr, &x1, &y1, &x2, &y2);

    int clip_width = x2 - x1;
    int clip_height = y2 - y1;

    if (clip_width != width || clip_height != height) {
        return 0;
    }
    return 1;
}

gboolean
spectrum_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    w_spectrum_t *w = user_data;

    GtkAllocation a;
    gtk_widget_get_allocation (w->drawarea, &a);
    const int width = a.width;
    const int height = a.height;

    struct spectrum_render_ctx_t r_ctx = spectrum_get_render_ctx (cr, width, height);
    w->spectrum_rectangle = r_ctx.r_s;

    if (width != w->prev_width || w->need_redraw) {
        if (w->need_redraw == 1) {
            w->need_redraw = 0;
        }
        create_frequency_table(w->data, w->samplerate, spectrum_width_max (r_ctx.font_layout, width));
    }
    w->prev_width = width;
    w->prev_height = height;


    // draw background
    spectrum_background_draw (cr, width, height);

    spectrum_render (w, get_num_bars ());

    spectrum_draw_cairo_static (w, cr, r_ctx.note_width, r_ctx.num_bands, &r_ctx.r_s);
    if (!CONFIG_DRAW_STYLE) {
        spectrum_draw_cairo_bars (w->render, cr, r_ctx.num_bands, r_ctx.note_width, &r_ctx.r_s);
    }
    else {
        spectrum_draw_cairo (w->render, cr, r_ctx.num_bands, &r_ctx.r_s);
    }

    if (draw_labels (cr, width, height)) {
        if (CONFIG_ENABLE_TOP_LABELS) {
            spectrum_draw_labels_freq (cr, &r_ctx, &r_ctx.r_t);
        }
        if (CONFIG_ENABLE_BOTTOM_LABELS) {
            spectrum_draw_labels_freq (cr, &r_ctx, &r_ctx.r_b);
        }
        if (CONFIG_ENABLE_LEFT_LABELS) {
            spectrum_draw_labels_db (cr, &r_ctx, &r_ctx.r_l);
        }
        if (CONFIG_ENABLE_RIGHT_LABELS) {
            spectrum_draw_labels_db (cr, &r_ctx, &r_ctx.r_r);
        }
    }

    if (CONFIG_ENABLE_TOOLTIP && w->motion_ctx.entered) {
        spectrum_draw_tooltip (w->render, w->data, cr, &r_ctx, &w->motion_ctx);
    }

    g_object_unref (r_ctx.font_layout);

    return FALSE;
}

