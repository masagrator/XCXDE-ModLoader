# XCXDE Mod Loader
Exlaunch plugin that allows loading files outside of sts.ard/sts.arh

# How to install:
1. Download latest release
2. Copy `atmosphere` folder to root of your sdcard

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
This plugin is hooking only one function that is responsible for getting ready to load file. I am checking with wrapper around `fopen` if file exists, if yes I am replacing path in that hooked function so it can't find its hash and tries to load it outside of ard.
