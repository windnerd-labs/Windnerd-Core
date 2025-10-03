# How to program the WindNerd Core

## Download and install Arduino IDE 2 


https://docs.arduino.cc/software/ide-v2/tutorials/getting-started/ide-v2-downloading-and-installing/


## Add support for STM32 boards

Launch Arduino IDE. Click on "File" menu and then "Preferences".

The "Preferences" dialog will open, then add the following link to the "Additional Boards Managers URLs" field:

https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json

## Install WindNerd Core Arduino Library


### Via Library manager (available soon)

1. Go to **Sketch → Include Library → Manage Libraries…**.

2. In the Library Manager window, type `WindNerd Core` in the search bar.

3. Locate the **WindNerd Core** entry in the results.

4. Click **Install**.

5. Once installed, you can access the example sketches via **File → Examples → WindNerd-Core**.


Using the Library Manager is the easiest way to keep the library up to date.



### Manually

To install the library manually:

1. **Download or clone the repository** from [https://github.com/windnerd-labs/Windnerd-Core](https://github.com/windnerd-labs/Windnerd-Core).  
   - If you download as ZIP, extract it.  
   - If you clone with Git:  
     ```bash
     git clone https://github.com/windnerd-labs/Windnerd-Core.git
     ```

2. **Copy the `Windnerd-Core` folder** into your Arduino libraries folder:  
   - **Windows**: `Documents\Arduino\libraries\`  
   - **macOS**: `~/Documents/Arduino/libraries/`  
   - **Linux**: `~/Arduino/libraries/`

3. **Restart Arduino IDE**.  
   You should now see **Windnerd-Core** in **File → Examples**.


## Set the board


1. **Select the board:**
   - Go to **Tools → Board → STM32 Boards (selected via STMicroelectronics)**.
   - Choose **Generic STM32G031F8Px**.

2. **Set the upload method:**
   - Go to **Tools → Upload Method**.
   - For STM32G0 series, choose:
     - **STM32CubeProgrammer (SWD)** if you are using an ST-Link.
     - **Serial** if your board has a bootloader for UART flashing (on RX1 and TX1). This mode needs to be enabled on the board by placing a jumper between pin headers named CLK and 3.3 and repowering the board.  

4. **Upload a sketch** as usual using the **Upload** button.
