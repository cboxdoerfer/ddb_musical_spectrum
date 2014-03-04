Musical Spectrum plugin for DeaDBeeF audio player
====================

This plugin is based on DeaDBeeFs stock spectrum. It currently offers more accuracy (FFT size 16384) and the bars are layed out to represent the musical keys. In the future I will add more features like color customization, select FFT size, display music keys or frequencies - basically most of what [foo_musical_spectrum](http://wiki.hydrogenaudio.org/index.php?title=Foobar2000:Components/Musical_Spectrum_%28foo_musical_spectrum%29) already does. 

## Installation

### Arch Linux
See the [AUR](https://aur.archlinux.org/packages/deadbeef-plugin-musical-spectrum-git/)

### Gentoo
See ebuilds [here](https://github.com/megabaks/stuff/tree/master/media-plugins/deadbeef-musical-spectrum)

### Other distributions
#### Build from sources
First install DeaDBeeF (>=0.6) and fftw3
```bash
make
./userinstall.sh
```

## Screenshot

![](http://i.imgur.com/xmv2RRl.png)
