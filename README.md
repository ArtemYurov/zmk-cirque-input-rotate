# zmk-cirque-input-rotate

A ZMK module that rotates the relative X/Y motion of a Cirque Pinnacle trackpad by a fixed angle.

A round trackpad has no visual "up". When it is mounted at an angle, a natural vertical stroke of the finger moves the cursor diagonally. This module turns that stroke back into vertical cursor motion. ZMK's built-in `zip_xy_transform` can only swap and invert axes, so it handles 90-degree steps only.

Requires ZMK with Zephyr 4.1 or newer (ZMK `main` / v0.4), where the trackpad uses Zephyr's `cirque,pinnacle` driver.

## How it works

The module adds an input pseudo-device that sits between the trackpad and the ZMK input listener:

```
glidepoint (cirque,pinnacle) ──► glidepoint_rotate (zmk,cirque-input-rotate) ──► zmk,input-listener
```

- `REL_X` and `REL_Y` of one packet are rotated together and re-emitted from the pseudo-device.
- All other events (`REL_WHEEL`, `BTN_TOUCH`, `BTN_1`, …) are forwarded unchanged, so taps and the chip's edge scroll keep working.
- Rotation happens before the listener's input processors, so scalers, transforms and layer overrides (for example scroll layers) see rotated motion.
- Fractional motion is carried between packets, so slow movements are not lost. The carry is reset on a zero-motion packet and after a pause of more than 50 ms.

## Installation

Add the module to `config/west.yml`:

```yaml
manifest:
  projects:
    - name: zmk-cirque-input-rotate
      url: https://github.com/ArtemYurov/zmk-cirque-input-rotate
      revision: main
```

Add the rotation node and point your listener at it instead of the trackpad:

```dts
/ {
    glidepoint_rotate: glidepoint_rotate {
        compatible = "zmk,cirque-input-rotate";
        input = <&glidepoint>;
        angle = <(-30)>;
    };

    glidepoint_listener: glidepoint_listener {
        compatible = "zmk,input-listener";
        device = <&glidepoint_rotate>;
        /* input-processors, layer overrides: unchanged */
    };
};
```

The module is compiled only when an enabled `zmk,cirque-input-rotate` node exists. To turn rotation off, set the node's `status = "disabled"` and point the listener back at `&glidepoint`.

Nothing else may listen to `&glidepoint` directly while rotation is on, or its events would be processed twice.

## Properties

| Property | Type    | Description |
|----------|---------|-------------|
| `input`  | phandle | Source input device. |
| `angle`  | int     | Degrees, positive = counterclockwise in the trackpad's raw X/Y frame. Negative values need parentheses: `<(-30)>`. |

The raw frame comes before the listener's processors. If your listener applies `INPUT_TRANSFORM_Y_INVERT`, the rotation looks mirrored on screen, so if the cursor drifts the wrong way, flip the sign.

## Event queue

The module re-emits events from the Zephyr input thread with `K_NO_WAIT`, because that thread is the only reader of the input queue and blocking it would deadlock. When the queue is full, the motion that could not be sent is added to the next packet, and a `LOG_WRN` is logged. A dropped button state is restored by the next packet.

If you see these warnings or lag, increase the queue size:

```
CONFIG_INPUT_QUEUE_MAX_MSGS=32
```

## Testing

The rotation math is plain C and has a host test:

```sh
cc -std=c11 -Wall -Wextra -Werror -I src tests/rotate_math_test.c -o "$TMPDIR/rotate_math_test" && "$TMPDIR/rotate_math_test"
```

## License

MIT
