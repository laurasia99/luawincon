# luawincon - low-level Windows Console interface

## Supported functions:

### GetConsoleInputMode()

Returns the current mode of the console's input buffer.
The mode is comprised of the following flags (refer to
Microsoft's reference for [details](https://docs.microsoft.com/en-us/windows/console/getconsolemode)):

- `ENABLE_ECHO_INPUT`
- `ENABLE_INSERT_MODE`
- `ENABLE_LINE_INPUT`
- `ENABLE_MOUSE_INPUT`
- `ENABLE_PROCESSED_INPUT`
- `ENABLE_QUICK_EDIT_MODE` - used with `ENABLE_EXTENDED_FLAGS`
- `ENABLE_WINDOW_INPUT`
- `ENABLE_VIRTUAL_TERMINAL_INPUT`

By default all flags are enabled, except for `ENABLE_WINDOW_INPUT` and
`ENABLE_VIRTUAL_TERMINAL_INPUT`.

Note: it [appears](https://devblogs.microsoft.com/oldnewthing/20130506-00/?p=4453)
that mouse events (movements, clicks) are not processed
when quick-edit mode is enabled.

### SetConsoleInputMode(Integer)

Sets the current mode of the console's input buffer.

### GetConsoleOutputMode():Integer

Returns the current mode of the console's output buffer.

The mode is comprised of the following flags (refer to
Microsoft's reference for [details](https://docs.microsoft.com/en-us/windows/console/getconsolemode)):

- `ENABLE_PROCESSED_OUTPUT`
- `ENABLE_WRAP_AT_EOL_OUTPUT`
- `ENABLE_VIRTUAL_TERMINAL_PROCESSING`
- `DISABLE_NEWLINE_AUTO_RETURN`
- `ENABLE_LVB_GRID_WORLDWIDE`

The flags `ENABLE_PROCESSED_OUTPUT` and `ENABLE_WRAP_AT_EOL_OUTPUT`
are enabled by default.

### SetConsoleOutputMode(Integer)

Sets the current mode of the console's ouput buffer.

### GetConsoleCP():Integer

Returns the code page used for input processing.

### SetConsoleCP(Integer)

Sets the code page used for input processing.

### GetConsoleOutputCP():Integer

Returns the code page used for output processing.

### SetConsoleOutputCP(Integer)

Sets the code page used for output processing.

### GetConsoleScreenBufferInfo()

Returns lua table with basic information about the console.

| Field | Value |
| --- | --- |
| "columns" | Number of columns in screen buffer |
| "rows" | Number of rows in buffer |
| "cx" | Cursor position column |
| "cy" | Cursor position row |
| "attributes" | Default character attributes |
| "left" | Co-ordinate of display window in screen buffer |
| "top" | Co-ordinate of display window in screen buffer |
| "right" | Co-ordinate of display window in screen buffer |
| "bottom" | Co-ordinate of display window in screen buffer |
| "maxcols" | Maximum number of columns in display window |
| "maxrows" | Maximum number of rows in display window |

### FlushConsoleInputBuffer()

Discards all input records in the input buffer.

### GetNumberOfConsoleInputEvents():Integer

Returns the number of unread input records in the input buffer.

### GetNumberOfConsoleMouseButtons():Integer

Returns the number of buttons on the mouse used by the current
console.

### PeekConsoleInput():{events}[]

Returns an array of record events from the console input buffer
without removing them. Each element returned is a table.

Note that the
[menu](https://docs.microsoft.com/en-us/windows/console/menu-event-record-str) and
[focus](https://docs.microsoft.com/en-us/windows/console/focus-event-record-str)
events are filtered out.

#### Key Event

| Field | Value |
| --- | --- |
| "type" | "key" |
| "vkey" | Virtual key [code](https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes) |
| "state" | Control key state |
| "keydown" | Boolean; true if key is pressed |
| "count" | Repeat count when key held down |

#### Mouse Event

| Field | Value |
| --- | --- |
| "type" | "mouse" |
| "x" | X coordinate (column); 0 is the leftmost column |
| "y" | Y coordinate (row); 0 is the topmost row |
| "event" | Type of mouse event |
| "buttons" | Bit field; set if button pressed |
| "state" | Control key state |

The button bit field flags are:

- `FROM_LEFT_1ST_BUTTON_PRESSED`
- `RIGHTMOST_BUTTON_PRESSED`
- `FROM_LEFT_2ND_BUTTON_PRESSED`
- `FROM_LEFT_3RD_BUTTON_PRESSED`
- `FROM_LEFT_4TH_BUTTON_PRESSED`

The mouse event types are:

- `DOUBLE_CLICK`
- `MOUSE_MOVED`
- `MOUSE_WHEELED` - mask `DIRECTION` applies

#### Resize Event

| Field | Value |
| --- | --- |
| "type" | "key" |
| "rows" | Number of rows in resized window |
| "columns" | Number of columns in resized window |

#### Control Key State

- `SHIFT_PRESSED`
- `LEFT_ALT_PRESSED`
- `LEFT_CTRL_PRESSED`
- `RIGHT_ALT_PRESSED`
- `RIGHT_CTRL_PRESSED`
- `CAPSLOCK_ON`
- `NUMLOCK_ON`
- `SCROLLLOCK_ON`
- `ENHANCED_KEY`

### ReadConsoleInput():{events}[]

Returns an array of record events from the console input buffer,
removing them from the buffer. Each element returned is a table.

Note that the
[menu](https://docs.microsoft.com/en-us/windows/console/menu-event-record-str) and
[focus](https://docs.microsoft.com/en-us/windows/console/focus-event-record-str)
events are filtered out.

### WaitForConsoleInput([delay]):Boolean

Wait for an input event. The function accepts a default argument, delay,
of the maximum number of milliseconds to wait for an event
(default: INFINITE). If delay is zero the function will return immediately.
The function returns `true` if an input event was detected; otherwise `false`.

### FillConsoleOutputAttribute(x,y,attr,count)

Sets character attributes for the given number of cells, starting
at the given coordinates.

`attr` is comprised of the following flags:

- `FOREGROUND_RED`
- `FOREGROUND_GREEN`
- `FOREGROUND_BLUE`
- `FOREGROUND_INTENSITY`
- `BACKGROUND_RED`
- `BACKGROUND_GREEN`
- `BACKGROUND_BLUE`
- `BACKGROUND_INTENSITY`

### FillConsoleOutputCharacter(x,y,char,count)

Writes the given character the given number of times, starting
at the given coordinates.

### SetConsoleTextAttribute(attr)

Sets the default attribute of characters written to the console.
Note that this attribute also applies to characters echo to the
console during input processing.

### WriteConsole(String):Integer

Write character string to the console beginning at the current
cursor location. Advances the cursor. Returns number of characters
written.

### GetConsoleCursorInfo():(Boolean,Integer)

Returns two values: a boolean flag (visible) and an integer (size: 1..100).
The boolean flag is true if the cursor is visible; false otherwise.
The size value is (normally) in the range 1..100; with 100 indicating that
the cursor is filling the entire cell.

Note: *The size parameter does not appear to work!*

### SetConsoleCursorInfo(,Boolean,Integer)

Set the visibility and size of the cursor. Both arguments are optional,
and default to true and 100 respectively.

### SetConsoleCursorPosition(x,y)

Set the console cursor position as (x,y); where x is the column (0<=x<max-column)
and y is the row (0<=y<max-row).

### GetConsoleOriginalTitle():String

Returns the original console title.

### GetConsoleTitle():String

Returns the current console title.

### SetConsoleTitle(String)

Sets the current console title.

