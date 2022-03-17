# RMB
Mouse Panning for Ryujinx(https://ryujinx.org/).<br/>
It's a simple learning project. I wanted to use my mouse and change the camera view position but Ryujinx doesn't support this yet, you have to use third-party software to achieve it.<br/>
In short, this lets you use your mouse to move the in-game camera view position for some games.<br/>
**_For some games?_**<br/>
Well, if your desired game changes its camera view position according to your right-stick/left-stick/d-pad input and if you've bound these inputs with your keyboard then this can help you.


# Requirements
Just be able to run Ryujinx and for now this works for Windows only.
But the code structure was designed to work on all the platforms Ryujinx supports, only need to support native API(s) for specific platforms.

# How to configure?
It simulates key press of your keyboard according to the mouse position change.<br/>
**_What does it mean by simulate key press?_**<br/>
If you've bound the right-stick(Or the stick which changes the camera) inputs with your keyboard eg. the default Ryujinx input configuration binds the right-sitck inputs like this:<br/>
![image](https://i.ibb.co/CbptV6w/image.png)<br/>
then after enabling mouse panning it will send **_I_** key press to Ryujinx if you move your mouse cursor **UP**<br/>
**_K_** if you move **DOWN** and so on.<br/>
If you've changed these keys in Ryujinx make sure you also changed those in RMB's configuration just click on the **Configure Input** and change accordingly.<br/>
![image](https://i.ibb.co/r4Qjnyr/image.png)<br/>

You can change the **_Sensitivity_** and **_Camera Update Time_** to adjust your needs.

# Disclaimer
This is a simple learning project so, things might get broken. Since it simulates key presses according to mouse position change, you might feel the camera movement is a bit choppy and wrong. And since this simulates key presses, you should close(recommended) or hide/minimize other applications.<br/>
**_Make sure Ryujinx is running, focused and is in center of your screen before enabling mouse panning._**

# How this works?
1. It uses GLFW(https://www.glfw.org/) to monitor your mouse position changes. It creates an invisible window size of the primary monitor's screen. Then in it's own thread checks for mouse position.
2. It always sets the mouse position to the center of the screen and after moving the mouse it calculates which direction it was moved from the center of the screen and simulates key presses.

# Dependencies
GLFW 3.4(Build from the official github repo for your specific OS) for windows a custom build was placed in this repo.

# For Linux and other OSs
1. First you need to implement global hotkey listener, for windows I made a dummy window and used `RegisterHotKey` API to register hotkeys and listened to `WM_HOTKEY` messages on that window. On Linux I think something similar can be done using `XGrabKey`(X11 window manager) I don't have much experience.
2. Then you need to implement how to simulate key presses on your corresponding OS, for windows `SendInput` was enough. On Linux I think you can look for how `xdotool` works.
3. After that you need to implement how to set your cursor position, for Windows used `SetCursorPos` same effect can be done with `SendInput`. For Linux again look how `xdotool` works.
4. (Optional) If you want to hide your cursor during panning mode you need to actually change the cursor image file your system currently using, fow Windows used `SetSystemCursor` API. For linux google can help I suppose.
5. (Optional) If you want to automatically focus Ryujinx after entering panning mode then again you need to use some native API(s), for windows `SetForegroundWindow` was used and I think on linux `xdotool` should be able to help again.

Check `native.h` a simple interface which was used as a bridge to call these native API(s).
Use this as a base class for your specific OS implementation.

# FIN
Again this was a learning project, if anyone want to help me improve I would really appericiate it. If you find any bug or issue let me know by opening an issue. Feel free to pull request any improvement you've made. Thank you for your time.
