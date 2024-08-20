# T-Display-S3-Wifi-Image-Transfer
Transfer images to the T-Display S3 wirelessly, Cross compatible with both T-Display-S3 LCD and AMOLED.


# Setup 
Windows - Visual Studio Code - Python

Clone Repo
   T-Display-S3 LCD: https://github.com/Xinyuan-LilyGO/T-Display-S3
   OR
   T-Display-S3 AMOLED: https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED

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
