# RMB
Mouse Panning and Mouse Button binder for Ryujinx(https://ryujinx.org/).<br/>
It's a simple learning project. I wanted to use my mouse and change the camera view position but Ryujinx doesn't support this yet, you have to use third-party software to achieve it. In short, this lets you use your mouse to change the in-game camera view position for some games<br/>
**_For some games?_**<br/>
Well, if your desired game changes its camera view position according to your right-stick/left-stick/d-pad input and if you've bound these inputs with your keyboard then this can help you.<br/>
This also let's you bind mouse buttons with controller buttons such as ZL/ZR.

# Requirements
Just be able to run Ryujinx and for now this only works for Windows and Linux(X11 Window Manager).
But the code structure was designed to work on all the platforms Ryujinx supports, only needs to support native API(s) for specific platforms.

# How to configure?
It simulates keypress of your keyboard according to the mouse position change.<br/>
**_What does it mean by simulate key press?_**<br/>
If you've bound the right-stick(Or the stick which changes the camera) inputs with your keyboard eg. the default Ryujinx input configuration binds the right-stick inputs like this:<br/>
![image](https://i.ibb.co/CbptV6w/image.png)<br/>
then after enabling mouse panning.<br/>
* If you move your mouse **UP** (↑) it will send **_I_** keypress to Ryujinx.<br/>
* If you move **DOWN** (↓) it will send **_K_** and so on.<br/>
* If you've changed these keys in Ryujinx make sure you also change those in RMB's configuration.<br/>
![conf_pic](https://user-images.githubusercontent.com/38256064/163551837-57ecccab-69f2-428b-94c6-ea32d1166d2f.png)<br/>

You can change the **_Sensitivity_** according to your need.<br/>

Now you can bind your mouse buttons with any keyboard key. Example if you want to press the **ZR** key and bound that key with **O** in Ryujinx (the dafualt configuration).<br/>
![ryu_zr_zl](https://user-images.githubusercontent.com/38256064/163552661-93081905-d60b-4480-834f-4ea4b3173c20.png)<br/>
Then you can bind right mouse button or left mouse button with **O** or **Q** or whatever key is mapped to the desired controller button in Ryujinx.<br/>
![mbinds](https://user-images.githubusercontent.com/38256064/163568717-ed7145a2-643b-4002-8e4e-d3e75f0d260a.png)<br/>
* **BTW, make sure 'Bind Mouse Buttons' is ticked.**
* **If you don't want to apply the mouse binding feature at the moment but still want to keep your binding settings just untick 'Bind Mouse Buttons' for the moment.**

# Disclaimer
This is a simple learning project so, things might get broken. Since it simulates key presses according to the mouse position, you might feel the camera movement is a bit choppy and weird/wrong. And since this simulates keypresses, you should close(recommended) or hide/minimize other applications.<br/><br/>
* **_Make sure Ryujinx is running, focused, and in the centre of your screen before enabling mouse panning.._**
* **_Default Panning Toggle Hotkey is Ctrl+F9 if any other application uses the same hotkey it will fail so make sure to choose a unique hotkey or just close other applications._**
* **_If you don't want to bind mouse buttons with your keyboard keys then just simply press the 'Default' button and set the mouse binding keys to None or just untick the 'Bind Mouse Buttons'_**

# How does this work?
1. It uses [GLFW](https://www.glfw.org/) and [ImGui](https://github.com/ocornut/imgui) for the UI part. It directly calls Native functions to handle keyboard and mouse events.
2. It always sets the mouse position to the center of the screen and if the user moves the mouse then it calculates which direction it was moved from the center of the screen and simulates key presses.

# Dependencies
* [GLFW 3.3+](https://github.com/glfw/glfw/) for windows/Linux a custom build was placed in this repo.
* [ImGui](https://github.com/ocornut/imgui) for simple or more like lazy UI.

# How to Build
Output directory: `build/RMB/(Debug|Release)/(platform-x64)/RMB(.exe if windows)`

* Windows:
  - Visual Studio 2019+ with `Desktop development with C++` components installed and `vc143+` should be able to build this. (Yes, no need to manually link ImGui and GLFW libs).
    - `cl.exe nob.c && nob` (Make sure `cl.exe` is in your `PATH` environment variable, typicall open `Developer PowerShell for VS {VERSION}` and run the commands)
        - After running `nob` or the above command you can also directly use Visual Studio to build.
* Linux:
  - Install libx11, mesa and libxtst libs on your Linux machine.
    - On Debian/Ubuntu based Linux distros run `sudo apt install libx11-dev mesa-common-dev libxtst-dev`.
    - On Arch Linux based Linux distros run `sudo pacman -Syu base-devel libx11 mesa libxtst`.
  - After installing these libs simply run `cc nob.c -o nob && nob` on the directory.
* For other OSs/Platforms you need to implement all these stuff listed [here](#for-other-platforms). You also going to need GLFW 3.3+ lib for the corresponding platform/os.

# macOS(Experimental) Build
*After some testing with [@VladimirProg](https://github.com/VladimirProg) or @prog_11765(discord) help the build seems to be stable and the performance seems to be quit okay, so it's safe to use for now.*<br>
<br>
After some requests for macOS support here we are.<br>
Haven't tested properly at all(Can't test in a real environment, don't own a mac device).<br>
Apple's clang doesn't support c++ 20's `jthread` so came up with some weird solution so that it at least builds and checksout basic usage.<br>
It will definately become easier and better when they decides to support it :).<br>
The follwing steps worked on Ventura 13.0.0 should be fine till macOS 12.0 and later versions.
  - Need [HomeBrew](https://brew.sh/)
  - Make sure everything is fine by running `cc --version` you should see some version output. If not then [HomeBrew](https://brew.sh/) was not installed successfully, try again.
  - `brew install gcc` - need GCC version 10+
    - why `gcc` even after Apple stopped supporting it? Well to support c++ 20's `jthread`.
    - (Optional) If the build fails with some header file missing then, run: `g++ --version`(this is gcc's c++ compiler), if you see some output containing `clang`(which is aliased as `g++` by default) then please [WATCH THIS VIDEO](https://youtu.be/0z-fCNNqfEg?t=168).
  - Run: `git clone --depth=1 https://github.com/IamSanjid/RMB.git && cd RMB`
  - Run: `cc -o nob nob.c && ./nob` should succeed with some compiler warnings most of them are safe to ignore.
  - Confirm by running `build/RMB/Release/macos-x64/RMB`, it will ask for accessibility permissions allow those from System Preference.

# For Other Platforms
1. First you need to implement a global hotkey listener, for windows, a dummy window is created and `RegisterHotKey` API is used to register the hotkeys for that window and listened to `WM_HOTKEY` messages on that window. `XGrabKey` is used for X11 window manager.
2. Then you need to implement how to simulate the key presses on the corresponding OS, for windows `SendInput` was enough. For X11 wm `XTestFakeKeyEvent` API was used.
3. You now need to implement how to get the cursor position, for Windows `GetCursorPos` and for X11 wm `XQueryPointer` API(s) was used.
4. After that you need to implement how to set your cursor position, for Windows `SetCursorPos` API was used, same effect can be done by using `SendInput`. For X11 wm `XWarpPointer` did the job.
5. Detect if Ryujinx is active/focused or not. On windows `GetForegroundWindow` and on X11 wm `XGetWindowProperty` API(s) was used, check [here](https://github.com/IamSanjid/RMB/blob/main/linux/linux_native.cpp#L513).
6. (Optional) If you want to hide your cursor during panning mode you need to actually change the cursor image file your system currently using, for Windows `SetSystemCursor` API is used. For X11 wm simple `XFixesHideCursor`/`XFixesShowCursor` API(s) did the job.
7. (Optional) If you want to automatically focus Ryujinx after entering panning mode then again you need to use some native API(s), for windows `SetForegroundWindow` API was used and for X11 wm `XSendEvent` was able to help. On both platforms you have to check all the visible windows and you can either check their class name/title name to select your target processs and send Focus/Active action.
8. (Optional) If you want to add the mouse button to keyboard binder feature, you going to need to listen to global mouse button press/release events, on windows this was achieved via `SetWindowHookEx` and `LowLevelMouseProc` Win32 API, on X11 wm had to use `XRecord` API(s) you can check `XRecordHandler` from [here](https://github.com/IamSanjid/RMB/blob/main/linux/linux_native.cpp#L26). Then it uses the 2nd feature to send the keyboard key press.

Check `native.h` a simple interface that was used as a bridge to call these native API(s).<br/>
Use this as a base class for your specific OS implementation.

# FIN
Again this is a learning project, if anyone wants to help me improve this I would really appreciate it. If you find any bugs or issues let me know by opening an issue. Feel free to pull any request. Thank you for your time.<br/>

You can download binaries for Windows and Linux from [releases](https://github.com/IamSanjid/RMB/releases/).

# Demo
![image](https://github.com/IamSanjid/RMB/assets/38256064/7d7d3e81-3ac6-4b83-9fc2-5da722dd5e70)<br/>

*Videos shows demo with an older version but the main usage remains the same.*
The differences you can notice is:
* No longer need to set the `Camera Update Time`.
* You can specify the emulator target window by providing the name. It means you can also use it on other emulators like **CEMU** just need to configure it correctly.
* You can set more settings regarding mouse panning, such as X and Y offset, range, deadzone etc.

https://user-images.githubusercontent.com/38256064/163562362-4d90e742-52d8-4d20-8f3d-3dc3005962a8.mp4

https://user-images.githubusercontent.com/38256064/163563189-f3498c7e-e15e-4975-8b92-8629f10e2137.mp4

https://user-images.githubusercontent.com/38256064/163569782-a08c5754-62d1-414b-a1a3-4b970b7eaa17.mp4
