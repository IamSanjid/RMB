# RMB
Mouse Panning and Mouse Button binder for Ryujinx(https://ryujinx.org/).

It's a simple learning project. I wanted to use my mouse and change the camera view position but Ryujinx doesn't support this yet, you have to use third-party software to achieve it. In short, this lets you use your mouse to change the in-game camera view position for some games.

##### **_For some games?_**

Well, if your desired game changes its camera view position according to your right-stick/left-stick/d-pad input and if you've bound these inputs with your keyboard then this can help you.

This also let's you bind mouse buttons with controller buttons such as ZL/ZR.

## Requirements
Just be able to run Ryujinx and for now this only works for Windows, Linux(X11 Window Manager) and macOS(12+).
But the code structure was designed to work on all the platforms Ryujinx supports, only needs to support native API(s) for specific platforms.

---

# How to configure?
It simulates keypress of your keyboard according to the mouse position change.

##### **_What does it mean by simulate key press?_**

If you've bound the right-stick(Or the stick which changes the camera) inputs with your keyboard eg. the default Ryujinx input configuration binds the right-stick inputs like this:

![image](https://i.ibb.co/CbptV6w/image.png)

then after enabling mouse panning.

* If you move your mouse **UP** (↑) it will send **_I_** keypress to Ryujinx.

* If you move **DOWN** (↓) it will send **_K_** and so on.

* If you've changed these keys in Ryujinx make sure you also change those in RMB's configuration.

![conf_pic](https://user-images.githubusercontent.com/38256064/163551837-57ecccab-69f2-428b-94c6-ea32d1166d2f.png)


You can change the **_Sensitivity_** according to your need.


Now you can bind your mouse buttons with any keyboard key. Example if you want to press the **ZR** key and bound that key with **O** in Ryujinx (the dafualt configuration).

![ryu_zr_zl](https://user-images.githubusercontent.com/38256064/163552661-93081905-d60b-4480-834f-4ea4b3173c20.png)

Then you can bind right mouse button or left mouse button with **O** or **Q** or whatever key is mapped to the desired controller button in Ryujinx.

![mbinds](https://user-images.githubusercontent.com/38256064/163568717-ed7145a2-643b-4002-8e4e-d3e75f0d260a.png)

* **BTW, make sure 'Bind Mouse Buttons' is ticked.**
* **If you don't want to apply the mouse binding feature at the moment but still want to keep your binding settings just untick 'Bind Mouse Buttons' for the moment.**

You can now unbound the mouse buttons or set to None by clicking on them and pressing Ctrl+C.

---

# Disclaimer
This was a simple learning project so, things might get broken. Since it simulates key presses according to the mouse position, you might feel the camera movement is a bit choppy and weird/wrong. And you should close(recommended) or hide/minimize other applications.



* **_Make sure Ryujinx is running, focused, and is in the centre of your screen before enabling mouse panning.._**

* **_Default Panning Toggle Hotkey is Ctrl+F9 if any other application uses the same hotkey it will fail so make sure to choose a unique hotkey or just close other applications._**

* **_If you don't want to bind mouse buttons with your keyboard keys then just simply press the 'Default' button and set the mouse binding keys to None or just untick the 'Bind Mouse Buttons'_**

---

# How does this work?
1. It uses [GLFW](https://www.glfw.org/) and [ImGui](https://github.com/ocornut/imgui) for the UI part. It directly calls Native functions to handle keyboard and mouse events.

2. It always sets the mouse position to the center of the screen and if the user moves the mouse then it calculates which direction it was moved from the center of the screen and simulates key presses.

---

# Dependencies
* [GLFW 3.4+](https://github.com/glfw/glfw/) for windows/Linux a custom build was placed in this repo.
* [ImGui](https://github.com/ocornut/imgui) for simple immediate mode UI.

---

# How to Build
Output directory: `build/RMB/${Build}/${Platform-Arch}/RMB{.exe; windows}`
  - Build: `Release, Debug`
  - Platform: `macos, linux, windows`
  - Arch: `x64, arm64`

#### Windows:
  - Visual Studio 2019+ with `Desktop development with C++` components installed and `vc143+` should be able to build this. (Yes, no need to manually link ImGui and GLFW libs).
    - Make sure `cl.exe` is in your `PATH` environment variable, typicall open `Developer PowerShell for VS {VERSION}` or `Developer Command Prompt for VS {VERSION}` and run the commands
    - PowerShell:
    ```pwsh
    cl.exe nob.c; .\nob.exe
    ```
    - Command Prompt:
    ```bat
    cl.exe nob.c && nob.exe
    ```

#### Linux:
  - Install required libs.
    - Debian/Ubuntu based distros:
    ```bash
    sudo apt install libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxkbfile-dev libxi-dev libxext-dev libxtst-dev mesa-common-dev
    ```
    - Arch based Linux distros:
    ```bash
    sudo pacman -Syu base-devel libx11 libxcursor libxrandr libxinerama libxkbfile libxi libxext libxtst mesa
    ```
  - After installing these libs simply run `cc nob.c -o nob && nob` in the directory.

#### macOS(Experimental) Build
Thanks to [@VladimirProg](https://github.com/VladimirProg) for help with testing! The build is now stable and performs well — safe to use.

Added macOS support based on user requests. Since I don't own a Mac ourselves, we haven't been able to test it in a real environment, things might not work as expected.

Apple's default compiler doesn't support a modern C++ feature (`jthread`) that RMB requires. So, we'll need llvm 20+ toolchain.

##### **1. Install Homebrew**

Follow instructions from the official [HomeBrew](https://brew.sh/) site.

- Follow the on-screen instructions
- After installation, verify it worked by typing:

```bash
cc --version
```

If you see version information, you're good to go. If not, try following Homebrew's installation guide again.

---

##### **2. Install LLVM (the compiler we need)**

In Terminal, run:

```bash
brew install llvm
```

This installs LLVM version 20 or newer. It may take a few minutes.

---

##### **3. Download RMB**

In Terminal, run:

```bash
git clone --depth=1 https://github.com/IamSanjid/RMB.git && cd RMB
```

This downloads RMB and moves you into the project folder.

---

##### **4. (Optional) Configure LLVM Location**

Most users can skip this step.

If you installed LLVM in a non-standard location, you'll need to tell RMB's build system where to find it.

- Find where LLVM is installed:

```bash
brew --prefix llvm
```

- Open the file `.nob/nob_config.h` in a text editor
- Add this line at the very top (replace `<path>` with the path from the command above):

```c
#define LLVM_TOOLCHAIN "<path>"
```

---

##### **5. Build RMB**

In Terminal, make sure you're in the downloaded RMB folder, you will be if you've followed the previous steps step by step.

Run:

```bash
cc -o nob nob.c && ./nob
```

You'll see some compiler warnings — these are normal and safe to ignore. The build should complete successfully.

---

##### **6. Run RMB**

Depending on your Mac's chip:

- **Apple Silicon (M1, M2, M3, etc.)** — run:

```bash
build/RMB/Release/macos-arm64/RMB
```

- **Intel** — run:

```bash
build/RMB/Release/macos-x64/RMB
```

##### **System Requirements**

- macOS 12.0 (Monterey) or newer
- Older versions (10.x–11.x) should work in theory but haven't been tested
- Tested on Tahoe 26.1


**First-time setup:** macOS will ask for **Accessibility permissions**. You need to allow these for RMB to work properly. Just allow it from the settings.

---

# For Other Platforms
1. First you need to implement a global hotkey listener, for windows, a dummy window is created and `RegisterHotKey` API is used to register the hotkeys for that window and listened to `WM_HOTKEY` messages on that window. `XGrabKey` is used for X11 window manager.

2. Then you need to implement how to simulate the key presses on the corresponding OS, for windows `SendInput` was enough. For X11 wm `XTestFakeKeyEvent` API was used.

3. You now need to implement how to get the cursor position, for Windows `GetCursorPos` and for X11 wm `XQueryPointer` API(s) was used.

4. After that you need to implement how to set your cursor position, for Windows `SetCursorPos` API was used, same effect can be done by using `SendInput`. For X11 wm `XWarpPointer` did the job.

5. Detect if Ryujinx is active/focused or not. On windows `GetForegroundWindow` and on X11 wm `XGetWindowProperty` API(s) was used, check [here](https://github.com/IamSanjid/RMB/blob/e395294392ec2e2cf2a489c10d5286aea95c30ad/src/linux/linux_native.cpp#L513).

6. (Optional) If you want to hide your cursor during panning mode you need to actually change the cursor image file your system currently using, for Windows `SetSystemCursor` API is used. For X11 wm simple `XFixesHideCursor`/`XFixesShowCursor` API(s) did the job.

7. (Optional) If you want to automatically focus Ryujinx after entering panning mode then again you need to use some native API(s), for windows `SetForegroundWindow` API was used and for X11 wm `XSendEvent` was able to help. On both platforms you have to check all the visible windows and you can either check their class name/title name to select your target processs and send Focus/Active action.

8. (Optional) If you want to add the mouse button to keyboard binder feature, you going to need to listen to global mouse button press/release events, on windows this was achieved via `SetWindowHookEx` and `LowLevelMouseProc` Win32 API, on X11 wm had to use `XRecord` API(s) you can check `XRecordHandler` from [here](https://github.com/IamSanjid/RMB/blob/e395294392ec2e2cf2a489c10d5286aea95c30ad/src/linux/linux_native.cpp#L26). Then it uses the 2nd feature to send the keyboard key press.

Check `native.h` a simple interface that was used as a bridge to call these native API(s).

Use this as a base class for your specific OS implementation.

---

# FIN
Again this is a learning project, if anyone wants to help me improve this I would really appreciate it. If you find any bugs or issues let me know by opening an issue. Feel free to pull any request. Thank you for your time.


You can download available binaries from [releases](https://github.com/IamSanjid/RMB/releases/).

---

# Demo
![image](https://github.com/IamSanjid/RMB/assets/38256064/7d7d3e81-3ac6-4b83-9fc2-5da722dd5e70)


*Videos shows demo with an older version but the main usage remains the same.*

The differences you can notice are:
* No longer need to set the `Camera Update Time`.

* You can specify the emulator target window by providing the name. It means you can also use it on other emulators like **CEMU** just need to configure it correctly.

* You can set more settings regarding mouse panning, such as X and Y offset, range, deadzone etc.

https://user-images.githubusercontent.com/38256064/163562362-4d90e742-52d8-4d20-8f3d-3dc3005962a8.mp4

https://user-images.githubusercontent.com/38256064/163563189-f3498c7e-e15e-4975-8b92-8629f10e2137.mp4

https://user-images.githubusercontent.com/38256064/163569782-a08c5754-62d1-414b-a1a3-4b970b7eaa17.mp4
