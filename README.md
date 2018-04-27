# Electron Weather Station
[![DOI](https://zenodo.org/badge/87865768.svg)](https://zenodo.org/badge/latestdoi/87865768)

## Summary
This repository contains the files that I used to create a "cloud-connected" weather station via the [Particle Electron](https://store.particle.io/collections/electron). Please see [my how-to guide](http://tylernigon.me/projects/weather_station.html) describing the steps I took.

## Files
- The [circuit_board](https://github.com/tnigon/Electron_weather-station/tree/master/circuit_board) folder contains items for designing and manufacturing the printed circuit board (pcb).
- The [particle_code](https://github.com/tnigon/Electron_weather-station/tree/master/particle_code) folder contains the .ino files that contain the user-readable Particle code. These can be loaded or copied and pasted into Particle's [IDE](https://build.particle.io/build) to modify the code and/or compile to binary.
- [binary](https://github.com/tnigon/Electron_weather-station/tree/master/binary) contains a blank binary file that can be flashed to the Electron to stop publishing. This came in handy for me when troubleshooting my code and I didn't quite have it right.
- [webhook_templates](https://github.com/tnigon/Electron_weather-station/tree/master/webhook_templates) contains webhook templates for publishing your data to ThingSpeak or Weather Underground. Note that device ID and webhook name should be modified to be used with your device.

## Author
- [Tyler Nigon](https://tylernigon.me)

## License
This project is licensed under the GNU General Public License - see the [LICENSE.md](https://github.com/tnigon/Electron_weather-station/blob/master/LICENSE) for details

## Change log
- v2.0:
- v2.1:
- v2.2: 
