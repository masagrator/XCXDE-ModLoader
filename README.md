# Xenoblade Chronicles X: Definitive Edition Mod Loader
Exlaunch plugin that allows loading files outside of sts.ard/sts.arh.<br>
Additionally allows loading BGM wem files outside of Music.pck without need to edit bgm.bnk to update loops.

Only for Western release, version 1.0.1 and 1.0.2 (TID: `0100453019AA8000`)

# How to install:
1. Download latest release
2. Copy `atmosphere` folder to root of your sdcard

For emulators read guide specific to your emulator about how to install mods.

# How to use:
Copy your files in the same folder tree to
```
atmosphere/contents/0100453019AA8000/romfs/mod/
```
If any folder doesn't exist, create it manually.

So for example if you modified "lib_nx.ini" that is in "monolib/shader", whole path should look like this:
```
atmosphere/contents/0100453019AA8000/romfs/mod/monolib/shader/lib_nx.ini
```

# How to modify sound files:
First we need to unpack Music.pck. You can use this Python 3 script [HERE](https://github.com/masagrator/NXGameScripts/blob/main/XenobladeChroniclesX/pck_unpack_digits.py)<br>
Download it to the same folder where you copied Music.cpk (you can find it in romfs/sound folder)<br>
Example of use in Command Prompt:
```
python pck_unpack_digits.py Music.cpk
```
This will create new folder "Music" with BANKS and STREAMS folders. Look only for files in STREAMS folder.
As we don't know what names are hidden under hashes used for .wem files, there is no easy way to determine which song is which. For discussion about this go here:<br>
https://gbatemp.net/threads/xenoblade-x-replace-music.668397/

To create new audio file use this package made up for Windows: [REPOSITORY](https://github.com/masagrator/NXGameScripts/tree/main/XenobladeChroniclesX/ConvertWavToRIFFOpus) [DOWNLOAD](https://downgit.github.io/#/home?url=https://github.com/masagrator/NXGameScripts/tree/main/XenobladeChroniclesX/ConvertWavToRIFFOpus)

Drop standard .wav file (format s16le) on "DROP_HERE.cmd" if you want it to be played in loop from start to finish, or "DROP_HERE_CUSTOM_LOOP.cmd" if you want to modify where new loop should start. Afaik there is no way to change loop end time that would stop playing previous loop immediately. You need to modify your audio file to end in the way that allows proper looping (the same way was done for game).
Rename converted file to what file you want to replace, and put it to folder:
```
atmosphere/contents/0100453019AA8000/romfs/sound/
```

# Technical details:
For files from sts.ard/sts.arh: Plugin is hooking only one function that is responsible for preparing struct with info about file.
When first time this function is called, code is iterating recursively through whole "mod" folder to hash its paths and store it in cache. Then that cache is used instead of checking if file exists every single time for better performance. In tests each fopen() call was taking around 500 ticks, cache in worst case situation (which is storing info about 100000+ files) takes averagely 13 ticks.

For Music.pck: Plugin is hooking two functions. One is reponsible for loading .wem files, we are setting flag to load it from romfs if such file exists in romfs/sound folder. Second hooked function is responsible for parsing HIRC data that stores info about loops in bgm.bnk. We are iterating through whole file loaded to memory and patch everything at once. This function supports only .wem files that use Opus codec. If you are replacing audio file that was originally using different audio codec, code may need an update since bank files also store which codec is used.
