#include <stdlib.h>
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
#include "support.h"

#define TOP_EXTRA_SPACE 10
#define DB_GRID_DISTANCE 10
#define FONT_PADDING_HORIZONTAL 8
#define FONT_PADDING_VERTICAL 0
#define NUM_NOTES_FOR_OCTAVE 12

struct spectrum_render_ctx_t {
    int num_bands;
    double band_width;
    double note_width;
    // Spectrum rectangle
    cairo_rectangle_t center;
    // Left labels rectangle
    cairo_rectangle_t left;
    // Right labels rectangle
    cairo_rectangle_t right;
    // Top labels rectangle
    cairo_rectangle_t top;
    // Bottom labels rectangle
    cairo_rectangle_t bottom;
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
    if (render->pattern) {
        cairo_pattern_destroy (render->pattern);
        render->pattern = NULL;
    }
    free (render);
    render = NULL;
}

struct spectrum_data_t *
spectrum_data_new (void)
{
    struct spectrum_data_t *s_data = calloc (1, sizeof (struct spectrum_data_t));
    s_data->samples = calloc (MAX_FFT_SIZE * DDB_FREQ_MAX_CHANNELS, sizeof (float));
    s_data->spectrum = calloc (MAX_FFT_SIZE, sizeof (double));
    s_data->window = calloc (MAX_FFT_SIZE, sizeof (double));
    s_data->frequency = calloc (MAX_FFT_SIZE, sizeof (double));
    s_data->keys = calloc (MAX_FFT_SIZE, sizeof (int));
    s_data->low_res_indices = calloc (MAX_FFT_SIZE, sizeof (int));
    s_data->fft_in = fftw_alloc_real (MAX_FFT_SIZE);
    s_data->fft_out = fftw_alloc_complex (MAX_FFT_SIZE);
    s_data->fft_plan = fftw_plan_dft_r2c_1d (CLAMP (config_get_int (ID_FFT_SIZE), 512, MAX_FFT_SIZE), s_data->fft_in, s_data->fft_out, FFTW_ESTIMATE);
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
    render->pattern = NULL;
    return render;
}

static int
get_db_range ()
{
    return config_get_int (ID_AMPLITUDE_MAX)- config_get_int (ID_AMPLITUDE_MIN);
}

static int
get_align_pos (int width, int spectrum_width)
{
    int left = 0;
    switch (config_get_int (ID_ALIGNMENT)) {
        case LEFT_ALIGN:
            left = 0;
            break;
        case RIGHT_ALIGN:
            left = MIN (width, width - spectrum_width);
            break;
        case CENTER_ALIGN:
            left = MAX (0, (width - spectrum_width)/2);
            break;
        default:
            left = 0;
            break;
    }
    return left;
}

static uint32_t channel_list[] = {
    DDB_SPEAKER_FRONT_LEFT,
    DDB_SPEAKER_FRONT_RIGHT,
    DDB_SPEAKER_FRONT_CENTER,
    DDB_SPEAKER_LOW_FREQUENCY,
    DDB_SPEAKER_BACK_LEFT,
    DDB_SPEAKER_BACK_RIGHT,
    DDB_SPEAKER_FRONT_LEFT_OF_CENTER,
    DDB_SPEAKER_FRONT_RIGHT_OF_CENTER,
    DDB_SPEAKER_BACK_CENTER,
    DDB_SPEAKER_SIDE_LEFT,
    DDB_SPEAKER_SIDE_RIGHT,
    DDB_SPEAKER_TOP_CENTER,
    DDB_SPEAKER_TOP_FRONT_LEFT,
    DDB_SPEAKER_TOP_FRONT_CENTER,
    DDB_SPEAKER_TOP_FRONT_RIGHT,
    DDB_SPEAKER_TOP_BACK_LEFT,
    DDB_SPEAKER_TOP_BACK_CENTER,
    DDB_SPEAKER_TOP_BACK_RIGHT,
    0,
};

static int
skip_channel (int channel, int num_channels, uint32_t channel_mask)
{
    int ch_temp = 0;
    int i = 0;
    int ch_id = channel_list[i];
    while (ch_id != 0) {
        if (channel_mask & ch_id) {
            if (ch_temp == channel) {
                if (config_get_int (ID_CHANNEL) & ch_id) {
                    return 0;
                }
                else {
                    return 1;
                }
            }
            ch_temp++;
        }
        i++;
        ch_id = channel_list[i];
    }

    return 1;
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

    const int fft_size = config_get_int (ID_FFT_SIZE);
    const double fft_squared = fft_size * fft_size;

    for (int ch = 0; ch < s->num_channels; ++ch) {
        if (skip_channel (ch, s->num_channels, s->channel_mask)) {
            continue;
        }
        for (int i = 0; i < fft_size; i++) {
            s->fft_in[i] = s->samples[i * s->num_channels + ch] * s->window[i];
        }

        fftw_execute (s->fft_plan);
        for (int i = 0; i < fft_size/2; i++) {
            const double real = s->fft_out[i][0];
            const double imag = s->fft_out[i][1];
            const double mag = 10.0 * log10 (4.0 * (real*real + imag*imag)/ fft_squared);
            s->spectrum[i] = MAX (mag, s->spectrum[i]);
        }
    }
    deadbeef->mutex_unlock (s->mutex);
}

static inline double
spectrum_get_value (w_spectrum_t *w, int band, int num_bands)
{
    band = MAX (band, 1);
    const double k0 = w->data->keys[MAX(band - 1, 0)];
    const double k1 = w->data->keys[band];
    const double k2 = w->data->keys[MIN(band + 1, num_bands -1)];

    const int start = ceil((k1 - k0)/2.0 + k0);
    const int end = ceil((k2 - k1)/2.0 + k1);

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
            peaks[band] -= velocities[band] * config_get_int (ID_REFRESH_INTERVAL);
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
    r->bars[band] = CLAMP (amplitude - config_get_int (ID_AMPLITUDE_MIN), -DBL_MAX, get_db_range ());

    const int state = deadbeef->get_output ()->state ();
    if (state == OUTPUT_STATE_PLAYING) {
        if (config_get_int (ID_ENABLE_PEAKS) && config_get_int (ID_PEAK_FALLOFF) != -1) {
            // peak gravity
            spectrum_band_gravity (r->peaks, r->bars, r->v_peaks, r->peak_velocity, r->delay_peaks, r->peak_delay, band);
        }
        if (config_get_int (ID_ENABLE_AMPLITUDES) && config_get_int (ID_BAR_FALLOFF )!= -1) {
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

    for (int i = 0; i <= low_res_end; i++) {
        y[i] = w->data->spectrum[w->data->keys[x[i]]];
    }

    int band = 0;
    // Interpolate
    if (config_get_int (ID_INTERPOLATE)) {
        for (int i = 0; i < low_res_end; i++) {
            const int i_end = MIN (i + 1, low_res_end - 1);
            for (int x_temp = x[i]; x_temp < x[i_end]; x_temp++) {
                const double mu = (double)(x_temp - x[i]) / (double)(x[i_end] - x[i]);
                const double amp = hermite_interpolate (y, mu, i-1, 0.35, 0);
                spectrum_band_set (w->render, playback_status, amp, band++);
            }
        }
    }
    // Fill the rest of the bands which don't need to be interpolated
    for (int i = band; i < num_bands; ++i) {
        const double amp = spectrum_get_value (w, i, num_bands);
        spectrum_band_set (w->render, playback_status, amp, i);
    }
}

static void
spectrum_render (w_spectrum_t *w, int num_bands)
{
    if (w->playback_status != STOPPED) {
        do_fft (w->data);
        spectrum_bands_fill (w, num_bands, w->playback_status);
    }
    else {
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

static int
get_num_db_gridlines (void)
{
    return get_db_range ()/DB_GRID_DISTANCE;
}

static void
spectrum_draw_cairo_static (w_spectrum_t *w, cairo_t *cr, double note_width, int bands, cairo_rectangle_t *r)
{
    cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
    cairo_set_line_width (cr, 1);

    const double y = r->y;
    const double height = r->height;
    if (config_get_int (ID_ENABLE_WHITE_KEYS) || config_get_int (ID_ENABLE_BLACK_KEYS)) {
        int num_notes = get_num_notes ();
        double x = r->x;
        for (int i = 0; i < num_notes; i++, x += note_width) {
            const int r = (config_get_int (ID_NOTE_MIN) + i) % NUM_NOTES_FOR_OCTAVE;
            if (is_full_step (r)) {
                if (!config_get_int (ID_ENABLE_WHITE_KEYS)) {
                    continue;
                }
                gdk_cairo_set_source_color (cr, config_get_color (ID_COLOR_WHITE_KEYS));
            }
            else {
                if (!config_get_int (ID_ENABLE_BLACK_KEYS)) {
                    continue;
                }
                gdk_cairo_set_source_color (cr, config_get_color (ID_COLOR_BLACK_KEYS));
            }
            cairo_rectangle (cr, x, y, note_width - 1, height);
            cairo_fill (cr);
        }
    }

    const double octave_width = note_width * NUM_NOTES_FOR_OCTAVE;

    // draw octave grid
    if (config_get_int (ID_ENABLE_VGRID)) {
        const int offset = config_get_int (ID_NOTE_MIN) % NUM_NOTES_FOR_OCTAVE;
        gdk_cairo_set_source_color (cr, config_get_color (ID_COLOR_VGRID));
        double x = r->x + (NUM_NOTES_FOR_OCTAVE - offset) * note_width;
        while (x < r->x + r->width) {
            cairo_move_to (cr, x, r->y);
            cairo_rel_line_to (cr, 0, r->height);
            x += octave_width;
        }
        cairo_stroke (cr);
    }

    // draw horizontal grid
    const int hgrid_num = get_num_db_gridlines ();
    if (config_get_int (ID_ENABLE_HGRID) && r->height > 2*hgrid_num && r->width > 1) {
        gdk_cairo_set_source_color (cr, config_get_color (ID_COLOR_HGRID));
        const double y = r->y + TOP_EXTRA_SPACE;
        const double height = r->height - TOP_EXTRA_SPACE;
        for (int i = 0; i <= hgrid_num; i++) {
            cairo_move_to (cr, r->x - 3, y + floor (i/(double)hgrid_num * height));
            cairo_rel_line_to (cr, r->width + 6, 0);
        }
        cairo_stroke (cr);
    }

    // draw octave grid on hover
    if (config_get_int (ID_ENABLE_OGRID) && w->motion_ctx.entered) {
        const int dx = (int)(w->motion_ctx.x - r->x);
        if (dx >= 0 && dx <= r->width) {
            const int octave_offset = dx % (int)octave_width;
            const int band_offset = dx % (int)note_width;
            gdk_cairo_set_source_color (cr, config_get_color (ID_COLOR_OGRID));
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
    gdk_cairo_set_source_color (cr, config_get_color (ID_COLOR_BG));
    cairo_rectangle (cr, 0, 0, width, height);
    cairo_fill (cr);
}

static double
spectrum_amp_scale_get (const double height)
{
    return (height - TOP_EXTRA_SPACE) / (double)get_db_range ();
}

static void
spectrum_draw_peaks (struct spectrum_render_t *render, cairo_t *cr, cairo_rectangle_t *r, const int bar_width,
                     const int peak_width, const int num_bands, const double amp_scale, const double bar_offset)
{
    if (config_get_int (ID_ENABLE_PEAKS) && config_get_int (ID_PEAK_FALLOFF) >= 0) {
        if (config_get_int (ID_ENABLE_PEAKS_COLOR)) {
            gdk_cairo_set_source_color (cr, config_get_color (ID_COLOR_PEAKS));
        }
        double x = r->x;
        for (int i = 0; i < num_bands; i++, x += bar_width) {
            if (render->peaks[i] <= 0) {
                continue;
            }
            const double y = r->y + CLAMP (r->height - render->peaks[i] * amp_scale, 0, r->height - 1);
            cairo_move_to (cr, x + bar_offset, y); 
            cairo_rel_line_to (cr, peak_width, 0);
        }
        cairo_stroke (cr);
    }
}

static void
spectrum_draw_cairo_bars (struct spectrum_render_t *render, cairo_t *cr, int num_bars, int barw, cairo_rectangle_t *r)
{
    if (r->height <= 0) {
        return;
    }
    cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
    cairo_set_line_width (cr, 1);
    const double amp_scale = spectrum_amp_scale_get (r->height);

    // draw spectrum

    // draw bars
    cairo_set_source (cr, render->pattern);

    double x = r->x;
    double bar_width = barw;
    if (config_get_int (ID_GAPS) && barw > 1) {
        bar_width -= 1;
    }
    double bar_offset = 0;
    if (config_get_int (ID_SPACING) && bar_width > 4) {
        bar_offset = 1;
        bar_width -= 2;
    }

    if (config_get_int (ID_ENABLE_BAR_MODE)) { 
        for (int i = 0; i < num_bars; i++, x += barw) {
            const int y0 = r->y + r->height;
            for (int y = y0; y > y0 - render->bars[i] * amp_scale; y -= 2) {
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
            cairo_rectangle (cr, x + bar_offset, r->y + r->height - 1, bar_width, - render->bars[i] * amp_scale);
        }

        if (config_get_int (ID_FILL_SPECTRUM)) {
            cairo_fill (cr);
        }
        else {
            cairo_stroke (cr);
        }
    }

    // draw peaks
    spectrum_draw_peaks (render, cr, r, barw, bar_width, num_bars, amp_scale, bar_offset);
}

static void
spectrum_draw_cairo (struct spectrum_render_t *render, cairo_t *cr, int bands, cairo_rectangle_t *r)
{
    if (r->height <= 0) {
        return;
    }
    const double amp_scale = spectrum_amp_scale_get (r->height);
    const int barw = CLAMP (floor(r->width / bands), 2, 20) - 1;

    // draw spectrum
    cairo_set_source (cr, render->pattern);

    cairo_set_antialias (cr, CAIRO_ANTIALIAS_DEFAULT);
    cairo_set_line_width (cr, 1);
    cairo_move_to (cr, r->x, r->height);
    double py = r->height - amp_scale * render->bars[0];
    cairo_rel_line_to (cr, 0, py);
    for (gint i = 0; i < bands; i++)
    {
        const double x = r->x + barw * i;
        const double y = r->height - amp_scale * MAX (render->bars[i],0);

        if (!config_get_int (ID_FILL_SPECTRUM)) {
            cairo_move_to (cr, x - 0.5, py);
        }
        cairo_line_to (cr, x + 0.5, y);
        py = y;
    }
    if (config_get_int (ID_FILL_SPECTRUM)) {
        cairo_line_to (cr, r->x + r->width, r->height);
        cairo_rel_line_to (cr, r->width, 0);

        cairo_close_path (cr);
        cairo_fill (cr);
    }
    else {
        cairo_stroke (cr);
    }

    spectrum_draw_peaks (render, cr, r, barw, barw, bands, amp_scale, 0);
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
    const int hgrid_num = get_num_db_gridlines ();
    char s[100] = "";
    double w_max = 0;
    for (int i = 1; i < hgrid_num; i++) {
        snprintf (s, sizeof (s), "%ddB", config_get_int (ID_AMPLITUDE_MAX) - i * DB_GRID_DISTANCE);
        w_max = MAX (w_max, spectrum_pango_font_width (layout, s));
    }

    return w_max;
}

static void
spectrum_cairo_set_source_color_rgba (cairo_t *cr, GdkColor *clr, double alpha)
{
    const double d = 65535.0;
    cairo_set_source_rgba (cr, clr->red / d , clr->green / d, clr->blue / d, alpha);
}

static void
spectrum_draw_labels_freq (cairo_t *cr, PangoLayout *layout, struct spectrum_render_ctx_t *r_ctx, cairo_rectangle_t *r)
{
    const double note_width = r_ctx->note_width;
    const double f_w_full = spectrum_pango_font_width (layout, "D") + 2;
    const double f_w_half = spectrum_pango_font_width (layout, "C#") + 2;
    const double y = r->y + FONT_PADDING_VERTICAL/2;
    cairo_move_to (cr, r->x, y);
    char s[100] = "";

    char *note_array[] = {
        "C","C#","D","D#","E","F","F#","G","G#","A","A#","B",
    };


    double x = r->x + note_width/2;
    const double x_end = r->x + r->width;

    const int show_full_steps = note_width > f_w_full ? 1 : 0;
    const int show_half_steps = note_width > f_w_half ? 1 : 0;
    for (int i = config_get_int (ID_NOTE_MIN); i <= config_get_int (ID_NOTE_MAX) && x < x_end; i++, x += note_width) {
        int r = i % NUM_NOTES_FOR_OCTAVE; 
        if (i == 0 || r == 0) {
            snprintf (s, sizeof (s), "%s", spectrum_notes[i]);
            gdk_cairo_set_source_color (cr, config_get_color (ID_COLOR_TEXT));
        }
        else if (show_full_steps && is_full_step (r)) {
            snprintf (s, sizeof (s), "%s", note_array[r]);
            spectrum_cairo_set_source_color_rgba (cr, config_get_color (ID_COLOR_TEXT), 0.75);
        }
        else if (show_half_steps) {
            snprintf (s, sizeof (s), "%s", note_array[r]);
            spectrum_cairo_set_source_color_rgba (cr, config_get_color (ID_COLOR_TEXT), 0.5);
        }
        else {
            continue;
        }

        const double font_width = spectrum_pango_font_width (layout, s);
        const double xx = x - font_width/2;
        cairo_move_to (cr, xx, y);
        pango_layout_set_text (layout, s, -1);
        pango_cairo_show_layout (cr, layout);
    }
}

static void
spectrum_draw_labels_db (cairo_t *cr, PangoLayout *layout, cairo_rectangle_t *r)
{
    const int hgrid_num = get_num_db_gridlines ();
    const double font_height = spectrum_pango_font_height (layout, "-120dB");
    if (r->height > 2*hgrid_num && r->width > 1) {
        gdk_cairo_set_source_color (cr, config_get_color (ID_COLOR_TEXT));
        const double y = r->y + TOP_EXTRA_SPACE;
        const double height = r->height - TOP_EXTRA_SPACE;
        cairo_move_to (cr, r->x, y);
        char s[100] = "";
        for (int i = 0; i <= hgrid_num; i++) {
            snprintf (s, sizeof (s), "%ddB", config_get_int (ID_AMPLITUDE_MAX) - i * DB_GRID_DISTANCE);
            cairo_move_to (cr, r->x + FONT_PADDING_HORIZONTAL/2, y + i/(double)hgrid_num * height - font_height/2);
            pango_layout_set_text (layout, s, -1);
            pango_cairo_show_layout (cr, layout);
        }
    }
}

static int
spectrum_bar_width_get (int num_bands, double width)
{
    int barw = 0;
    if (config_get_int (ID_DRAW_STYLE) == SOLID_STYLE) {
        barw = 1;
    }
    else {
        if (config_get_int (ID_BAR_W) == 0) {
            barw = CLAMP (width / num_bands, 2, 100);
        }
        else {
            barw = config_get_int (ID_BAR_W);
        }
    }
    return barw;
}

//static int
//spectrum_width_max (PangoLayout *layout, double width)
//{
//    const double font_width = spectrum_font_width_max (layout);
//    const double label_width = font_width + FONT_PADDING_HORIZONTAL;
//    const double labels_width = 2 * label_width;
//    return MAX (width - labels_width, 0);
//}

static PangoLayout *
spectrum_font_layout_get (cairo_t *cr, const enum spectrum_config_string_index id)
{
    PangoLayout *layout = pango_cairo_create_layout (cr);
    PangoFontDescription *desc = pango_font_description_from_string (config_get_string (id));
    pango_layout_set_font_description (layout, desc);
    pango_font_description_free (desc);
    desc = NULL;

    return layout;
}

static void
spectrum_draw_tooltip (struct spectrum_render_t *render,
                       struct spectrum_data_t *data,
                       cairo_t *cr,
                       struct spectrum_render_ctx_t *r_ctx,
                       struct motion_context *m_ctx)
{
    cairo_rectangle_t *r = &r_ctx->center;

    const double band_width = r_ctx->band_width;
    const double note_width = r_ctx->note_width;

    const double x_bar = m_ctx->x - r->x;
    const int pos = (int)floor(x_bar/note_width + config_get_int (ID_NOTE_MIN));
    const int freq_pos = (int)(x_bar/band_width);

    if (pos < config_get_int (ID_NOTE_MIN) || pos > config_get_int (ID_NOTE_MAX)
        || freq_pos < 0 || freq_pos >= r_ctx->num_bands) {
        return;
    }

    char t1[100];
    const double amp = render->bars[pos];
    if (amp > -1000 && amp < 1000) {
        snprintf (t1, sizeof (t1), "%.0f Hz (%s)\n%3.2f dB", data->frequency[freq_pos], spectrum_notes[pos], render->bars[pos] + config_get_int (ID_AMPLITUDE_MIN));
    }
    else {
        snprintf (t1, sizeof (t1), "%.0f Hz (%s)\n-inf dB", data->frequency[freq_pos], spectrum_notes[pos]);
    }

    cairo_save (cr);

    PangoLayout *layout = spectrum_font_layout_get (cr, ID_STRING_FONT_TOOLTIP);
    const double text_width = spectrum_pango_font_width (layout, t1);
    const double text_height = spectrum_pango_font_height (layout, t1);

    const double padding = 5;
    double x = m_ctx->x + 20;
    double y = m_ctx->y + 20;
    const double w_rect = text_width + 2 * padding;
    const double h_rect = text_height + 2 * padding;
    y = CLAMP (y, r->y, r->y + r->height - h_rect);
    x = CLAMP (x, r->x, r->x + r->width - w_rect);
    const double x_rect = x - padding;
    const double y_rect = y - padding;

    gdk_cairo_set_source_color (cr, config_get_color (ID_COLOR_BG));
    cairo_rectangle (cr, x_rect, y_rect, w_rect, h_rect);
    cairo_fill (cr);

    gdk_cairo_set_source_color (cr, config_get_color (ID_COLOR_TEXT));
    cairo_set_line_width (cr, 2.0);
    cairo_rectangle (cr, x_rect, y_rect, w_rect, h_rect);
    cairo_stroke (cr);

    gdk_cairo_set_source_color (cr, config_get_color (ID_COLOR_TEXT));
    cairo_move_to (cr, x, y);
    pango_layout_set_text (layout, t1, -1);
    pango_cairo_show_layout (cr, layout);

    cairo_restore (cr);

    g_object_unref (layout);
}

struct spectrum_render_ctx_t
spectrum_get_render_ctx (cairo_t *cr, double width, double height)
{
    PangoLayout *layout = spectrum_font_layout_get (cr, ID_STRING_FONT);
    const double font_width = spectrum_font_width_max (layout);
    const double font_height = spectrum_font_height_max (layout);

    g_object_unref (layout);

    const double label_height = font_height + FONT_PADDING_VERTICAL;
    const double label_width = font_width + FONT_PADDING_HORIZONTAL;

    const double labels_width = (config_get_int (ID_ENABLE_RIGHT_LABELS) + config_get_int (ID_ENABLE_LEFT_LABELS)) * label_width;
    const double spectrum_width_max = width - labels_width;
    const int num_bands = get_num_bars (spectrum_width_max);
    const int bar_width = spectrum_bar_width_get (num_bands, spectrum_width_max);
    const double spectrum_width = bar_width * num_bands;
    const int x_start = get_align_pos (width, spectrum_width + labels_width);

    cairo_rectangle_t left, right, top, bottom, center;

    left.width = label_width * config_get_int (ID_ENABLE_LEFT_LABELS);
    right.width = label_width * config_get_int (ID_ENABLE_RIGHT_LABELS);
    center.width = spectrum_width;
    top.width = spectrum_width;
    bottom.width = spectrum_width;

    top.height = label_height * config_get_int (ID_ENABLE_TOP_LABELS);
    bottom.height = label_height * config_get_int (ID_ENABLE_BOTTOM_LABELS);
    center.height = height - top.height - bottom.height;
    left.height = center.height;
    right.height = center.height;

    left.x = x_start;
    center.x = left.x + left.width;
    top.x = center.x;
    bottom.x = center.x;
    right.x = center.x + center.width;

    top.y = 0;
    center.y = top.y + top.height;
    bottom.y = center.y + center.height;
    left.y = center.y;
    right.y = center.y;

    struct spectrum_render_ctx_t r_ctx = {
        .num_bands = num_bands,
        .band_width = bar_width,
        .note_width = spectrum_width / (double)(get_num_notes ()),
        .right = right,
        .left = left,
        .top = top,
        .bottom = bottom,
        .center = center,
    };

    return r_ctx;
}

int
draw_labels (cairo_t *cr, int width, int height)
{
    double x1, y1, x2, y2;
    cairo_clip_extents (cr, &x1, &y1, &x2, &y2);

    const int clip_width = x2 - x1;
    const int clip_height = y2 - y1;

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
    w->spectrum_rectangle = r_ctx.center;

    if (width != w->prev_width || w->need_redraw) {
        if (w->need_redraw == 1) {
            w->need_redraw = 0;
        }
        create_frequency_table(w->data, w->samplerate, r_ctx.num_bands);
        if (w->render->pattern) {
            cairo_pattern_destroy (w->render->pattern);
            w->render->pattern = NULL;
        }
        w->render->pattern = spectrum_gradient_pattern_get (CONFIG_GRADIENT_COLORS, config_get_int (ID_GRADIENT_ORIENTATION), width, height);
    }
    w->prev_width = width;
    w->prev_height = height;


    spectrum_render (w, r_ctx.num_bands);

    // draw background
    spectrum_background_draw (cr, width, height);

    spectrum_draw_cairo_static (w, cr, r_ctx.note_width, r_ctx.num_bands, &r_ctx.center);
    if (config_get_int (ID_DRAW_STYLE) == MUSICAL_STYLE) {
        spectrum_draw_cairo_bars (w->render, cr, r_ctx.num_bands, r_ctx.note_width, &r_ctx.center);
    }
    else {
        spectrum_draw_cairo (w->render, cr, r_ctx.num_bands, &r_ctx.center);
    }

    if (draw_labels (cr, width, height)) {
        PangoLayout *layout = spectrum_font_layout_get (cr, ID_STRING_FONT);
        if (config_get_int (ID_ENABLE_TOP_LABELS)) {
            spectrum_draw_labels_freq (cr, layout, &r_ctx, &r_ctx.top);
        }
        if (config_get_int (ID_ENABLE_BOTTOM_LABELS)) {
            spectrum_draw_labels_freq (cr, layout, &r_ctx, &r_ctx.bottom);
        }
        if (config_get_int (ID_ENABLE_LEFT_LABELS)) {
            spectrum_draw_labels_db (cr, layout, &r_ctx.left);
        }
        if (config_get_int (ID_ENABLE_RIGHT_LABELS)) {
            spectrum_draw_labels_db (cr, layout, &r_ctx.right);
        }
        g_object_unref (layout);
        layout = NULL;

    }

    if (config_get_int (ID_ENABLE_TOOLTIP) && w->motion_ctx.entered) {
        spectrum_draw_tooltip (w->render, w->data, cr, &r_ctx, &w->motion_ctx);
    }

    return FALSE;
}

