# Oh-In-Pe- Project

## Overview

The Oh-In-Pe- project is a sound modulation project using the STM32F767 microcontroller. It includes features such as an equalizer, piano, WAV file playback, and FFT spectrum analysis.

## Features

- **Equalizer**: Adjust volume, bass, and treble.
- **Piano**: Simulate piano keys and play notes.
- **WAV Playback**: Play audio files from an SD card.
- **FFT Spectrum Analysis**: Visualize the frequency spectrum of the audio signal.

## File Structure

### main.c

The `main.c` file contains the core logic of the project. Here are the main components:

#### Constants

- **Theme Colors**: Defined colors for UI elements.
- **Key Dimensions**: Dimensions for piano keys.
- **Notes Frequencies**: Frequencies for musical notes.
- **Slider and Bar Settings**: Configuration for UI sliders and bars.
- **Buffer Settings**: Sizes for audio buffers and headers.

#### Data Structures

- **`KeyInfo`**: Stores coordinates for piano keys.
- **`Slider`**: Stores slider values and positions.
- **`WAV_Header`**: Stores metadata for WAV files.

#### Functions

##### Initialization

- **`System_Init`**: Initializes the system and peripherals.
- **`TIM1_Init`**: Initializes Timer 1 for audio playback.

##### UI Functions

- **`MainScreen`**: Displays the main screen.
- **`Menu_Equalizer`**: Displays the equalizer menu.
- **`Menu_Piano_WAV`**: Displays the piano and WAV menu.
- **`Draw_MainScreen`**: Draws the main screen UI.
- **`Draw_Equalizer_UI`**: Draws the equalizer UI.
- **`Update_Equalizer_UI`**: Updates the equalizer UI based on user input.
- **`Draw_Piano_WAV_UI`**: Draws the piano and WAV UI.
- **`Draw_Keys`**: Draws the piano keys.

##### Input Handling

- **`Get_Only_Key_Input`**: Handles key input.
- **`Get_Touch_Input`**: Handles touch input.
- **`Key_Touch_Handler`**: Handles touch input for piano keys.
- **`Piano_Input_Handler`**: Handles piano input.

##### Audio Functions

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

#### Global Variables

- **Menu Selection**: Tracks which menu is currently selected.
- **Graph Piano Mode**: Tracks if the piano mode is active.
- **Keys and Touch States**: Tracks touch states for piano keys.
- **Slider Configurations**: Stores slider values and positions.
- **WAV File Data**: Stores data related to WAV files and playback.

### FFT_WAV.c

The `FFT_WAV.c` file contains the FFT spectrum analysis logic. Here are the main components:

#### Constants

- **Sample Size**: Number of samples for FFT.
- **FFT Size**: Size of the FFT buffer.

#### Data Structures

- **`complex_f`**: Stores complex numbers for FFT calculations.

#### Functions

##### Initialization

- **`Initialize_MCU`**: Initializes the microcontroller.
- **`Initialize_LCD`**: Initializes the LCD.
- **`Initialize_TFT_LCD`**: Initializes the TFT LCD.

##### FFT Functions

- **`TIM7_IRQHandler`**: Interrupt handler for ADC input.
- **`Display_FFT_screen`**: Displays the FFT screen.
- **`do_fft`**: Performs FFT on the input signal.
- **`Draw_FFT`**: Draws the FFT result on the screen.
- **`apply_hamming_window`**: Applies a Hamming window to the input signal.
- **`low_pass_filter`**: Applies a low-pass filter to the FFT data.

#### Global Variables

- **FFT Mode and Flag**: Tracks the FFT mode and status.
- **FFT Count**: Tracks the number of samples collected.
- **ADC Buffer**: Stores ADC input samples.
- **FFT Buffers**: Stores FFT input, output, and intermediate results.

## Getting Started

### Prerequisites

- STM32F767 microcontroller
- SD card with WAV files
- TFT LCD for display
- Touch screen for input

### Building and Flashing

1. Compile the project using your preferred IDE (e.g., IAR).
2. Flash the compiled binary to the STM32F767 microcontroller.

### Usage

1. Power on the system.
2. Use the touch screen to navigate between the equalizer, piano, and WAV playback menus.
3. Adjust equalizer settings and play audio.
4. Observe the FFT spectrum on the FFT screen to see the changes in the frequency spectrum as you adjust the equalizer.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- STM32F767 microcontroller
- Arm CMSIS-DSP library


### images
![alt text](image.png)
![alt text](image-1.png)
![alt text](image-2.png)
![alt text](image-3.png)
![alt text](image-4.png)
![alt text](image-5.png)