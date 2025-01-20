# mel

## Goals

- JIT compilation
    - Maybe AOT too if I finish it
- Common Lisp flavour
    - `(let ((a b) (c d)) ...)`
    - Not an implementation of Common Lisp
    - string, number, arrays, hash-table, etc
    - `[...]` is for new arrays
    - `{...}` is for new hash-tables
- No structs/classes
    - Lua-like metatable "classes" over hash-table
    - defclass macro in stdlib
- Full UTF-8 support
    - Strings, variable names, functions, etc
- Single-header + small as possible
    - Most "features" will go in the stdlib written in mel
    - Keep implementation as clutter-free as possible
- Try to be as portable as possible
    - Probably after first final release

## LICENSE
```
mini embeddable lisp

Copyright (C) 2024 George Watson

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
```
