# T-Display-S3-Wifi-Image-Transfer
Transfer images to the T-Display S3 wirelessly, Cross compatible with both T-Display-S3 LCD and AMOLED.


# Setup 
Windows - Visual Studio Code - Python

Clone Repo
 - T-Display-S3 LCD: https://github.com/Xinyuan-LilyGO/T-Display-S3
 - OR
 - T-Display-S3 AMOLED: https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED

Open in VSCode
 - Open the repo in Visual Studio Code
 - > Click on the extensions tab in the left column → search platformIO → install the first plug-in
 - > Click Platforms → Embedded → Search Espressif 32 in the input box → Select the corresponding firmware installation

Add WebSockets Library
 - In VSCode, Open the PIO Home (The home button bottom left of VSCode window)
 - Go to the Libraries tab
 - Search "WebSockets"
 - The correct library to add is "WebSockets by Markus Sattler". Tested with version 2.4.1, Latest version should work.

Add Code
 - Return to the Exporer tab in VSCode in the left column
 - Expand the "examples" folder
 - Create a new folder, name it something like "WebSocketClient"
 - Create a new file in that folder, name it something like "WebSocketClient.cpp"
 - Expand the "lvgl_demo" folder (AMOLED) or expand the "lv_demos" folder (LCD)
 - (AMOLED ONLY) Copy pins_config.h rm67162.cpp and rm67162.h into to the new folder you created
 - (LCD ONLY) Copy pin_config.h into the new folder you created
 - Open the new .cpp file you created, it should be empty
 - Copy the code from "WebSocketClient.cpp" located in this repo into the file you created.

Modify Code for your Platform
 - open the .cpp file you created and copied code into
 - (AMOLED ONLY) the first line of the file should be #define PLATFORM_LCDA
 - (LCD ONLY) the first line of the file should be #define PLATFORM_LCD

Modify platformio.ini
 - In the Explorer tab in VSCode open the platformio.ini file
 - (AMOLED ONLY) change src_dir = to src_dir = examples/WebSocketClient (use the name of the folder you created earlier)
 - (LCD ONLY) change default_envs = to default_envs = WebSocketClient (use the name of the folder you created earlier)
