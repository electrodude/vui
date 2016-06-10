# vui

User interface library that provides a Vi-like status bar and command line and input system for interactive applications written in C and C++.

Currently under heavy construction, meaning the API is very likely to change, but it is currently generally usable.

The library itself has no external dependencies, although the demo depends on ncurses.

This library is the result of me being fed up by the fact that every program wtih a `vi`-like interface has some stupid inconsistency.  It is also a result of the fact that I am *way* too excited by state machines.

## Features

* Basic Vi-like features
  * modes
  * cmdlines
    * line editing
    * register insertion
    * separate history per cmdline definition
    * cmdline parser
  * registers
  * showcmd
  * count
  * map, noremap
  * macros
* UTF-8 support (sort of)
  * allows states to pretend they have more than 256 transitions by converting input Unicode codepoints to UTF-8 strings first
  * functions ending in `_u` accept Unicode codepoints and convert them to UTF-8 strings for you
* Contains general-purpose deterministic finite state machine library
  * can be used independently of vui, although they are currently part of the same library
  * uses mark-sweep garbage collector for cleanup
  * can output (pretty messy) Graphviz representation of a state machine

## Planned/Under Development Features

* fragments and combinators
  * function to create a fragment from a regexp
* DFA-based parser for cmdline and other things
  * currently used by `vuitest` to unescape arguments to `:map`
  * I've only tried it with regular languages so far, but I think it will eventually be able to parse any deterministic context-free language
  * will support at least the following types: `vui_string`, `vui_state`, `vui_stack` of `vui_state`s, `vui_frag`.
  * `vui_fragf`: varargs function sort of like `sprintf`, but that makes a state machine fragment instead of a string
* saving and loading state machines to disk


## Demo
vui comes with a ncurses-based demo in `vuitest.c`.  Clone or download the repository, `cd` into it, and run `make vuitest && ./vuitest`. As you push keys, text will appear in an ncurses window on the left half of the screen.  The same text is appended to a file `log`.

Hopefully, `vuitest` will one day become a full-fledged state machine editor with a `vi`-like interface.

`:q<Enter>`, `Q`, and `ZZ` all quit.  `:` and `/` enter Vi-like command and search modes.  Search mode does nothing (other than make a log entry), but command mode has some commands:
* `:q`: quit
* `:map action reaction`: map `action` to `reaction`

If given a count, `Q` and `ZZ` log the count instead of quitting.

`q<register>` records, `q` ends recording, and `@<register>` plays back a macro.

`CTRL-r<register>` while editing a cmdline inserts that register.

`gv` outputs a Graphviz file `vui.dot` in the current working directory of the main state machine.

`gc` runs the garbage collector.  This isn't really very interesting, unless debug output is on, and is only useful for debugging the garbage collector.

## Using vui in your own program
Run `make libvui.a` and then link the resulting file `libvui.a` into your own program.

Before using any features of vui, you must call `vui_init(width)`, where `width` is the desired width, in characters, of the vui bar.

The contents of the vui status and command line are stored in `char* vui_bar`.  This string should be drawn to the screen regularly by the user, preferably after each call to `vui_input`.  The address contained in `vui_bar` changes depending if vui is in status or command mode, so you should not cache it.  The variable `int vui_crsrx` contains the cursor position in the bar, where `-1` means cursor should be hidden and `n` means the cursor should be to the left of character `n`.  To change the width of the vui bar (e.g. if the screen gets resized), call `vui_resize(newwidth)`, where `newwidth` is the new desired width.

To feed user input into vui, call `vui_input(key)`, where `key` is the value of the entered keystroke.  `key` may be any value from 0 to 0x7FFFFFFF (UTF-8 codepoint range).

To create a new mode, call `vui_mode_new` (around `vui.h:198`).  To create a new command line type (like `:` or `/` in Vi) and to register an on submit callback (of type `vui_cmdline_submit_callback` around `vui.h:50`), call `vui_cmdline_new` (around `vui.h:208`).

To add commands like `ZZ`, see functions like `vui_bind` (around `vui.h:163`).

