# JSON over Serial communication protocol

## Universal message settings

All messages regardless of origin will contain a `type` designation.

If any message has a `type` of `debug`, it may be acknowledged by the receiving application, but will have no effect (IE: host may log incoming nano debug messages).

## Host to Nano

### app-dev config message

Purpose: Notify nano of application/device configuration updates.

Can be used for:

- bulk updates
- single item updates (array size of one)
- may be in response to a request for update

Example message:

```json
{
    "type": "app-dev-config",
    "info": [
        {
            "id": "spotify",
            "name": "Spotify",
            "type": "application",
            "currentDetent": 43,
            "detents": 100,
            "muted": false,
            "deejConfig": {
                "keyColor": "#00FEFE",
                "ringPrimary": "#FF0000",
                "ringSecondary": "#00FE00",
                "haptics": "smooth",
            }
        },
        {
            "id": "usb-speakers",
            "name": "Speakers",
            "type": "output-device",
            "currentDetent": 32,
            "detents": 50,
            "muted": false,
            "deejConfig": {
                "keyColor": "#99FF99",
                "ringPrimary": "#F000F0",
                "ringSecondary": "#0000FE"
            }
        },
        {
            "id": "shure-mv6",
            "name": "Shure MV6",
            "type": "input-device",
            "currentDetent": 100,
            "detents": 100,
            "muted": true,
            "deejConfig": null
        },
    ]
}
```

Annotated message schema (using typescript):

```typescript
type AppDevConfig = {
   type: "app-dev-config",
    info: {
        /** unique identifier, not necessarily human readable. MUST be unique */
        "id": "spotify",
        /** Human readable identifier, SHOULD be unique */
        "name": "Spotify",
        /**
         * One of the following options:
         *  - application
         *  - input-device
         *  - output-device
         * While the exact definitions of how the type is determined is decided by the host system, generally:
         * - an application is a virtual source of audio that is meant to be mixed with other virtual sources of audio for play-back over an output device
         * - an output device is a speaker/speakers or recording device. Primarily
         * - an input device is typically a microphone. It's audio is typically not meant to be sent to an output device
         */
        "type": "application" | "input-device" | "output-device",
        /** the current "volume" of the device or application. */
        "currentDetent": number,
        /** The configured amount of volume steps available for this device. */
        "detents": number,
        /** If the app-device is currently in a muted state */
        "muted": false,
        /**
         * Device specific configuration for the Nano.
         * If this key is omitted, or any sub-property is omitted, the Nano will fall back to any previously set config for the current device or application.
         * If this key or any sub-property key is specifically set to `null`, it will be set to the default configuration value
         */
        "deejConfig"?: {
            /** A CSS hex color string. This determines the un-pressed LED color of the corresponding mapped key (assuming that the application is mapped) */
            "keyColor"?: string | null,
            /** A CSS hex color string. This determines the primary color of the LED volume ring. It represents the percentage of the current volume/detent. */
            "ringPrimary"?: string | null,
            /** A CSS hex color string. This determines the secondary color of the LED volume ring. It represents the remaining percentage of the remaining volume/detents. */
            "ringSecondary"?: string | null,
            /** A CSS hex color string. This determines the pointer color of the LED volume ring. It displays the current detent/volume selected on the LED ring. */
            "ringPointer"?: string | null,
            /** A general feeling when selecting the volume of this device */
            "haptics"?: "smooth" | "coarse" | null
        } | null
    }[]
}
```

### app-dev key-mapping

Purpose: notify the nano of the current key-mapping for a given application or device.

Example message:

```json
{
    "type": "app-dev-key-mapping",
    "info": ["spotify", "chrome", "usb-speakers", "special__FULLSCREENGAME"]
}
```

`ids` MUST be an array of position sensitive unique device/application identifiers.

### deej-defaults

Purpose: update the device defaults of the nano. If the Nano needs a config value, but the host has not provided one, these will be the values used.

Example message:

```json
{
    "type": "deej-defaults",
    "info": {
        "keyColor": "#A0A0A0",
        "ringPrimary": "#00AF00",
        "ringSecondary": "#AF0000",
        "ringPointer": "#FEFEFE",
        "haptics": "smooth"
    }
}
```

All of these keys can be found above, meanings and options are the same.

## Nano to host

### volume-update

Purpose: notify host system of a volume change for a specific application/device
```json
{
    "type": "volume-update",
    "delta": -5,
    "absolute": 43,
    "id": "spotify"
}
```

`delta`: the amount of detents changed since the last sent update
`absolute`: The non-source of truth volume of the device or application.
`id`: The unique identifier of the application or device

### Mute toggle

Purpose: notify host system that user wishes to toggle mute status of a specific application/device

```json
{
    "type": "mute-toggle",
    "id": "shure-mv6"
}
```


### configuration request

Purpose: Request the status/configuration for one or more applications or defaults.

The host SHOULD respond with an `app-dev-config` message that has the requested information.

```json
{
    "type": "config-request",
    "ids": [],
    "types": [],
    "brief": true
}
```

`ids`: A list of unique identifiers that the nano would like information on.
`types`: A list of device classes (valid options are `application`, `input-device`, `output-device`)
`brief`: if `true`, the host SHOULD respond without the `deejConfig` option in its `app-dev-config` response message.

### key-map-request

Purpose: Request the currently mapped applications/devices

The host SHOULD respond to this message with a `app-dev-key-mapping` message.

```json
{
    "type": "key-map-request"
}
```
