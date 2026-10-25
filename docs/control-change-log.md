# ArcadeKart Controls Change Log

This file is the running list for intentional input and binding changes on the ArcadeKart work.

## Touch And Mouse Menus (2026-10-16)

### Radial Pedals And Camera Placement

- Gas is the center of a four-sector ring: Jump above, Drift right, Brake below,
  Item left. Existing pedal icons retained; Jump uses the Lucide up arrow.
- A direct gas press holds throttle until that finger lifts, including while
  crossing a ring action. Ring actions hold only while the finger is inside the
  sector, and release on exit/lift. Sliding into gas never begins throttle.
- Free fingers can enter the ring; steering/camera/shifter and utility fingers
  keep their existing ownership. Multitouch, hide/focus/cancel release retained.
- Camera stick sits inward beside the minimap, rear-view button directly above.
  Manual shifter sits left of the pedal ring. Controls-inward/HUD-outward remains.
- Place number uses the same scaled edge inset as lap times, anchored by the
  rendered quad edge even during the finish-line enlargement.
- User requested no autonomous tests for this revision. Compiled and packaged;
  interactive/visual feel is pending user testing.

- Native menu choices accept mouse clicks and Android touches, activating on
  release over the same choice. Modals own their targets and context changes
  cancel held presses. Existing controller mappings remain unchanged.
- Added visible Back actions, a larger transmission picker, paged race/view
  settings with arrows, tappable pause/results choices and course Random.
- Android gameplay controls hide in menus, pause, results, demos and port UI.
  All held controls clear on hide/focus loss. A new touch is required after
  returning to gameplay; fingers held across a transition do not reactivate.

## Touch Camera Stick And Mirror (2026-10-16)

- Added an independent blue camera stick inward of the shifter/action buttons,
  with an eye symbol. It feeds existing first-person look axes, springs back on
  release and leaves steering/throttle/shifter pointers independent.
- Rear-view mirror icon holds existing Look Behind (C-left/0x2):180 degrees,
  overriding look-stick direction while held. Release resumes a still-held look
  stick or returns forward. This is not a latched toggle or live mirror window.
- Look axes occupy the last16 bits of the coherent64-bit Android touch snapshot.
  Native pad merge handles them like physical right-stick axes. Controller/VR
  mappings and camera math unchanged; network look remains local.
- Cancel, focus loss and layout changes clear both look axes and rear-view hold.

## Pedals And Steering Art (2026-10-16)

- Swapped the steering art: large outer wheel, movable silver inner ring. The
  wheel turns with horizontal thumb input (up to50 degrees each way), then
  recenters on release/cancel. Steering input mapping and travel are unchanged.
- Replaced the accelerator gauge and brake octagon with original rendered pedal
  sprites: tall accelerator with vertical ribs, wide brake with horizontal ribs.
- Minimap now fills the entire gap between lap times and RPM, keeping its aspect
  ratio and outer-edge alignment. The prior symmetric fit region wasted space
  above it; the current phone track is63% taller. Corner blocks do not move.

## Android Gated Shifter (2026-10-16)

- Touch art pass: stock-HUD-inspired beveled colored rims, shaded action icons,
  steering wheel, gold knob, recessed gate and outlined gold/lavender gear labels.
  Pressed buttons brighten; clutch interaction lights the gate gold. All input
  mappings, hit targets, positions, short travel and wide corner tolerance remain
  unchanged. See `docs/touch-controls-art.md` for asset source and visual checks.

- Manual transmission displays a touch lever: 1/3/5 above, 2/4/6 below, reverse
  at the far right of the neutral crossbar. The lever stays on the gate and must
  pass through neutral before moving between columns. Released detents latch;
  releasing partway along a slot returns to neutral.
- Touching/dragging the lever holds the clutch; lifting the finger releases it.
  No separate clutch button. This matches the user's explicit preference.
- Gear requests use the existing direct-gear commands, clutch and shift scoring.
  Cancel/focus loss clears held inputs. Steering uses an independent touch pointer.
- Gate and action buttons stay inward of the outer HUD in the preferred A layout.
  The four action icons sit above the gate in manual mode; automatic mode keeps
  its previous controls. Existing gamepad/keyboard bindings are unchanged.
- User-requested ergonomic revision: lever travel shortened20%, visible lane width
  increased2.18x, and physical corner/slot-entry tolerance at least doubled.
- Android touch now merges before networking and the isolated test input queue.
  The test queue supports all32 command bits, including Neutral, and attributes
  validation errors to the submitted command instead of leaving clients waiting.

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
