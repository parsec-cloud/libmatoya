# tvOS spike

A small app that exercises the UIKit backend the way Parsec uses it: a Metal render
thread that clears and presents (the color cycles over time), with every event printed
to the console. Remote presses and the controller's A button shift the color. Every two
seconds it prints the frame count and, via `diag.m`, the raw state of each controller
GameController can see.

```sh
test/tvos/run.sh sim [seconds]      # tvOS simulator
test/tvos/run.sh device [seconds]   # paired Apple TV
```

The script builds libmatoya for the target, builds and bundles the app, installs it,
launches it with the console attached, and prints the console after `seconds`
(default 60). Output goes to `test/tvos/build/`.

## Device setup (once)

1. Xcode > Settings > Accounts: add an Apple ID. A free personal team works, but its
   profiles expire after 7 days.
2. Pair the Apple TV: on the TV, Settings > Remotes and Devices > Remote App and Devices,
   then pair it from Xcode's devices window.
3. Open `SigningHelper/SigningHelper.xcodeproj`, choose your team under Signing &
   Capabilities, and run it once on the Apple TV. This creates the Apple Development
   certificate and a provisioning profile for `tv.parsec.mty-spike` that includes the
   device. Run it again when the profile expires.

`run.sh device` then finds the device, identity and profile itself. If the bundle ID is
taken for your team, change it in the helper project and set `TVOS_BUNDLE_ID` to match.

## Notes

- Applications built for UIKit targets must compile their `main` as `mty_main`
  (`-Dmain=mty_main`); libmatoya provides `main` and calls it once the window scene
  connects.
- The simulator does not pass controllers paired with the Mac through to tvOS; it only
  exposes a virtual "Gamepad" that never reports input. Test controllers on a device,
  with the controller paired to the Apple TV.
- The simulator does not hold presents to the display rate (it presents well above
  60 fps); a device does.
