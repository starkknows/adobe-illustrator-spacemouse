# SpaceNavigator for Illustrator

Smooth pan and zoom in Adobe Illustrator with a USB 3Dconnexion SpaceNavigator. Adjust speed, direction, and dead zone from Illustrator; settings persist between restarts.

Independent community project. Tested with **Windows x64, Illustrator 2026 (30.8.1), and SpaceNavigator `046D:C626`**. Other models and connection types need additional support. Cap rotation and buttons are not implemented.

### Creator note:

This is something I have wanted for years. I have a very old spacemouse, feel free to add support for newer devices if yours doesn't work. Your favorite coding AI should be able to help. 

## Build

Install these build tools:

- [Visual Studio 2026](https://visualstudio.microsoft.com/downloads/) with **Desktop development with C++**, toolset **v145**, and a Windows SDK.
- [Python 3](https://www.python.org/downloads/windows/), available as `python` on your PATH.
- [**Windows Illustrator 2026 / 30.x C++ SDK** from the Adobe Developer Console](https://developer.adobe.com/console/servicesandapis/ai). Sign in if prompted. See the [Illustrator developer portal](https://developer.adobe.com/illustrator/) or [SDK downloads](https://developer.adobe.com/console/downloads) if the direct link redirects.

Extract Adobe's SDK into the project so this path exists:

```text
sdk/Adobe Illustrator 2026 SDK/illustratorapi/illustrator/AIDocumentView.h
```

From PowerShell in the project root:

```powershell
.\build.ps1
```

For a different Python installation, use `-Python 'C:\path\to\python.exe'`.

The build runs automated checks and creates `out/IllustratorNavigator.aip` and `out/NavigatorInputBridge.exe`. **Python, Visual Studio, and the SDK are only needed to build—not to run the plugin.**

## Install

Close Illustrator, then run this from an **administrator PowerShell** in the project root:

```powershell
.\install.ps1
```

The script copies both runtime files into the default Illustrator 2026 `Plug-ins` folder. For another location:

```powershell
.\install.ps1 -Destination 'D:\path\to\Illustrator\Plug-ins'
```

You can also copy the two files manually; keep them together. Reopen Illustrator after installation. Use the same steps to install updates.

**In 3Dconnexion Settings, disable navigation/scroll mappings for Illustrator's profile.** Otherwise driver scrolling can interfere with the plugin's pan.

## Use

Open a document and select **Window > SpaceNavigator > Enable navigation**.

| Cap movement | Action |
| --- | --- |
| Left/right | Horizontal pan |
| Forward/back | Vertical pan |
| Lift/press | Zoom around the view center |

- **Pan / Zoom:** adjust speed and reverse direction.
- **Advanced:** swap forward/lift axes, reverse individual axes, or adjust the dead zone.
- **Reset settings:** restore default tuning.
- **Device status:** inspect settings and device input.

Increase the dead zone if the view drifts at rest. Settings use Illustrator's preferences API and are written to disk when Illustrator quits normally. Navigation resumes at startup if left enabled; a crash may lose recent changes.

## Troubleshooting

- **No menu:** check the installation folder and restart Illustrator.
- **No movement:** enable navigation and check Device status after touching the cap. Devices found should be `1`, and Translation reports should increase. Confirm the helper EXE is beside the plugin.
- **Jumpy movement:** disable 3Dconnexion's Illustrator navigation/scroll mappings.
- **Build fails:** check the SDK folder layout, Python path, and installed C++ toolset.
- **Install fails:** close Illustrator and run PowerShell as administrator.

Binaries are unsigned; downloaded copies may encounter Windows security warnings or policy blocks.

## Project layout and contributions

| File | Purpose |
| --- | --- |
| `NavigatorPlugin.cpp` | Illustrator menus and native view updates |
| `InputBridge.cpp` | Windows Raw Input and device matching |
| `InputState.h` | Report decoding and movement response |
| `Settings.h` | Saved preferences |
| `InputTests.cpp` | Automated checks |

### Adding another SpaceMouse

Start with the input helper; the Illustrator pan/zoom code can usually stay unchanged.

1. **Identify the device.** Find its USB vendor/product IDs in Device Manager > Properties > Details > Hardware Ids. Record the model and connection type (USB cable, receiver, or Bluetooth); these can expose different IDs and reports.
2. **Inspect reports.** `Accept()` in `InputBridge.cpp` currently filters for `046D:C626`. Add matching for your model, then use temporary diagnostic logging in the helper to capture report IDs, lengths, and bytes while moving one axis at a time and releasing the cap. Do not guess the layout from the device ID alone.
3. **Decode translation.** Extend `DecodeTranslation()` in `InputState.h` and its call site as needed. The current decoder reads report `1` with three signed 16-bit little-endian axes. Other devices may combine translation and rotation or use different formats. Preserve the existing decoder, normalize axes to the current response range (approximately ±350), and publish neutral input when released or disconnected.
4. **Check and build.** Add representative reports to `InputTests.cpp`: positive/negative axes, neutral, truncated, and unrelated reports. Run `build.ps1`, close Illustrator, then run `install.ps1` as administrator.
5. **Test on hardware.** Verify pan/zoom directions, speed, release-to-stop, unplug/reconnect, and switching to another app. Test each connection type you intend to support, then document the model, connection, and Illustrator version in your contribution.

Keep Raw Input registration in the helper process and Illustrator SDK calls in the plugin. Do not commit diagnostic captures containing device serial numbers, personal paths, Adobe's SDK, or generated files.

**Using a coding AI:** point it at this README and ask:

> Help me build and install this plugin, then add support for my [model] using [connection type]. Inspect the device IDs and actual input reports, preserve existing SpaceNavigator support, and add decoder checks. Keep Illustrator view updates in the plugin. Ask me to test physical movement and confirm Illustrator is closed before installation.

An assistant with local file and terminal access can help with setup and implementation. You still need to obtain Adobe's SDK and test the physical device; compatibility cannot be established from code alone.

## Footprint and uninstall

The helper runs only while navigation is enabled and exits when disabled or Illustrator closes. There is no installed service, driver, Windows startup entry, networking, or telemetry.

To uninstall, close Illustrator and remove `IllustratorNavigator.aip` and `NavigatorInputBridge.exe` from its `Plug-ins` folder. Preferences are stored by Illustrator under the `IllustratorNavigator` prefix. This version contains no registry access or migration code.

## License

Original project code and documentation use the [MIT License](LICENSE). Adobe's SDK and sample framework retain their own terms; download the SDK separately and preserve required third-party notices.


