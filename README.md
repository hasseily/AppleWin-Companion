# AppleWin-Companion

Welcome to AppleWin Companion!
It extends the information visible when running an Apple 2 program, and is especially useful when playing games and wanting to display in-memory information that isn't visible on screen.

This app requires DirectX 12 and Windows 10 x64.
Fork on github or contact me for other builds, but the DirectX 12 requirement is set in stone, sorry.

## Installation

- Copy the AppleWin Companion folder anywhere you want.
- Try to run the app. If it fails to launch, run and install `vc_redistx64.exe` which is included in the release zip.
- If you don't already have a version of AppleWin that is compatible with the Companion, there might be included for convenience a zipped version that you can use. It can also be found at https://github.com/hasseily/AppleWin/releases/tag/v1.29.16.0-C

## Running the program

The companion is made to be the primary window from which to run your game. When launching, to minimize the chances of AppleWin and the Companion being out of sync, you should first run AppleWin and select whatever game you want and set any other options like the video output.

Make sure the option `Remote Control` is set in the configuration window. You can also set AppleWin to run `headless`, which will only send the video to the Companion and reduce computer resource usage.

Then launch AppleWin Companion, load the profile of your choice, and play the game.

## Profiles

The key feature of the Companion is its profiles. A profile is a JSON document that specifies what the Companion should display, and where. The Companion has support for many types of data in memory, including being able to translate numeric identifiers into strings (something very useful when you want to show "Short Sword +1" instead of 0x0b).

The documentation for profiles is sorely lacking, but I've included some sort of profile schema and a number of sample profiles for the game Nox Archaist. Feel free to experiment and ping me for more info.

Happy retro gaming!

Rikkles, Lebanon, 2021.


@rikretro on twitter
https://github.com/hasseily/AppleWin-Companion
