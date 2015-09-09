# vui

User interface library that provides a Vi-like status bar and command line and input system for interactive applications written in C and C++.

Currently under heavy construction, meaning the API is very likely to change, but it is currently generally usable.  

The library itself uses no external dependencies (although the demo does).

## Features

* Basic Vim-like commands, modes, cmdline, and macros
  * each cmdline type has its own history
  * showcmd
  * count
* UTF-8 support (sort of)
  * allows states to pretend they have more than 256 transitions by converting input Unicode codepoints to UTF-8 first
* Contains general-purpose deterministic finite state machine library
  * can be used independently of vui, although they are currently part of the same library

## Planned/Under Development Features

* fragments and combinators
  * function to create a fragment from a regexp
* DFA-based parser for cmdline and other things


## Demo
vui comes with a ncurses-based demo in `vuitest.c`.  Clone or download the repository and run `make vuitest && ./vuitest` in one terminal and `tail -f log` in another.  

`:q<Enter>`, `Q`, and `ZZ` all quit.  `:` and `/` enter Vi-like command and search modes.  Search mode does nothing (other than make a log entry), but command mode has some commands:
* `:q`: quit
* `:map action reaction`: map `action` to `reaction`

If given a count, `Q` and `ZZ` log the count instead of quitting.

`q<register>` records, `q` ends recording, and `@<register>` plays back a macro.

Macros currently are very buggy, and will very readily cause stack overflows, even if they don't call themselves.

## Using vui in your own program
Run `make libvui.a` and then link the resulting file `libvui.a` into your own program.

Before using any features of vui, you must call `vui_init(width)`, where `width` is the desired width, in characters, of the vui bar.  

The contents of the vui bar are stored in `char* vui_bar`.  This string should be drawn to the screen regularly by the user, preferably after each call to `vui_input`.  The address contained in `vui_bar` changes depending if vui is in status or command mode, so you should not cache it.  The variable `int vui_crsrx` contains the cursor position in the bar, where `-1` means cursor should be hidden and `n` means the cursor should be to the left of character `n`.  To change the width of the vui bar (e.g. if the screen gets resized), call `vui_resize(newwidth)`, where `newwidth` is the new desired width.  

To feed user input into vui, call `vui_input(key)`, where `key` is the value of the entered keystroke.  `key` may be any value from 0 to 0x7FFFFFFF (UTF-8 codepoint range).  

To create a new mode, call `vui_mode_new` (see `vui.h:67`).  To create a new command line type (like `:` or `/` in Vi) and to register an on submit callback (of type `vui_cmdline_submit_callback` at `vui.h:39`), call `vui_cmdline_type_new` (see `vui.h:77`)

To add commands like `ZZ`, see functions like `vui_set_string_t` (defined at `statemachine.h:65`).  

