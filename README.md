# Oh-In-Pe- Project

## Overview

The Oh-In-Pe- project is a sound modulation project using the STM32F767 microcontroller. It includes features such as an equalizer, piano, and WAV file playback.

## Features

- **Equalizer**: Adjust volume, bass, and treble.
- **Piano**: Simulate piano keys and play notes.
- **WAV Playback**: Play audio files from an SD card.

## File Structure

### main.c

The `main.c` file contains the core logic of the project. Here are the main components:

### Constants

- **Theme Colors**: Defined colors for UI elements.
- **Key Dimensions**: Dimensions for piano keys.
- **Notes Frequencies**: Frequencies for musical notes.
- **Slider and Bar Settings**: Configuration for UI sliders and bars.
- **Buffer Settings**: Sizes for audio buffers and headers.

### Data Structures

- **`KeyInfo`**: Stores coordinates for piano keys.
- **`Slider`**: Stores slider values and positions.
- **`WAV_Header`**: Stores metadata for WAV files.

### Functions

#### Initialization

- **`System_Init`**: Initializes the system and peripherals.
- **`TIM1_Init`**: Initializes Timer 1 for audio playback.

#### UI Functions

- **`MainScreen`**: Displays the main screen.
- **`Menu_Equalizer`**: Displays the equalizer menu.
- **`Menu_Piano_WAV`**: Displays the piano and WAV menu.
- **`Draw_MainScreen`**: Draws the main screen UI.
- **`Draw_Equalizer_UI`**: Draws the equalizer UI.
- **`Update_Equalizer_UI`**: Updates the equalizer UI based on user input.
- **`Draw_Piano_WAV_UI`**: Draws the piano and WAV UI.
- **`Draw_Keys`**: Draws the piano keys.

#### Input Handling

- **`Get_Only_Key_Input`**: Handles key input.
- **`Get_Touch_Input`**: Handles touch input.
- **`Key_Touch_Handler`**: Handles touch input for piano keys.
- **`Piano_Input_Handler`**: Handles piano input.

#### Audio Functions

- **`Play_Note`**: Plays a musical note.
- **`Reset_Key_State`**: Resets the state of piano keys.
- **`Process_Equalizer_Key1`**: Processes input for equalizer key 1.
- **`Process_Equalizer_Key2`**: Processes input for equalizer key 2.
- **`Process_Equalizer_Key3`**: Processes input for equalizer key 3.
- **`Initialize_Peripherals`**: Initializes peripherals for audio playback.
- **`Play_Audio`**: Plays audio from the buffer.
- **`Update_WAV_Display`**: Updates the WAV display.
- **`TFT_Filename`**: Displays the filename of the current WAV file.
- **`Check_valid_increment_file`**: Checks for valid WAV files when incrementing.
- **`Check_valid_decrement_file`**: Checks for valid WAV files when decrementing.

### Global Variables

- **Menu Selection**: Tracks which menu is currently selected.
- **Graph Piano Mode**: Tracks if the piano mode is active.
- **Keys and Touch States**: Tracks touch states for piano keys.
- **Slider Configurations**: Stores slider values and positions.
- **WAV File Data**: Stores data related to WAV files and playback.

## Getting Started

### Prerequisites

- STM32F767 microcontroller
- SD card with WAV files
- TFT LCD for display
- Touch screen for input

### Building and Flashing

1. Compile the project using your preferred IDE (e.g., STM32CubeIDE).
2. Flash the compiled binary to the STM32F767 microcontroller.

### Usage

1. Power on the system.
2. Use the touch screen to navigate between the equalizer, piano, and WAV playback menus.
3. Adjust settings and play audio as desired.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- STM32F767 microcontroller
- Arm CMSIS-DSP library