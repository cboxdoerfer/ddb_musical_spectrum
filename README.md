[![Build Status](https://drone.io/github.com/cboxdoerfer/ddb_musical_spectrum/status.png)](https://drone.io/github.com/cboxdoerfer/ddb_musical_spectrum/latest)

Musical Spectrum plugin for DeaDBeeF audio player
====================

This plugin is based on DeaDBeeFs stock spectrum. It offers variable FFT size (up to 32768), Blackmann-Harris and Hanning window functions, and various eye candy options.

## Installation

### Arch Linux
See the [AUR](https://aur.archlinux.org/packages/deadbeef-plugin-musical-spectrum-git/)

### Gentoo
See ebuilds [here](https://github.com/megabaks/stuff/tree/master/media-plugins/deadbeef-musical-spectrum)

### Binaries

#### Dev
[x86_64](https://drone.io/github.com/cboxdoerfer/ddb_musical_spectrum/files/deadbeef-plugin-builder/ddb_musical_spectrum_x86_64.tar.gz)

[i686](https://drone.io/github.com/cboxdoerfer/ddb_musical_spectrum/files/deadbeef-plugin-builder/ddb_musical_spectrum_i686.tar.gz)

Install them as follows:

x86_64: ```tar -xvf ddb_musical_spectrum_x86_64.tar.gz -C ~/.local/lib/deadbeef```

i686: ```tar -xvf ddb_musical_spectrum_i686.tar.gz -C ~/.local/lib/deadbeef```

### Other distributions
#### Build from sources
First install DeaDBeeF (>=0.6) and fftw3 and their development files.

(e.g. on Ubuntu: ```sudo apt-get update && sudo apt-get install build-essential git libgtk2.0-dev libgtk-3-dev libfftw3-dev```)


```bash
make
./userinstall.sh
```

## Screenshot

![](https://user-images.githubusercontent.com/6108388/70710858-6f132880-1ce0-11ea-9b8e-85cfa711eda8.png)
