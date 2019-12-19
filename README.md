Musical Spectrum plugin for DeaDBeeF audio player
====================

This plugin was inspired by the incredible [foo_musical_spectrum](https://wiki.hydrogenaud.io/index.php?title=Foobar2000:Components/Musical_Spectrum_(foo_musical_spectrum)) plugin for the foobar2000 audio player. It offers variable FFT size (up to 32768), Blackmann-Harris and Hanning window functions, and lots of other options..

## Installation

### Arch Linux
See the AUR ([GTK3](https://aur.archlinux.org/packages/deadbeef-plugin-musical-spectrum-gtk3-git/) /
[GTK2](https://aur.archlinux.org/packages/deadbeef-plugin-musical-spectrum-gtk2-git/))

### Gentoo
See ebuilds [here](https://github.com/megabaks/stuff/tree/master/media-plugins/deadbeef-musical-spectrum)

### Other distributions
#### Build from sources
First install DeaDBeeF (>=0.6) and fftw3 and their development files.

(e.g. on Ubuntu: ```sudo apt-get update && sudo apt-get install build-essential git libgtk2.0-dev libgtk-3-dev libfftw3-dev```)


```bash
make
./userinstall.sh
```

## Screenshot

![Spectrum 1](https://user-images.githubusercontent.com/6108388/70710858-6f132880-1ce0-11ea-9b8e-85cfa711eda8.png)

![Spectrum 2](https://user-images.githubusercontent.com/6108388/71197942-048b5b00-2293-11ea-82fb-b7d2dc80d839.png)

![Settings Dialog](https://user-images.githubusercontent.com/6108388/71197745-95156b80-2292-11ea-9960-a2707d4864d0.png)
