[![Build Status](https://drone.io/github.com/cboxdoerfer/ddb_musical_spectrum/status.png)](https://drone.io/github.com/cboxdoerfer/ddb_musical_spectrum/latest)

Musical Spectrum plugin for DeaDBeeF audio player
====================

This plugin is based on DeaDBeeFs stock spectrum. It currently offers more accuracy (FFT size 8192) and the bars are layed out to represent the musical keys. In the future I will add more features like select FFT size, display music keys or frequencies - basically most of what [foo_musical_spectrum](http://wiki.hydrogenaudio.org/index.php?title=Foobar2000:Components/Musical_Spectrum_%28foo_musical_spectrum%29) already does. 

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

![](http://i.imgur.com/IGice7K.png)
