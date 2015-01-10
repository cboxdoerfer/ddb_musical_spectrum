/*
    Musical Spectrum plugin for the DeaDBeeF audio player

    Copyright (C) 2014 Christian Boxdörfer <christian.boxdoerfer@posteo.de>

    Based on DeaDBeeFs stock spectrum.
    Copyright (c) 2009-2014 Alexey Yakovenko <waker@users.sourceforge.net>
    Copyright (c) 2011 William Pitcock <nenolod@dereferenced.org>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef UTILS_HEADER
#define UTILS_HEADER

#include <gtk/gtk.h>

char *notes[] = {"C0","C#0","D0","D#0","E0","F0","F#0","G0","G#0","A0","A#0","B0",
                        "C1","C#1","D1","D#1","E1","F1","F#1","G1","G#1","A1","A#1","B1",
                        "C2","C#2","D2","D#2","E2","F2","F#2","G2","G#2","A2","A#2","B2",
                        "C3","C#3","D3","D#3","E3","F3","F#3","G3","G#3","A3","A#3","B3",
                        "C4","C#4","D4","D#4","E4","F4","F#4","G4","G#4","A4","A#4","B4",
                        "C5","C#5","D5","D#5","E5","F5","F#5","G5","G#5","A5","A#5","B5",
                        "C6","C#6","D6","D#6","E6","F6","F#6","G6","G#6","A6","A#6","B6",
                        "C7","C#7","D7","D#7","E7","F7","F#7","G7","G#7","A7","A#7","B7",
                        "C8","C#8","D8","D#8","E8","F8","F#8","G8","G#8","A8","A#8","B8",
                        "C9","C#9","D9","D#9","E9","F9","F#9","G9","G#9","A9","A#9","B9",
                        "C10","C#10","D10","D#10","E10","F10","F#10","G10","G#10","A10","A#10","B10"
                       };

void
_memset_pattern (char *data, const void* pattern, size_t data_len, size_t pattern_len);

void
create_gradient_table (gpointer user_data, GdkColor *colors, int num_colors);

void
create_window_table (gpointer user_data);

void
create_frequency_table (gpointer user_data);

#endif
