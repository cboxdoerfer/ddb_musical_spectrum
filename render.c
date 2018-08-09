#include <math.h>
#include <gdk/gdk.h>
#include <stdint.h>
#include "render.h"
#include "config.h"
#include "utils.h"
#include "fastftoi.h"
#include "draw_utils.h"
#include "spectrum.h"

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
    if (data->fft_in) {
        free (data->fft_in);
        data->fft_in = NULL;
    }
    if (data->fft_out) {
        free (data->fft_out);
        data->fft_out = NULL;
    }
    free (data);
    data = NULL;
}

struct spectrum_data_t *
spectrum_data_new (void)
{
    struct spectrum_data_t *s_data = calloc (1, sizeof (struct spectrum_data_t));
    s_data->samples = calloc (MAX_FFT_SIZE * DDB_FREQ_MAX_CHANNELS, sizeof (double));
    s_data->spectrum = calloc (MAX_FFT_SIZE, sizeof (double));
    s_data->fft_in = calloc (MAX_FFT_SIZE, sizeof (double));
    s_data->fft_out = calloc (MAX_FFT_SIZE, sizeof (double));
    return s_data;
}

static int
get_align_pos (int width, int bands, int bar_width)
{
    int left = 0;
    switch (CONFIG_ALIGNMENT) {
        case LEFT:
            left = 0;
            break;
        case RIGHT:
            left = MIN (width, width - (bar_width * bands));
            break;
        case CENTER:
            left = MAX (0, (width - (bar_width * bands))/2);
            break;
        default:
            left = 0;
            break;
    }
    return left;
}

static void
do_fft (w_spectrum_t *w)
{
    deadbeef->mutex_lock (w->mutex);
    if (!w->data->samples || !w->p_r2c) {
        return;
    }

    for (int i = 0; i < CONFIG_FFT_SIZE; ++i) {
        w->data->spectrum[i] = -DBL_MAX;
    }

    for (int ch = 0; ch < w->data->num_channels; ++ch) {
        for (int i = 0; i < CONFIG_FFT_SIZE; i++) {
            w->fft_in[i] = w->data->samples[i * w->data->num_channels + ch] * w->window[i];
        }

        fftw_execute (w->p_r2c);
        for (int i = 0; i < CONFIG_FFT_SIZE/2; i++) {
            const double real = w->fft_out[i][0];
            const double imag = w->fft_out[i][1];
            const double mag = 10.0 * log10 (4.0 * (real*real + imag*imag)/ (double)(CONFIG_FFT_SIZE*CONFIG_FFT_SIZE));
            w->data->spectrum[i] = MAX (mag, w->data->spectrum[i]);
        }
    }
    deadbeef->mutex_unlock (w->mutex);
}

static inline double
spectrum_get_value (w_spectrum_t *w, int band, int num_bands)
{
    band = MAX (band, 1);
    int k0 = w->keys[MAX(band - 1, w->low_res_end)];
    int k1 = w->keys[band];
    int k2 = w->keys[MIN(band + 1, num_bands -1)];

    int start = floorf((float)(k1 - k0)/2 + k0);
    int end = ceilf((float)(k2 - k1)/2 + k1);

    if (start >= end) {
        return w->data->spectrum[end];
    }
    double value = -DBL_MAX;
    for (int i = k1; i < k2; i++) {
        value = MAX (w->data->spectrum[i] ,value);
    }
    return value;
}

static void
spectrum_bands_fill (w_spectrum_t *w, int num_bands)
{
    int num_low_res_values = w->low_res_end;

    int *x = w->low_res_indices;
    double y[num_low_res_values];

    for (int i = 0; i < num_low_res_values; i++) {
        y[i] = w->data->spectrum[w->keys[x[i]]];
    }

    // Interpolate
    for (int i = 0; i < num_low_res_values; i++) {
        int i_end = MIN (i + 1, num_low_res_values - 1);
        for (int xx = x[i]; xx < x[i_end]; xx++) {
            double mu = (double)(xx - x[i]) / (double)(x[i_end] - x[i]);
            w->bars[xx] = hermite_interpolate (y, mu, i-1, 0.35, 0);
        }
    }

    // Fill the rest of the bands which don't need to be interpolated
    for (int i = num_low_res_values + 1; i < num_bands; ++i) {
        w->bars[i] = spectrum_get_value (w, i, num_bands);
    }
}

static void
spectrum_render (w_spectrum_t *w, int height, int num_bands)
{
    if (w->playback_status != STOPPED) {
        if (w->playback_status == PAUSED) {
            spectrum_remove_refresh_interval (w);
        }
        do_fft (w);

        const int peak_delay = ftoi (CONFIG_PEAK_DELAY/CONFIG_REFRESH_INTERVAL);
        const int bars_delay = ftoi (CONFIG_BAR_DELAY/CONFIG_REFRESH_INTERVAL);

        const double peak_gravity = CONFIG_PEAK_FALLOFF/(1000.0 * 1000.0);
        const double peak_velocity = peak_gravity * CONFIG_REFRESH_INTERVAL;

        const double bars_gravity = CONFIG_BAR_FALLOFF/(1000.0 * 1000.0);
        const double bars_velocity = bars_gravity * CONFIG_REFRESH_INTERVAL;

        spectrum_bands_fill (w, num_bands);
        for (int i = 0; i < num_bands; i++) {
            w->bars[i] = CLAMP (w->bars[i] + CONFIG_DB_RANGE, 0, CONFIG_DB_RANGE);

            if (CONFIG_ENABLE_PEAKS && CONFIG_PEAK_FALLOFF != -1) {
                if (w->peaks[i] <= w->bars[i]) {
                    w->peaks[i] = w->bars[i];
                    w->delay_peaks[i] = peak_delay;
                    w->v_peaks[i] = 0;
                }
                else {
                    w->v_peaks[i] += peak_velocity;
                    if (w->delay_peaks[i] < 0) {
                        w->peaks[i] -= w->v_peaks[i] * CONFIG_REFRESH_INTERVAL;
                    }
                    else {
                        w->delay_peaks[i]--;
                    }
                }
            }

            if (!CONFIG_DRAW_STYLE && CONFIG_BAR_FALLOFF != -1) {
                if (w->bars_peak[i] <= w->bars[i]) {
                    w->bars_peak[i] = w->bars[i];
                    w->delay_bars[i] = bars_delay;
                    w->v_bars[i] = 0;
                }
                else {
                    w->v_bars[i] += bars_velocity;
                    if (w->delay_bars[i] < 0) {
                        w->bars_peak[i] -= w->v_bars[i] * CONFIG_REFRESH_INTERVAL;
                    }
                    else {
                        w->delay_bars[i]--;
                    }
                }
                w->bars[i] = w->bars_peak[i];
            }
        }
    }
    else if (w->playback_status == STOPPED) {
        spectrum_remove_refresh_interval (w);
        for (int i = 0; i < num_bands; i++) {
                w->bars[i] = 0;
                w->v_bars[i] = 0;
                w->delay_bars[i] = 0;
                w->bars_peak[i] = 0;
                w->peaks[i] = 0;
                w->v_peaks[i] = 0;
                w->delay_peaks[i] = 0;
        }
    }


}
static void
spectrum_draw_cairo_static (w_spectrum_t *w, cairo_t *cr, int barw, int bands, float left, float width, float height)
{
    // draw octave grid
    if (CONFIG_ENABLE_OCTAVE_GRID) {
        gdk_cairo_set_source_color (cr, &CONFIG_COLOR_OCTAVE_GRID);
        const int spectrum_width = MIN (barw * bands, width);
        const float octave_width = CLAMP (((float)spectrum_width / 11), 1, spectrum_width);
        for (float i = left; i < spectrum_width - 1 && i < width - 1; i += octave_width) {
            cairo_move_to (cr, i, 0);
            cairo_line_to (cr, i, height);
            cairo_stroke (cr);
        }
    }

    // draw horizontal grid
    const int hgrid_num = CONFIG_DB_RANGE/10;
    if (CONFIG_ENABLE_HGRID && height > 2*hgrid_num && width > 1) {
        cairo_set_source_rgba (cr,
                               CONFIG_COLOR_HGRID.red/65535.f,
                               CONFIG_COLOR_HGRID.green/65535.f,
                               CONFIG_COLOR_HGRID.blue/65535.f,
                               0.2);
        for (int i = 1; i < hgrid_num; i++) {
            cairo_move_to (cr, 0, i/(float)hgrid_num * height);
            cairo_line_to (cr, width, i/(float)hgrid_num * height);
            cairo_stroke (cr);
        }
    }

    // draw octave grid on hover
    if (CONFIG_DISPLAY_OCTAVES && w->motion_ctx.entered) {
        const int band_offset = (((int)w->motion_ctx.x % ((barw * bands) / 11)))/barw;
        cairo_set_source_rgba (cr, 1, 0, 0, 0.5);
        for (gint i = 0; i < bands; i++) {
            const int octave_enabled  = (w->motion_ctx.entered && (i % (bands / 11)) == band_offset) ? 1 : 0;
            const double x = left + barw * i;
            if (octave_enabled) {
                cairo_move_to (cr, x, 0);
                cairo_line_to (cr, x, height);
                cairo_stroke (cr);
            }
        }
    }
}

static cairo_pattern_t *
spectrum_gradient_pattern_get (cairo_t *cr, float width, float height)
{
    cairo_pattern_t *pat = NULL;

    if (CONFIG_GRADIENT_ORIENTATION == 0) {
        pat = cairo_pattern_create_linear (0, 0, 0, height);
    }
    else {
        pat = cairo_pattern_create_linear (0, 0, width, 0);
    }

    const float step = 1.0/(CONFIG_NUM_COLORS - 1);
    float grad_pos = 0;
    for (int i = 0; i < CONFIG_NUM_COLORS; i++) {
        cairo_pattern_add_color_stop_rgb (pat,
                                          grad_pos,
                                          CONFIG_GRADIENT_COLORS[i].red/65535.f,
                                          CONFIG_GRADIENT_COLORS[i].green/65535.f,
                                          CONFIG_GRADIENT_COLORS[i].blue/65535.f);
        grad_pos += step;
    }
    return pat;
}

static void
spectrum_gradient_set (cairo_t *cr, float width, float height)
{
    if (CONFIG_NUM_COLORS > 1) {
        cairo_pattern_t *pat = spectrum_gradient_pattern_get (cr, width, height);
        cairo_set_source (cr, pat);
        cairo_pattern_destroy (pat);
    }
    else {
        gdk_cairo_set_source_color (cr, &CONFIG_GRADIENT_COLORS[0]);
    }
}

static void
spectrum_background_draw (cairo_t *cr, float width, float height)
{
    gdk_cairo_set_source_color (cr, &CONFIG_COLOR_BG);
    cairo_rectangle (cr, 0, 0, width, height);
    cairo_fill (cr);
}

static void
spectrum_draw_cairo_bars (w_spectrum_t *w, cairo_t *cr, int bands, int width, int height)
{
    cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
    const float base_s = (height / (float)CONFIG_DB_RANGE);

    int barw = 0;
    if (CONFIG_GAPS || CONFIG_BAR_W > 1) {
        barw = CLAMP (width / bands, 2, 20);
    }
    else {
        barw = CLAMP (width / bands, 2, 20) - 1;
    }

    const int left = get_align_pos (width, bands, barw);

    // draw background
    spectrum_background_draw (cr, width, height);

    // draw spectrum

    // draw bars
    cairo_set_line_width (cr, 1);
    spectrum_gradient_set (cr, width, height);


    cairo_move_to (cr, left, height);
    for (gint i = 0; i < bands; i++)
    {
        const double x = left + barw * i;
        const double y = CLAMP (height - (w->bars[i] * base_s), 0, height);
        cairo_line_to (cr, x, y); 
        cairo_line_to (cr, x + barw, y);
    }
    cairo_line_to (cr, left + barw * (bands-1),height);
    cairo_line_to (cr, width,height);
    cairo_fill (cr);


    // draw peaks
    if (CONFIG_ENABLE_PEAKS && CONFIG_PEAK_FALLOFF >= 0) {
        for (gint i = 0; i < bands; i++) {
            const double x = left + barw * i;
            const double y = CLAMP (height - w->peaks[i] * base_s, 0, height);
            cairo_move_to (cr, x, y); 
            cairo_line_to (cr, x + barw, y);
        }
        cairo_stroke (cr);
    }

    if (CONFIG_ENABLE_BAR_MODE) { 
        gdk_cairo_set_source_color (cr, &CONFIG_COLOR_BG);
        for (gint j = 0; j < height; j += 2) {
                cairo_move_to (cr, 0, j); 
                cairo_line_to (cr, width, j);
        }
        cairo_stroke (cr);
    }

    if (CONFIG_ENABLE_VGRID && CONFIG_GAPS) {
        const int num_lines = MIN (width/barw, bands);
        gdk_cairo_set_source_color (cr, &CONFIG_COLOR_VGRID);
        for (int i = 0; i < num_lines; i++) {
            cairo_move_to (cr, left + barw * i, 0);
            cairo_line_to (cr, left + barw * i, height);
        }
        cairo_stroke (cr);
    }

    spectrum_draw_cairo_static (w, cr, barw, bands, left, width, height);
}

static void
spectrum_draw_cairo (w_spectrum_t *w, cairo_t *cr, int bands, int width, int height)
{
    const float base_s = (height / (float)CONFIG_DB_RANGE);
    const int barw = CLAMP (width / bands, 2, 20) - 1;
    const int left = get_align_pos (width, bands, barw);

    // draw background
    spectrum_background_draw (cr, width, height);


    // draw spectrum
    spectrum_gradient_set (cr, width, height);

    cairo_set_line_width (cr, 1);
    cairo_line_to (cr, 0, height);
    double py = height - base_s * w->bars[0];
    cairo_line_to (cr, 0, py);
    for (gint i = 0; i < bands; i++)
    {
        const double x = left + barw * i;
        const double y = height - base_s * w->bars[i];

        if (!CONFIG_FILL_SPECTRUM) {
            cairo_move_to (cr, x - 0.5, py);
        }
        cairo_line_to (cr, x + 0.5, y);
        py = y;
    }
    if (CONFIG_FILL_SPECTRUM) {
        cairo_move_to (cr, left + barw * bands, 0);
        cairo_line_to (cr, 0, height);

        cairo_close_path (cr);
        cairo_fill (cr);
    }
    else {
        cairo_stroke (cr);
    }

    if (CONFIG_ENABLE_PEAKS && CONFIG_PEAK_FALLOFF >= 0) {
        for (int i = 0; i < bands; i += 2) {
            const double x = left + barw * i;
            const double y = CLAMP (height - w->peaks[i] * base_s, 0, height);
            cairo_move_to (cr, x, y); 
            cairo_line_to (cr, x + barw, y);
        }
        cairo_stroke (cr);
    }

    spectrum_draw_cairo_static (w, cr, barw, bands, left, width, height);
}

gboolean
spectrum_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    w_spectrum_t *w = user_data;

    GtkAllocation a;
    gtk_widget_get_allocation (w->drawarea, &a);

    static int last_bar_w = -1;
    if (a.width != last_bar_w || w->need_redraw) {
        w->need_redraw = 0;
        create_frequency_table(w);
    }
    last_bar_w = a.width;

    const int bands = get_num_bars ();
    const int width = a.width;
    const int height = a.height;

    spectrum_render (w, height, bands);

    if (!CONFIG_DRAW_STYLE) {
        spectrum_draw_cairo_bars (w, cr, bands, width, height);
    }
    else {
        spectrum_draw_cairo (w, cr, bands, width, height);
    }

    return FALSE;
}

