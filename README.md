# Electron Weather Station
[![DOI](https://zenodo.org/badge/87865768.svg)](https://zenodo.org/badge/latestdoi/87865768)

## Summary
This repository contains the files that I used to create a "cloud-connected" weather station via the [Particle Electron](https://store.particle.io/collections/electron). Please see [my how-to guide](http://tylernigon.me/projects/weather_station.html) describing the steps I took.

## Files
[ElectronWeather_Protoboard.fzz](https://github.com/tnigon/ElectronRainGauge/blob/master/ElectronWeather_Protoboard.fzz) can be loaded into [Fritzing](http://fritzing.org/home/) to interactively view (or update) the wiring diagram.

[blank_firmware.bin](https://github.com/tnigon/ElectronRainGauge/blob/master/blank_firmware.bin) can be loaded to the Particle Electron to stop publishing. This came in handy for me when troubleshooting my code and I didn't quite have it right.

[weather_v2.ino](https://github.com/tnigon/ElectronRainGauge/blob/master/weather_v2.ino) is the Particle code I used for my weather station. This can be copied and pasted into Particle's [IDE](https://build.particle.io/build) to modify the code and/or compile to binary.

The .json files were used to create webhooks to ThingSpeak and Weather Underground. These should be modified for your device.

## Author
- [Tyler Nigon](https://tylernigon.me)

## License
This project is licensed under the GNU General Public License - see the [LICENSE.md](https://github.com/tnigon/Electron_weather-station/blob/master/LICENSE) for details
