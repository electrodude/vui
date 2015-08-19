# vui

User interface library that provides a Vi-like status bar and command line and input system for interactive applications written in C and C++.

Currently under fairly heavy construction, meaning the API is very likely to change, but it is currently generally usable.  

The library itself uses no external dependencies (although the demo does).

## Demo
vui comes with a ncurses-based demo in `vuitest.c`.  Clone or download the repository and run `make vuitest && ./vuitest` in one terminal and `tail -f log` in another.  `:q<Enter>`, `Q`, and `ZZ` are quit.  `:` and `/` enter Vi-like command and search modes, although entered commands don't do anything besides log that they were submitted (with the exception of `:q`, which quits).  

## Using vui in your own program
Run `make libvui.a` and then link the resulting file `libvui.a` into your own program.

Before using any features of vui, you must call `vui_init(width)`, where `width` is the desired width, in characters, of the vui bar.  

The contents of the vui bar are stored in `char* vui_bar`.  This string should be updated on the screen regularly by the user, preferably after each call to `vui_input`.  The address contained in `vui_bar` changes depending if vui is in status or command mode, so you must read it regularly.  The variable `int vui_crsrx` contains the cursor position in the bar, where `-1` means cursor should be hidden and `n` means the cursor should be to the left of character `n`.  To change the width of the vui bar (e.g. if the screen gets resized), call `vui_resize(newwidth)`, where `newwidth` is the new desired width.  

To feed user input into vui, call `vui_input(key)`, where `key` is the value of the entered keystroke.  `key` may be any value from 0 to 0x7FFFFFFF (UTF-8 codepoint range).  

To create a new mode, call `vui_mode_new` (see `vui.h:67`).  To create a new command line type (like `:` or `/` in Vi) and to register an on submit callback (of type `vui_cmdline_submit_callback` at `vui.h:39`), call `vui_cmdline_type_new` (see `vui.h:77`)

To add commands like `ZZ`, see functions like `vui_set_string_t` (defined at `statemachine.h:65`).  

