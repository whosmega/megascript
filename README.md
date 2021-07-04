# MegaScript 

<h2>A dynamically typed, fast scripting language</h2>

This scripting language is a solo project that I worked on for 2 months, the aim I had was to make a short, fast and capable 
scripting language with it's syntax inspired from both Lua and Python.

<h2>Building</h2>

Currently, the package includes the full compiler and bytecode interpreter, and has to be built from source<br>
A REPL is included for basic testing.<br><br>
A makefile has been included and should build it for Linux, Windows and MacOS
<hr>

Make sure the `git` command line tool, GNU `make` and `gcc` compiler have been installed and added to path (if not by default)
Then, the source can be downloaded to your device using the following command:
```
git clone https://github.com/megahypex/megascript
```
This will create a folder and download the source files into it,<br>
Next, change your directory to the folder
```
cd megascript
```
And issue the `make` command
```
make
```
After this process is complete, a binary executable `mega` should appear in the `megascript` directory (`mega.exe` on Windows)<br>
<h4>Optional Cleanup</h4>
The build object files are no longer needed and can be cleared up using 
```
make clean
```

<h2>Features and Syntax</h2>
Documentation is coming soon
