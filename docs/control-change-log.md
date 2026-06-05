# ArcadeKart Controls Change Log

This file is the running list for intentional input and binding changes on the ArcadeKart work.

## Command Model

- Replaced direct N64 button language in gameplay with bindable command names where the branch has touched input: Throttle, Brake, Jump, Drift, Use Item, Use Item Forward, Use Item Backward, Menu Confirm, Menu Cancel, Open Menu, Clutch, gear commands, Shift Gear Up, Shift Gear Down, Toggle Music, and Toggle HUD.
- Jump and Drift are separate commands. Binding both to one physical button preserves the original hop-then-drift flow; binding Drift alone allows drift without the hop.
- Use Item defaults to the forward item command unless the forward/back axis is below `-0.25`, in which case it uses Use Item Backward.
- Steering is treated as an axis, with wheel steering auto-fed into player 1.

## Keyboard Defaults

- Throttle: Shift.
- Brake: Control.
- Jump: Q.
- Drift: Space.
- Use Item: Z.
- Use Item Forward: X.
- Use Item Backward: V.
- Menu Confirm: Enter.
- Menu Cancel: Escape.
- Open Menu: Enter.
- Clutch: C.
- Gears: 1-6 select gears 1-6, 7 selects reverse, 0 selects neutral.
- Toggle Music: M.
- Toggle HUD: H.
- Menu directions: numpad 8/2/4/6.

## Controller Defaults

- Throttle: right trigger.
- Brake: left trigger.
- Jump: A.
- Drift: left bumper.
- Use Item: right bumper.
- Menu Confirm: A or Start.
- Menu Cancel: B.
- Open Menu: Start.
- Clutch: right stick click.
- Shift Gear Up/Down: right stick up/down, accepted only while Clutch is held.
- Right stick right is no longer a Toggle HUD default.

## Wheel Defaults

- Logitech G27-style wheel steering auto-feeds player 1 Steering.
- Pedals: axis 1 throttle, axis 2 brake, axis 4 clutch, each with its own tuning exponent.
- Wheel POV hat maps to Menu Up/Down/Left/Right.
- Button 1: Menu Confirm.
- Button 2: Menu Cancel.
- Button 3: Toggle HUD.
- Button 4: Open Menu.
- Buttons 7 and 8: Jump.
- Button 21: Toggle Music.
- Left paddle: Use Item Backward.
- Right paddle: Use Item Forward.
- H-pattern shifter buttons map gears 1-6 and reverse; neutral is inferred when no gear is engaged.
- Detected handbrake axis maps to Drift.

## Notes

- Toggle HUD now uses `KART_TOGGLE_HUD_BUTTON` instead of legacy `BTN_CRIGHT`, so old C-right or right-stick-right bindings do not toggle the HUD.
- Race-time post-FX tuning uses Menu Left/Right for screen shake strength and Menu Up/Down for fisheye/barrel warp intensity. In slider-only tuning mode, those two values directly drive the visual effect. Wheel spring tuning no longer consumes Menu Up/Down/Left/Right, and legacy debug lap skip is default-off.
