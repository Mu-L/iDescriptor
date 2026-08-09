# This markdown contains issues with Flatpak release

## No iFuse

Flatpak version will not support iFuse for now. It's technically possible however we need to escape the sandbox in order to call fusermount3 (see [fusermount3-wrapper](./packaging/linux/flatpak/idescriptor-fusermount3)). 

Our Flatpak submission was denied beceause we used the `flatpak-spawn --host` option to escape the sandbox.

iFuse is just a feature that allows you to mount your device's filesystem. Without it, you can still use iDescriptor, but you won't be able to mount your device's filesystem.

If you really need it you can either install iDescriptor from arch-aur or use the appimage build.


## Device Is Not Detected or Hot Plug Not Working

If your device is not detected or hot plug is not working, 

First check if you have usbmuxd installed so that iDescriptor can listen for device events.

This will depend on your distribution. Most distributions have a package for usbmuxd, so you should be able to install it using your package manager.

Or sometimes it's shipped with your distribution, for example Arch Linux, Ubuntu, Debian ships them by default.

If usbmuxd is installed then it's most likely due to usbmuxd socket being shut down after the last device was disconnected.

You can patch udev rules to prevent usbmuxd from being shut down (we will open a PR to fix this in libimobiledevice)

You should locate your `39-usbmuxd.rules` file (usually in `/usr/lib/udev/rules.d/` or `/lib/udev/rules.d/`)

For Arch Linux
```bash
sudo cp /usr/lib/udev/rules.d/39-usbmuxd.rules /usr/lib/udev/rules.d/39-usbmuxd.rules.bak
sudo nano /usr/lib/udev/rules.d/39-usbmuxd.rules
```

Comment out the last line

```
# Exit usbmuxd when the last device is removed
#SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ENV{PRODUCT}=="5ac/12[9a][0-9a-f]/*|5ac/190[1-5]/*|5ac/8600/*", ACTION=="remove", RUN+="@sbindir@/usbmuxd -x"
```

Done! Now reload your udev rules

```bash
sudo udevadm control --reload-rules
```

Hot plug should now work as expected.
