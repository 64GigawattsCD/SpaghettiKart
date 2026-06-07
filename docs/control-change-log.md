# ArcadeKart Controls Change Log

This file is the running list for intentional input and binding changes on the ArcadeKart work.

## Command Model

- Replaced direct N64 button language in gameplay with bindable command names where the branch has touched input: Throttle, Brake, Jump, Drift, Use Item, Use Item Forward, Use Item Backward, Menu Confirm, Menu Cancel, Open Menu, Clutch, gear commands, Shift Gear Up, Shift Gear Down, Toggle Music, and Toggle HUD.
- Jump and Drift are separate commands. Binding both to one physical button preserves the original hop-then-drift flow; binding Drift alone allows drift without the hop.
- Use Item now resolves to three states from the forward/back axis: Forward at `>= 0.25`, Backward at `<= -0.25`, and Neutral in between. Neutral lets the item choose its natural default direction; bananas and banana bunch drops default backward, shells default forward.
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
- Capture Screenshot: F12.
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
- Capture Screenshot: Select/Back/View.
- On character select, L/R toggles that player's transmission mode between Manual and Auto.

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
- Transmission mode is per human player. AI racers are forced automatic for now.
- Race-time post-FX tuning moved back to menu CVars; Menu Up/Down/Left/Right are no longer consumed by post-FX or wheel spring tuning, and legacy debug lap skip is default-off.
- Capture Screenshot writes full-window PNGs into `C:\Users\Craig\OneDrive\Documents\mk64 Arcade\screenshots`.
