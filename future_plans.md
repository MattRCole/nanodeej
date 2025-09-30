# Future Plans

Over-all goal: Change the `nano d++` to a client device that will communicate via Serial as a physical volume knob as well as audio input/output device selector.

## Modes

The device will have 5 modes

- Volume mode
- Wildcard volume mode
- Input/Output selection (two discrete but similar modes)
- Input mute toggle momentary mode


### Mode selection

The user will be able to press a chord (2+ keys) to select a mode. We will refer to the keys using A-D.

Examples:

If the user presses A+B: Volume mode will be enabled

If the user presses A+C Wildcard volume mode will be selected etc.

- Each mode should have a visual distinction. This distinction could either be the ring LED color, a distinction on the LCD screen, or the key LED color.

### Volume mode

- User uses keys to select the current application/output device
- The knob will be used to control the volume / mix volume of the selected application/output device

### Wildcard volume mode

- On entering wildcard volume mode:
- User will use knob to select from all available applications/output devices
- Once desired application is selected via knob, user will press confirmation key to select application.
- Knob will transition from application/output selection to volume control mode.

### Input/Output device selection

- User will use knob to scroll through available input/output device
- User will press confirmation button, selected device will be sent to the host
- Nano will transition back to previous mode

### Input mute toggle mode

- A short message will be displayed on the LCD
- Nano will transition back to previous mode
