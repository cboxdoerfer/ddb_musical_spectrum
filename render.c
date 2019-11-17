#include <math.h>
#include <gdk/gdk.h>
#include <stdint.h>
#include <fftw3.h>
#include "render.h"
#include "config.h"
#include "utils.h"
#include "draw_utils.h"
#include "spectrum.h"

#define FONT_SIZE 9
#define FONT_PADDING 8
#define NUM_OCTAVES 11
#define NUM_NOTES_FOR_OCTAVE 12

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
    deadbeef->mutex_lock (s->mutex);
    if (!s->samples || !s->fft_plan) {
        return;
    }

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
    int k0 = w->data->keys[MAX(band - 1, w->data->low_res_end)];
    int k1 = w->data->keys[band];
    int k2 = w->data->keys[MIN(band + 1, num_bands -1)];

    int start = ceil((double)(k1 - k0)/2 + k0);
    int end = ceil((double)(k2 - k1)/2 + k1);

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
    if (peaks[band] <= bars[band]) {
        peaks[band] = bars[band];
        delays[band] = delay;
        velocities[band] = 0;
    }
    else {
        if (delays[band] < 0) {
            peaks[band] -= velocities[band] * CONFIG_REFRESH_INTERVAL;
        }
        else {
            delays[band]--;
        }
        velocities[band] += d_velocity;
    }
}

static void
spectrum_band_set (struct spectrum_render_t *r, int playback_status, double amplitude, int band)
{
    r->bars[band] = CLAMP (amplitude - CONFIG_AMPLITUDE_MIN, -DBL_MAX, get_db_range ());

    if (playback_status == PLAYING) {
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
    const int num_low_res_values = w->data->low_res_end;

    int *x = w->data->low_res_indices;
    double y[num_low_res_values];

    for (int i = 0; i < num_low_res_values; i++) {
        y[i] = w->data->spectrum[w->data->keys[x[i]]];
    }

    int xx = 0;
    // Interpolate
    for (int i = 0; i < num_low_res_values; i++) {
        int i_end = MIN (i + 1, num_low_res_values - 1);
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
    if (w->playback_status != STOPPED) {
        if (w->playback_status == PAUSED) {
            spectrum_remove_refresh_interval (w);
        }
        do_fft (w->data);
        spectrum_bands_fill (w, num_bands, w->playback_status);
    }
    else if (w->playback_status == STOPPED) {
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
static void
spectrum_draw_cairo_static (w_spectrum_t *w, cairo_t *cr, int barw, int bands, cairo_rectangle_t *r)
{
    cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
    cairo_set_line_width (cr, 1);

    if (!CONFIG_DRAW_STYLE && CONFIG_ENABLE_VGRID && CONFIG_GAPS) {
        const int num_lines = MIN ((int)(r->width/barw), bands);
        gdk_cairo_set_source_color (cr, &CONFIG_COLOR_VGRID);
        for (int i = 0; i <= num_lines; i++) {
            cairo_move_to (cr, r->x + barw * i, r->y);
            cairo_rel_line_to (cr, 0, r->height);
        }
        cairo_stroke (cr);
    }

    // draw octave grid
    if (CONFIG_ENABLE_OGRID) {
        int offset = CONFIG_NOTE_MIN % NUM_NOTES_FOR_OCTAVE;
        gdk_cairo_set_source_color (cr, &CONFIG_COLOR_OGRID);
        const double octave_width = barw * NUM_NOTES_FOR_OCTAVE;
        double x = r->x + (NUM_NOTES_FOR_OCTAVE - offset) * barw;
        while (x < r->x + r->width) {
            cairo_move_to (cr, x, r->y);
            cairo_rel_line_to (cr, 0, r->height);
            cairo_stroke (cr);
            x += octave_width;
        }
    }

    // draw horizontal grid
    const int hgrid_num = get_db_range()/10;
    if (CONFIG_ENABLE_HGRID && r->height > 2*hgrid_num && r->width > 1) {
        cairo_set_source_rgba (cr,
                               CONFIG_COLOR_HGRID.red/65535.0,
                               CONFIG_COLOR_HGRID.green/65535.0,
                               CONFIG_COLOR_HGRID.blue/65535.0,
                               1);
        for (int i = 0; i <= hgrid_num; i++) {
            cairo_move_to (cr, r->x - 3, r->y + floor (i/(double)hgrid_num * r->height));
            cairo_rel_line_to (cr, r->width + 6, 0);
            cairo_stroke (cr);
        }
    }

    // draw octave grid on hover
    if (CONFIG_DISPLAY_OCTAVES && w->motion_ctx.entered && bands >= NUM_OCTAVES) {
        const int band_offset = ((((int)w->motion_ctx.x - (int)r->x) % ((barw * bands) / NUM_OCTAVES)))/barw;
        cairo_set_source_rgba (cr, 1, 0, 0, 0.5);
        for (int i = 1; i < bands; i++) {
            const int octave_enabled  = i % (bands / NUM_OCTAVES) == band_offset ? 1 : 0;
            const double x = r->x + barw * i;
            if (octave_enabled) {
                cairo_move_to (cr, x, r->y);
                cairo_rel_line_to (cr, 0, r->height);
                cairo_stroke (cr);
            }
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
spectrum_draw_cairo_bars (struct spectrum_render_t *render, cairo_t *cr, int bands, int barw, cairo_rectangle_t *r)
{
    if (r->height <= 0) {
        return;
    }
    cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
    const double base_s = (r->height / (double)get_db_range ());

    // draw spectrum

    // draw bars
    cairo_set_line_width (cr, 1);
    spectrum_gradient_set (cr, r->width, r->height);


    double x = r->x;
    for (int i = CONFIG_NOTE_MIN; i <= CONFIG_NOTE_MAX; i++, x += barw) {
        if (render->bars[i] <= 0) {
            continue;
        }
        cairo_rectangle (cr, x, r->y + r->height - 1, barw - 1, - render->bars[i] * base_s);
    }
    cairo_fill (cr);

    // draw peaks
    if (CONFIG_ENABLE_PEAKS && CONFIG_PEAK_FALLOFF >= 0) {
        x = r->x;
        for (gint i = CONFIG_NOTE_MIN; i < CONFIG_NOTE_MAX; i++, x += barw) {
            if (render->peaks[i] <= 0) {
                continue;
            }
            const double y = r->y + CLAMP (r->height - render->peaks[i] * base_s, 0, r->height - 1);
            cairo_move_to (cr, x, y); 
            cairo_rel_line_to (cr, barw - 1, 0);
        }
        cairo_stroke (cr);
    }

    if (CONFIG_ENABLE_BAR_MODE) { 
        gdk_cairo_set_source_color (cr, &CONFIG_COLOR_BG);
        for (int y = (int)r->y; y < r->height; y += 2) {
            cairo_move_to (cr, r->x, r->y + y); 
            cairo_rel_line_to (cr, r->width, 0);
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
    spectrum_gradient_set (cr, r->width, r->height);

    cairo_set_antialias (cr, CAIRO_ANTIALIAS_BEST);
    cairo_set_line_width (cr, 1);
    cairo_line_to (cr, r->x, r->height);
    double py = r->height - base_s * render->bars[0];
    cairo_line_to (cr, r->x, py);
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
        cairo_move_to (cr, r->x + barw * bands, 0);
        cairo_line_to (cr, r->x, r->height);

        cairo_close_path (cr);
        cairo_fill (cr);
    }
    else {
        cairo_stroke (cr);
    }

    if (CONFIG_ENABLE_PEAKS && CONFIG_PEAK_FALLOFF >= 0) {
        for (int i = 0; i < bands; i += 2) {
            const double x = r->x + barw * i;
            const double y = CLAMP (r->height - render->peaks[i] * base_s, 0, r->height);
            cairo_move_to (cr, x, y); 
            cairo_line_to (cr, x + barw, y);
        }
        cairo_stroke (cr);
    }

}

static double
spectrum_font_height (cairo_t *cr, const char *text)
{
    cairo_text_extents_t ex;
    cairo_text_extents (cr, text, &ex);
    return ex.height;
}

static double
spectrum_font_height_max (cairo_t *cr)
{
    cairo_set_font_size (cr, FONT_SIZE);
    return spectrum_font_height (cr, "C11");
}

static double
spectrum_font_width (cairo_t *cr, const char *text)
{
    cairo_text_extents_t ex;
    cairo_text_extents (cr, text, &ex);
    return ex.width;
}

static double
spectrum_font_width_max (cairo_t *cr)
{
    const int hgrid_num = get_db_range ()/10;
    cairo_set_font_size (cr, FONT_SIZE);
    char s[100] = "";
    double w_max = 0;
    for (int i = 1; i < hgrid_num; i++) {
        snprintf (s, sizeof (s), "%ddB", CONFIG_AMPLITUDE_MAX - i * 10);
        w_max = MAX (w_max, spectrum_font_width (cr, s));
    }

    return w_max;
}

static void
spectrum_draw_labels_freq (cairo_t *cr, cairo_rectangle_t *r, int barw)
{
    cairo_set_source_rgba (cr,
                           CONFIG_COLOR_TEXT.red/65535.0,
                           CONFIG_COLOR_TEXT.green/65535.0,
                           CONFIG_COLOR_TEXT.blue/65535.0,
                           1);
    cairo_set_font_size (cr, FONT_SIZE);
    double f_h = spectrum_font_height_max (cr);
    double y_offset = 3 * f_h/2;
    double y = r->y + y_offset;
    cairo_move_to (cr, r->x, y);
    char s[100] = "";

    int offset = CONFIG_NOTE_MIN % NUM_NOTES_FOR_OCTAVE;
    const double octave_width = barw * NUM_NOTES_FOR_OCTAVE;
    double x = r->x + (NUM_NOTES_FOR_OCTAVE - offset) * barw;
    int i = CONFIG_NOTE_MIN / NUM_NOTES_FOR_OCTAVE;
    while (x < r->x + r->width) {
        snprintf (s, sizeof (s), "C%d", 1 + i++);
        double font_width = spectrum_font_width (cr, s);
        cairo_move_to (cr, x - font_width/2, y);
        cairo_show_text (cr, s);
        x += octave_width;
    }
}

static void
spectrum_draw_labels_db (cairo_t *cr, cairo_rectangle_t *r)
{
    const int hgrid_num = get_db_range ()/10;
    const double f_h = spectrum_font_height_max (cr);
    if (CONFIG_ENABLE_HGRID && r->height > 2*hgrid_num && r->width > 1) {
        cairo_set_source_rgba (cr,
                               CONFIG_COLOR_TEXT.red/65535.0,
                               CONFIG_COLOR_TEXT.green/65535.0,
                               CONFIG_COLOR_TEXT.blue/65535.0,
                               1);
        cairo_set_font_size (cr, FONT_SIZE);
        cairo_move_to (cr, r->x, r->y);
        char s[100] = "";
        for (int i = 0; i <= hgrid_num; i++) {
            snprintf (s, sizeof (s), "%ddB", CONFIG_AMPLITUDE_MAX - i * 10);
            cairo_move_to (cr, r->x + FONT_PADDING/2, r->y + i/(double)hgrid_num * r->height + f_h/2);
            cairo_show_text (cr, s);
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
    else if (CONFIG_GAPS || CONFIG_BAR_W > 1) {
        barw = CLAMP (width / num_bands, 2, 20);
    }
    else {
        barw = CLAMP (width / num_bands, 2, 20) - 1;
    }
    return barw;
}

struct spectrum_render_ctx_t {
    int num_bands;
    int band_width;
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
};

static int
spectrum_width_max (cairo_t *cr, double width)
{
    const double font_width = spectrum_font_width_max (cr);
    const double label_width = font_width + FONT_PADDING;
    const double labels_width = 2 * label_width;
    return MAX (width - labels_width, 0);
}

static void
spectrum_draw_tooltip (struct spectrum_render_t *render, struct spectrum_data_t *data, cairo_t *cr, cairo_rectangle_t *r, struct motion_context *m_ctx, int bands, int barw)
{
    cairo_save (cr);

    const int x_bar = m_ctx->x - r->x;
    const int pos = (int)(x_bar/barw) + CONFIG_NOTE_MIN;
    if (pos >= CONFIG_NOTE_MIN && pos <= CONFIG_NOTE_MAX) {
        char t1[100];
        char t2[100];
        snprintf (t1, sizeof (t1), "%5.0f Hz (%s)", data->frequency[pos], spectrum_notes[pos]);
        const double amp = render->bars[pos];
        if (amp > -1000 && amp < 1000) {
            snprintf (t2, sizeof (t2), "%3.2f dB", render->bars[pos] + CONFIG_AMPLITUDE_MIN);
        }
        else {
            snprintf (t2, sizeof (t2), "-inf dB");
        }
        cairo_set_antialias (cr, CAIRO_ANTIALIAS_BEST);
        cairo_set_font_size (cr, 13);

        cairo_text_extents_t ex1 = {0};
        cairo_text_extents (cr, t1, &ex1);

        cairo_text_extents_t ex2 = {0};
        cairo_text_extents (cr, t2, &ex2);

        const int text_width = MAX (ex1.width - ex1.x_bearing, ex2.width - ex2.x_bearing);
        const int text_height = ex1.height + ex2.height;

        double x = m_ctx->x + 20;
        double y = m_ctx->y + 20;
        const double padding = 5;
        const double w_rect = text_width + 2 * padding + MAX (ex1.x_bearing, ex2.x_bearing);
        const double h_rect = text_height + 2 * padding + 6;
        y = MIN (y, r->height);
        x = CLAMP (x,r->x, r->x + r->width - w_rect);
        const double x_rect = x - padding;
        const double y_rect = y - padding + ex1.y_bearing;

        cairo_set_source_rgba (cr, 0, 0, 0, 1);
        cairo_rectangle (cr, x_rect, y_rect, w_rect, h_rect);
        cairo_fill (cr);
        cairo_set_source_rgba (cr, 0.9, 0.9, 0.9, 1);
        cairo_set_line_width (cr, 2.0);
        cairo_rectangle (cr, x_rect, y_rect, w_rect, h_rect);
        cairo_stroke (cr);
        cairo_set_source_rgba (cr,
                               CONFIG_COLOR_TEXT.red/65535.0,
                               CONFIG_COLOR_TEXT.green/65535.0,
                               CONFIG_COLOR_TEXT.blue/65535.0,
                               1);
        cairo_move_to (cr, x - ex1.x_bearing, y);
        cairo_show_text (cr, t1);
        cairo_move_to (cr, x - ex2.x_bearing, y + ex1.height + 3);
        cairo_show_text (cr, t2);
    }
    cairo_restore (cr);
}

struct spectrum_render_ctx_t
spectrum_get_render_ctx (cairo_t *cr, double width, double height)
{
    const double font_width = spectrum_font_width_max (cr);
    const double font_height = spectrum_font_height_max (cr);
    const double label_height = font_height + FONT_PADDING;
    const double label_width = font_width + FONT_PADDING;

    //const int num_bands = get_num_bars ();
    const int num_bands = CONFIG_NOTE_MAX - CONFIG_NOTE_MIN;
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
        .band_width = bar_width,
        .r_r = r_r,
        .r_l = r_l,
        .r_t = r_t,
        .r_b = r_b,
        .r_s = r_s,
    };
    return r_ctx;
}

gboolean
spectrum_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    w_spectrum_t *w = user_data;

    GtkAllocation a;
    gtk_widget_get_allocation (w->drawarea, &a);
    const int width = a.width;
    const int height = a.height;

    static int last_bar_w = -1;
    if (width != last_bar_w || w->need_redraw) {
        w->need_redraw = 0;
        create_frequency_table(w->data, w->samplerate, spectrum_width_max (cr, width));
    }
    last_bar_w = a.width;

    struct spectrum_render_ctx_t r_ctx = spectrum_get_render_ctx (cr, width, height);

    // draw background
    spectrum_background_draw (cr, width, height);

    spectrum_render (w, get_num_bars ());

    spectrum_draw_cairo_static (w, cr, r_ctx.band_width, r_ctx.num_bands, &r_ctx.r_s);
    if (!CONFIG_DRAW_STYLE) {
        spectrum_draw_cairo_bars (w->render, cr, r_ctx.num_bands, r_ctx.band_width, &r_ctx.r_s);
    }
    else {
        spectrum_draw_cairo (w->render, cr, r_ctx.num_bands, &r_ctx.r_s);
    }

    if (CONFIG_ENABLE_TOP_LABELS) {
        spectrum_draw_labels_freq (cr, &r_ctx.r_t, r_ctx.band_width);
    }
    if (CONFIG_ENABLE_BOTTOM_LABELS) {
        spectrum_draw_labels_freq (cr, &r_ctx.r_b, r_ctx.band_width);
    }
    if (CONFIG_ENABLE_LEFT_LABELS) {
        spectrum_draw_labels_db (cr, &r_ctx.r_l);
    }
    if (CONFIG_ENABLE_RIGHT_LABELS) {
        spectrum_draw_labels_db (cr, &r_ctx.r_r);
    }

    if (CONFIG_ENABLE_TOOLTIP && w->motion_ctx.entered) {
        spectrum_draw_tooltip (w->render, w->data, cr, &r_ctx.r_s, &w->motion_ctx, r_ctx.num_bands, r_ctx.band_width);
    }

    return FALSE;
}

