# IntelVistaEth

This is a Windows Vista (x64 only) driver for modern Intel ethernet cards, and its based on the Linux e1000e ethernet driver.

<sup><sub>couldn't think of a decent name...</sub></sup>

# Supported Models:

- I217-LM
- I217-V
- I218-LM
- I218-V
- I219-LM
- I219-V

The driver supports most (if not all) of their revisions. For supported specific ids, refer to DEVIDS.md

# Installing

1. Driver signature enforcement MUST be disabled, if you are on RTM you can with:
```
bcdedit /set nointegritychecks ON
```

or if you have SP1+, do so by rebooting, pressing F8 when Windows boots and selecting "Disable Driver Signature Enforcement"

2. Open Device Manager, right click on your device and click "Update Driver"

3. Click on "Browse my computer for drivers" then on "Let me pick from a list of available drivers on my computer"

4. Click on "Have disk" then on "Browse" and select `e1000ev.inf`.

5. Install it.

# Building

To build this driver from source, you must get the Windows Driver Kit at version 7.1.0, which you can get [here](https://www.microsoft.com/en-us/download/details.aspx?id=11800)

After installing it, you just simply run `build.cmd`, then the compiled driver should be at the `package` folder ready to install.

# Bug reports

If you got an issue with the driver, feel free to open an issue in the Issues tab of this repository.

It must include a brief description of the issue, your device model and ID, and if applicable, steps to reproduce.

# Disclaimer

This project is fully independent and it is NOT affiliated with, maintained or endorsed by **Intel** or **Microsoft**. All product names and brands are property of their respective owners.
