# Xenoblade Chronicles X: Definitive Edition Mod Loader
Exlaunch plugin that allows loading files outside of sts.ard/sts.arh.<br>
Only for version Western release, version 1.0.1 (TID: `0100453019AA8000`)

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

# Technical details:
It's hooking only one function that is responsible for preparing struct with info about file. I am checking with `fopen` wrapper called `nn::codec::FDKfopen` if file exists, if yes I am replacing path in that hooked function so it can't find its hash and tries to load it outside of ard.
There is no cache implemented at all. I can accept PR that will create small and fast cache for mod folder.
