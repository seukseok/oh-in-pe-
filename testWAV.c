#include "stm32f767xx.h"
#include "OK-STM767.h"
#include "OK-STM767_SD_card.h"
#include "OK-STM767_VS1053b.h"
#include "arm_math.h"

/*******************************************************************************
 * ��� ����
 ******************************************************************************/
#define THEME_BG        Black       // ����
#define THEME_HEADER    0x861D      // ���� ȸ��
#define THEME_TEXT      0xBDF7      // ���� ȸ��
#define THEME_HIGHLIGHT 0x05FF      // ���̶���Ʈ ����
#define THEME_ACCENT    0xFD20      // ���� ����
#define THEME_BUTTON    0x2DB7      // ��ư ����
#define THEME_WARNING   0xFBE0      // ��� ����

#define BAR_WIDTH       15 
#define BAR_GAP         4
#define START_Y         220
#define MAX_HEIGHT      140
#define INITIAL_VOLUME  200
#define BUFFER_SIZE     512
#define BUFFER_COUNT    2
#define WAV_HEADER_SIZE 44

/*******************************************************************************
 * �Լ� ����
 ******************************************************************************/
void TFT_filename(void);                          // Display MP3 file name, number, size
void TFT_volume(void);                            // Display volume
void TFT_bass(void);                              // Display bass
void TFT_treble(void);                            // Display treble
void Check_valid_increment_file(void);            // Check if valid file for increment
void Check_valid_decrement_file(void);            // Check if valid file for decrement
void TFT_MP3_bitrate(U16 highbyte, U16 lowbyte);  // Display MP3 file bitrate
unsigned char Icon_input(void);                   // Input touch screen icon

void SetupMainScreen(void);                       // ���� ȭ�� ����
void Initialize_VS1053b(void);                    // VS1053B �ʱ�ȭ

// ���� ���� ����� �Լ���
void DelLongFilename(char *filename, char *delLong_filename, unsigned int max_length);
unsigned char TFT_getTouch(unsigned int *x, unsigned int *y);
unsigned char IsWAVFile(U32 sector);
void sorting_music_file(void);

// KEY 4 �Լ���
void Piano_TILES(void);
void White_key_Init(void);
void Black_key_Init(void);
void Draw_Keys(void);
void Key_Touch(U16 touch_x, U16 touch_y);
void Key_input_handler(void);

void FFT_REC(void);

/*******************************************************************************
 * WAV ���� �Լ�
 ******************************************************************************/
uint8_t ParseWAVHeader(uint32_t sector);
void ConfigureVS1053ForWAV(void);

/*******************************************************************************
 * ���� ����
 ******************************************************************************/
unsigned char total_file;                         // Total file number
unsigned char file_number = 0;                    // Current file number

volatile uint32_t SysTick_Count = 0;
volatile uint8_t currentBuffer = 0;
uint32_t MP3_start_sector[MAX_FILE];
uint32_t MP3_start_backup[MAX_FILE];
uint32_t MP3_end_sector;
uint8_t play_flag = 0;                            // Play or stop flag
uint8_t MP3buffer[BUFFER_COUNT][BUFFER_SIZE];
uint32_t file_start[MAX_FILE];
uint32_t file_size[MAX_FILE];
uint16_t volume = INITIAL_VOLUME;
uint8_t graph_piano_mode = 0;

typedef struct {
    uint32_t sampleRate;
    uint16_t numChannels;
    uint16_t bitsPerSample;
    uint32_t dataSize;
} WAV_Header;

WAV_Header wavHeader;

/*******************************************************************************
 * SysTick ���� �Լ�
 ******************************************************************************/
void SysTick_Handler(void) {
    SysTick_Count++;
}

void SysTick_Initialize(void) {
    SysTick->LOAD = 216000 - 1;    /* 216MHz/1000 = 216000 (1ms �ֱ�) */
    SysTick->VAL = 0;              /* ī���� �ʱ�ȭ */
    SysTick->CTRL = 0x07;          /* Ŭ�� �ҽ�, ���ͷ�Ʈ, Ÿ�̸� Ȱ��ȭ */
}

/*******************************************************************************
 * ���� �Լ�
 ******************************************************************************/
int main(void) {
    unsigned char i, key;
    unsigned char func_mode = 0;                  // Function key mode
    unsigned short index = 512;                   // Byte index in a sector
    unsigned short loop = 0;                      // MP3 play loop counter
    unsigned int playPercentage;                  // Play percentage

    unsigned short time, stereo;                  // Decode time and mono/stereo
    unsigned short HDAT1, HDAT0;                  // MP3 file frame header information data

    Initialize_MCU();                             // Initialize MCU and kit
    Delay_ms(50);                                 // Wait for system stabilization
    Initialize_TFT_LCD();                         // Initialize TFT-LCD module
    Initialize_touch_screen();                    // Initialize touch screen

    TFT_string(0, 4, Green, Black, "****************************************");
    TFT_string(0, 6, White, Black, "                OH-IN-PE-               ");
    TFT_string(0, 8, Green, Black, "****************************************");
    TFT_string(0, 23, Cyan, Black, "           SD init...            ");
    Beep();
    Delay_ms(1000);
    TFT_clear_screen();

    Initialize_SD();                              // Initialize SD card
    Initialize_FAT32();                           // Initialize FAT32 file system
    Initialize_VS1053b();                         // Initialize VS1053b
    SysTick_Initialize();                         // SysTick �ʱ�ȭ
    Delay_ms(1000);

    volume = 175;                       // ����
    bass = 10;                          // ���̽�
    treble = 5;                         // Ʈ����

    VS1053b_SetVolume(volume);
    Delay_ms(1);
    VS1053b_SetBassTreble(bass, treble);

    total_file = fatGetDirEntry(FirstDirCluster); // Get total file number

    for (i = 0; i < total_file; i++) {            // Get start address of all files
        MP3_start_sector[i] = fatClustToSect(file_start_cluster[i]);
        MP3_start_backup[i] = MP3_start_sector[i];
    }

    file_number = 0;                              // Initialize file number

    // ���� ȭ�� ����
    SetupMainScreen();

    while (1) {
        if (((GPIOC->IDR & 0x0080) == 0x0080) && (play_flag == 1)) {
            if (index == 512) {
                if (MP3_end_sector == MP3_start_sector[file_number]) {
                    if (file_number != (total_file - 1))
                        file_number++;
                    else
                        file_number = 0;

                    TFT_filename();
                    Check_valid_increment_file();

                    MP3_start_sector[file_number] = MP3_start_backup[file_number];
                    MP3_end_sector = (file_size[file_number] >> 9) + MP3_start_sector[file_number];
                    VS1053b_software_reset();  // VS1053b software reset to change music file
                }
                index = 0;
                SD_read_sector(MP3_start_sector[file_number]++, MP3buffer[currentBuffer]);
            }

            for (i = 0; i < 32; i++) {           // Send 32 data bytes
                GPIOC->BSRR = 0x00400000;        // -MP3_DCS = 0
                SPI3_write(MP3buffer[currentBuffer][index++]);  // Write a byte of MP3 data to VS1053b
                GPIOC->BSRR = 0x00000040;        // -MP3_DCS = 1
            }
        }

        loop++;                                  // Display MP3 file bitrate or play percentage
        if ((extension == 0x004D5033) && (loop == 250) && (play_flag == 1)) {
            HDAT1 = VS1053b_SCI_Read(0x09);
            HDAT0 = VS1053b_SCI_Read(0x08);

            if ((HDAT1 & 0xFFE0) == 0xFFE0)
                if (((HDAT1 & 0x0006) != 0x0000) && ((HDAT0 & 0x0C00) != 0x0C00))
                    TFT_MP3_bitrate(HDAT1, HDAT0);
        } else if ((loop == 500) && (play_flag == 1)) {
            loop = 0;

            time = VS1053b_SCI_Read(0x04);       // Decode time
            TFT_xy(18, 13);
            TFT_color(Magenta, Black);
            TFT_unsigned_decimal(time / 60, 1, 2);
            TFT_xy(21, 13);
            TFT_unsigned_decimal(time % 60, 1, 2);

            playPercentage = MP3_end_sector - MP3_start_sector[file_number];
            playPercentage = (unsigned int)((float)playPercentage / (float)(file_size[file_number] >> 9) * 100.);
            playPercentage = 100 - playPercentage;

            TFT_xy(24, 13);                      // Display play percentage
            TFT_color(Yellow, Black);
            if (playPercentage >= 100)
                TFT_unsigned_decimal(playPercentage, 0, 3);
            else if (playPercentage >= 10)
                TFT_unsigned_decimal(playPercentage, 0, 2);
            else
                TFT_unsigned_decimal(playPercentage, 0, 1);
            TFT_color(Cyan, Black);
            TFT_English('%');
            TFT_English(')');
            TFT_English(' ');
            TFT_English(' ');

            stereo = VS1053b_SCI_Read(0x05);
            TFT_xy(27, 11);                      // Sampling rate
            TFT_color(Yellow, Black);
            TFT_unsigned_decimal(stereo >> 1, 0, 5);
            if ((stereo & 0x0001) == 0x0001) {   // Channel mode = stereo
                TFT_string(32, 17, Cyan, Black, "(");
                TFT_string(33, 17, Yellow, Black, "stereo");
                TFT_string(39, 17, Cyan, Black, ")");
            } else {                             // Channel mode = mono
                TFT_string(32, 17, Cyan, Black, "(");
                TFT_string(33, 17, Yellow, Black, " mono ");
                TFT_string(39, 17, Cyan, Black, ")");
            }
        }

        key = Key_input();                       // Key input
        if (key == no_key)                       // If no key input, read touch screen icon
            key = Icon_input();

        switch (key) {
            case KEY1:
                play_flag ^= 0x01;               // Toggle play or stop
                if(play_flag == 1)
                    TFT_string(33, 13, THEME_HIGHLIGHT, THEME_BG, " play ");
                else
                    TFT_string(33, 13, THEME_TEXT, THEME_BG, " stop ");
                break;

            case KEY2:
                if (func_mode == 0) {            // Select function
                    func_mode = 1;
                    TFT_string(0, 5, Magenta, Black, "  ");
                    TFT_string(0,17, Magenta, Black, ">>");
                } else if (func_mode == 1) {
                    func_mode = 2;
                    TFT_string(0,17, Magenta, Black, "  ");
                    TFT_string(0,19, Magenta, Black, ">>");
                } else if (func_mode == 2) {
                    func_mode = 3;
                    TFT_string(0,19, Magenta, Black, "  ");
                    TFT_string(0,21, Magenta, Black, ">>");
                } else {
                    func_mode = 0;
                    TFT_string(0,21, Magenta, Black, "  ");
                    TFT_string(0, 5, Magenta, Black, ">>");
                }
                break;

            case KEY3:
                if (func_mode == 0) {            // ���� ��ȣ ����
                    if (file_number != (total_file - 1))
                        file_number++;
                    else
                        file_number = 0;

                    VS1053b_software_reset();
                    TFT_filename();
                    Check_valid_increment_file();

                    MP3_start_sector[file_number] = MP3_start_backup[file_number];
                    MP3_end_sector = (file_size[file_number] >> 9) + MP3_start_sector[file_number];
                    index = 512;
                    VS1053b_software_reset();
                } else if (func_mode == 1) {     // ���� ����
                    volume++;
                    if (volume > 250) {
                        volume = 5;  // �ּҰ����� ���ư�
                    }
                    VS1053b_SetVolume(volume);
                    TFT_volume();
                } else if (func_mode == 2) {     // ���̽� ����
                    bass++;
                    if (bass > 15) {
                        bass = 0;  // �ּҰ����� ���ư�
                    }
                    VS1053b_SetBassTreble(bass, treble);
                    TFT_bass();
                } else {                         // Ʈ���� ����
                    treble++;
                    if (treble > 7) {
                        treble = -8;  // �ּҰ����� ���ư�
                    }
                    VS1053b_SetBassTreble(bass, treble);
                    TFT_treble();
                }
                break;

            case KEY4:
                // ��� ����
                graph_piano_mode = 1;
                Piano_TILES();
                break;

            default:
                break;
        }
    }
}

/* ���� ȭ�� ����*/
void SetupMainScreen(void) {
    // ���� ȭ�� ����
    TFT_clear_screen();                           
    TFT_string(0, 0, Black, THEME_HEADER, "  OH-IN-PE-  ");
    TFT_string(0, 3, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0, 5, THEME_HIGHLIGHT, THEME_BG, ">>");
    TFT_string(0, 7, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0, 9, THEME_TEXT, THEME_BG, "      ���� ��ȣ : 000/000 (   kbps)     ");
    TFT_string(0,11, THEME_TEXT, THEME_BG, "      ���� �뷮 : 0000KB  (     Hz)     ");
    TFT_string(0,13, THEME_TEXT, THEME_BG, "     Music play : 00:00(000%)   (      )");
    TFT_string(0,15, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0,17, THEME_TEXT, THEME_BG, "   ����(Volume) : 000%(000/250)         ");
    TFT_string(0,19, THEME_TEXT, THEME_BG, "   ����(Bass)   :  00 (00 ~ 15)         ");
    TFT_string(0,21, THEME_TEXT, THEME_BG, "   ����(Treble) :  00 (-8 ~ +7)         ");
    TFT_string(0,23, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0,25, THEME_TEXT, THEME_BG, "   KEY1      KEY2      KEY3      KEY4   ");
    TFT_string(0,27, THEME_ACCENT, THEME_BG, "  (PLAY)   (select)     (up)     (mode) ");
    
    // ��ư �׵θ�
    Rectangle(12, 196, 67, 235, THEME_BUTTON);    
    Rectangle(92, 196, 147, 235, THEME_BUTTON);
    Rectangle(176, 196, 231, 235, THEME_BUTTON);
    Rectangle(256, 196, 311, 235, THEME_BUTTON);

    TFT_volume();                                 // Display initial play value
    TFT_bass();
    TFT_treble();

    TFT_xy(22, 9);                                // Display total file number
    TFT_color(Yellow, Black);
    TFT_unsigned_decimal(total_file, 1, 3);
    TFT_filename();
    Check_valid_increment_file();

    MP3_end_sector = (file_size[file_number] >> 9) + MP3_start_sector[file_number];
}

// WAV ��� �Ľ�
uint8_t ParseWAVHeader(uint32_t sector) {
    uint8_t header[WAV_HEADER_SIZE];
    SD_read_sector(sector, header);

    // Wave ���� üũ (RIFF, WAVE) - front 4bit is ChunkID, back 4bit is Format 
    if(header[0] != 'R' || header[1] != 'I' || header[2] != 'F' || header[3] != 'F' ||
       header[8] != 'W' || header[9] != 'A' || header[10] != 'V' || header[11] != 'E') {
        return 0;
    }

    // Wave ���� ���� ����
    wavHeader.sampleRate = *(uint32_t*)&header[24];
    wavHeader.numChannels = *(uint16_t*)&header[22];
    wavHeader.bitsPerSample = *(uint16_t*)&header[34];
    wavHeader.dataSize = *(uint32_t*)&header[40];

    return 1;
}

// VS1053B WAV ���ڵ� ��� ����
void ConfigureVS1053ForWAV(void) {
    VS1053b_software_reset();
    Delay_ms(10);
    
    // Native SPI ��� ����
    VS1053b_SCI_Write(0x00, 0x0820);
    Delay_ms(1);
    
    // WAV ���ڵ� ��� ����
    VS1053b_SCI_Write(0x05, 0xAC45);  // 48kHz, stereo, 16-bit
    
    // ���� �� ���� ����
    VS1053b_SetVolume(volume);
    VS1053b_SetBassTreble(8, 3);
}

/* User-defined functions */

void TFT_filename(void) {
  unsigned char file_flag;
  unsigned short file_KB;

  TFT_string(0, 7, Cyan, Black, "----------------------------------------");
  TFT_string(3, 5, Green, Black, "                                     ");

  file_flag = Get_long_filename(file_number);   // Check file name

  if (file_flag == 0)                           // Short file name (8.3 format)
    TFT_short_filename(3, 5, White, Black);
  else if (file_flag == 1)                      // Long file name
    TFT_long_filename(3, 5, White, Black);
  else if (file_flag == 2)                      // File name is longer than 195 characters
    TFT_string(3, 5, Red, Black, "* ????? ???? ??? *");
  else                                          // File name error
    TFT_string(3, 5, Red, Black, "*** ????? ???? ***");

  file_KB = file_size[file_number] / 1024;      // Calculate file size in KB
  if ((file_size[file_number] % 1024) != 0)
    file_KB++;

  if (file_flag != 3) {
    TFT_xy(18, 9);                            // File number
      TFT_color(Magenta, Black);
    TFT_unsigned_decimal(file_number + 1, 1, 3);
    TFT_xy(17,11);                            // File size
      TFT_color(Magenta, Black);
    TFT_unsigned_decimal(file_KB, 0, 5);
    }

  TFT_string(27, 9, Yellow, Black, "000");      // Clear bitrate

  TFT_xy(24, 13);                               // Display percentage = 0
  TFT_color(Yellow, Black);
  TFT_English('0');
  TFT_color(Cyan, Black);
  TFT_English('%');
  TFT_English(')');
  TFT_English(' ');
  TFT_English(' ');
}

void TFT_volume(void) {
  TFT_xy(18, 17);
  TFT_color(Magenta, Black);
  TFT_unsigned_decimal((unsigned int)(volume * 100.0 / 250.0 + 0.5), 0, 3);
  TFT_xy(23, 17);
  TFT_color(Yellow, Black);
  TFT_unsigned_decimal(volume, 1, 3);
}

void TFT_bass(void) {
  TFT_xy(19, 19);
  TFT_color(Magenta, Black);
  TFT_unsigned_decimal(bass, 1, 2);
}

void TFT_treble(void) {
  TFT_xy(19, 21);
  TFT_color(Magenta, Black);
  TFT_signed_decimal(treble, 0, 1);
}

// ������ ��ȿ���� �˻��Ͽ� ������ ������Ŵ
void Check_valid_increment_file(void) {
    unsigned char file_OK_flag = 0;
    do {
        // WAV �߰� (0x00574156)
        if ((extension != 0x004D5033) &&    // MP3
            (extension != 0x00574156) &&    // WAV �߰�
            (extension != 0x00414143) &&    // AAC
            (extension != 0x00574D41) &&    // WMA
            (extension != 0x004D4944)) {    // MIDI
            if (file_number != (total_file - 1))
                file_number++;
            else
                file_number = 0;
            TFT_filename();
        } else {
            file_OK_flag = 1;
        }
    } while (file_OK_flag == 0);
}

void Check_valid_decrement_file(void) {
  unsigned char file_OK_flag = 0;
  do {
    if ((extension != 0x004D5033) && (extension != 0x00414143) &&
      (extension != 0x00574D41) && (extension != 0x004D4944)) {
      if (file_number != 0)
	    file_number--;
	  else
	    file_number = total_file - 1;
          TFT_filename();
    } else {
        file_OK_flag = 1;
    }
  } while (file_OK_flag == 0);
}

void TFT_MP3_bitrate(U16 highbyte, U16 lowbyte) {
  unsigned short MPEG10_Layer1[16] = {
    0, 32, 64, 96, 128, 160, 192, 224,
    256, 288, 320, 352, 384, 416, 448, 0
  };
  unsigned short MPEG10_Layer2[16] = {
    0, 32, 48, 56, 64, 80, 96, 112,
    128, 160, 192, 224, 256, 320, 384, 0
  };
  unsigned short MPEG10_Layer3[16] = {
    0, 32, 40, 48, 56, 64, 80, 96,
    112, 128, 160, 192, 224, 256, 320, 0
  };
  unsigned short MPEG20_Layer1[16] = {
    0, 32, 48, 56, 64, 80, 96, 112,
    128, 144, 160, 176, 192, 224, 256, 0
  };
  unsigned short MPEG20_Layer2[16] = {
    0, 8, 16, 24, 32, 40, 48, 56,
    64, 80, 96, 112, 128, 144, 160, 0
  };

  if ((highbyte & 0x0018) == 0x0018) {              // MPEG-1.0
    switch (highbyte & 0x0006) {
      case 0x0002:                              // Layer-3
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG10_Layer3[lowbyte >> 12], 1, 3);
		        break;
      case 0x0004:                              // Layer-2
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG10_Layer2[lowbyte >> 12], 1, 3);
		        break;
      case 0x0006:                              // Layer-1
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG10_Layer1[lowbyte >> 12], 1, 3);
		        break;
	}
  } else if ((highbyte & 0x0018) == 0x0010) {       // MPEG-2.0
    switch (highbyte & 0x0006) {
      case 0x0002:                              // Layer-3
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG20_Layer2[lowbyte >> 12], 1, 3);
		        break;
      case 0x0004:                              // Layer-2
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG20_Layer2[lowbyte >> 12], 1, 3);
		        break;
      case 0x0006:                              // Layer-1
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG20_Layer1[lowbyte >> 12], 1, 3);
		        break;
	}
  } else {                                          // MPEG-2.5
    switch (highbyte & 0x0006) {
      case 0x0002:                              // Layer-3
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG20_Layer2[lowbyte >> 12], 1, 3);
		        break;
      case 0x0004:                              // Layer-2
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG20_Layer2[lowbyte >> 12], 1, 3);
		        break;
      case 0x0006:                              // Layer-1
        TFT_xy(27, 9);
        TFT_color(Yellow, Black);
        TFT_unsigned_decimal(MPEG20_Layer1[lowbyte >> 12], 1, 3);
		        break;
	}
    }
}

unsigned char icon_flag = 0;

unsigned char Icon_input(void) {
  unsigned char keyPressed;

  Touch_screen_input();                          // Input touch screen

  if ((icon_flag == 0) && (x_touch >= 12) && (x_touch <= 67) &&
    (y_touch >= 196) && (y_touch <= 235)) {
    keyPressed = KEY1;
      icon_flag = 1;
    Rectangle(12, 196, 67, 235, Magenta);

  } else if ((icon_flag == 0) && (x_touch >= 92) && (x_touch <= 147) &&
         (y_touch >= 196) && (y_touch <= 235)) {
    keyPressed = KEY2;
      icon_flag = 1;
    Rectangle(92, 196, 147, 235, Magenta);

  } else if ((icon_flag == 0) && (x_touch >= 176) && (x_touch <= 231) &&
         (y_touch >= 196) && (y_touch <= 235)) {
    keyPressed = KEY3;
      icon_flag = 1;
    Rectangle(176, 196, 231, 235, Magenta);

  } else if ((icon_flag == 0) && (x_touch >= 256) && (x_touch <= 311) &&
         (y_touch >= 196) && (y_touch <= 235)) {
    keyPressed = KEY4;
      icon_flag = 1;
    Rectangle(256, 196, 311, 235, Magenta);

  } else if ((icon_flag == 1) && (x_touch == 0) && (y_touch == 0)) {
    keyPressed = no_key;
      icon_flag = 0;
    Rectangle(12, 196, 67, 235, Yellow);
    Rectangle(92, 196, 147, 235, Yellow);
    Rectangle(176, 196, 231, 235, Yellow);
    Rectangle(256, 196, 311, 235, Yellow);
      Delay_ms(50);
  } else {
    keyPressed = no_key;
  }

  return keyPressed;
}
/*
// ===============================================================
// Music File Sort & Make Select Box
// ===============================================================

#define FILES_PER_PAGE 8  // �� �������� ǥ���� ���� ��

char short_filename_buffer[13];  // 8.3 ���� ���� �̸� ����
char long_filename_buffer[256]; // �� ���� �̸� ����
unsigned int touch_flag = 0;     // ���� ���� �÷���
unsigned int selected_index = 0; // ���õ� ���� �ε���


// �� ���� �̸� �߶󳻱� �Լ�
void DelLongFilename(char *filename, char *delLong_filename, unsigned int max_length) {
    if (max_length < 2) return;  // �ʹ� ���� ���̴� ó������ ����
    if (strlen(filename) > max_length) {
        strncpy(delLong_filename, filename, max_length - 1);
        delLong_filename[max_length - 1] = '~';
        delLong_filename[max_length] = '\0';
    } else {
        strcpy(delLong_filename, filename);
    }
}

// ������ ��ġ ���� �Լ�
unsigned char TFT_getTouch(unsigned int *x, unsigned int *y) {
    if (TouchScreen_IsTouched()) {
        *x = TouchScreen_GetX();
        *y = TouchScreen_GetY();
        return 1;
    }
    return 0;
}

// ���� ���� �Լ�
unsigned char IsWAVFile(U32 sector) {
    return ParseWAVHeader(sector);
}

// ���� �ҷ����� �� ���� �Լ�
void sorting_music_file(void) {
    unsigned int current_page = 0;
    unsigned int stopline = 1;
    unsigned int start_file, end_file;
    unsigned char file_flag;
    unsigned int line_count;
    unsigned int touch_flag = 0;                // ���� ���� �÷���
    unsigned int selected_index = -1;           // ���õ� ���� �ε���
  
    TFT_clear_screen();
    TFT_string(0, 0, THEME_HEADER, Black, "���� ��� Ž��");

    while (stopline) {
        Rectangle(0, 20, 320, 200, White);
        color_screen(Black, 0, 20, 319, 199);

        start_file = current_page * FILES_PER_PAGE;
        end_file = start_file + FILES_PER_PAGE;
        if (end_file > total_file) end_file = total_file;

        line_count = 24;
        for (unsigned int i = start_file; i < end_file; i++) {

           // WAV ���� ���͸�
            if (!IsWAVFile(MP3_start_sector[i])) {
                continue;  // WAV ������ �ƴϸ� �ǳʶٱ�
            }

            file_flag = Get_long_filename(i);
            uint8_t cutLong_name[41]; // ȭ�鿡 ǥ���� �̸� (�ִ� 40��)

            if (file_flag == 0) {
              DelLongFilename(short_filename_buffer, cutLong_name, 40); // �̸� ���� �߶󳻱�(�ִ� 40��) - �Ƹ� �Ⱦ��ϰ�
              TFT_string(5, line_count, (i == selected_index && touch_flag) ? THEME_HIGHLIGHT : White, Black, cutLong_name);
            } else if (file_flag == 1) {
              DelLongFilename(long_filename_buffer, cutLong_name, 40); // �̸� ���� �߶󳻱�(�ִ� 40��)
              TFT_string(5, line_count, (i == selected_index && touch_flag) ? THEME_HIGHLIGHT : White, Black, cutLong_name);
            } else if (file_flag == 2) {
              TFT_string(5, line_count, Red, Black, "* �̸��� �ʹ� ��ϴ� *");
            } else {
              TFT_string(5, line_count, Red, Black, "*** ���� �̸� ���� ***");
            }

            line_count += 2;
            if(line_count >= 199) break; // ȭ�� ���� �����ϸ� ���� & ���� ó��
        }

        if (current_page > 0) {
          TFT_string(10, 210, White, Black, "[���� ������]");
        }
        if (current_page < (total_file + FILES_PER_PAGE - 1) / FILES_PER_PAGE - 1) {
          TFT_string(200, 210, White, Black, "[���� ������]");
        }

        while (1) {
            if (TFT_getTouch(&x_touch, &y_touch)) {
                if (y_touch >= 24 && y_touch <= 199) {
                    unsigned int temp_index = (y_touch - 24) / 16 + start_file;
                    if (temp_index < total_file) {
                      if (!IsWAVFile(MP3_start_sector[temp_index])) {
                        continue; // WAV ������ �ƴ� ��� ����
                      }

                      if (touch_flag == 1 && temp_index == selected_index) {
                        file_number = selected_index;
                        stopline = 0;
                        break;
                      } else {
                        touch_flag = 1;
                        selected_index = temp_index;
                        break;
                      }
                    }
                } else {
                    touch_flag = 0; // �߸��� ��ġ ó��
                }
                if ((x_touch >= 10 && x_touch <= 100) && (y_touch >= 210 && y_touch <= 230) && current_page > 0) {
                  current_page--;
                  break;
                }
                if ((x_touch >= 200 && x_touch <= 300) && (y_touch >= 210 && y_touch <= 230) &&
                  current_page < (total_file + FILES_PER_PAGE - 1) / FILES_PER_PAGE - 1) {
                  current_page++;
                  break;
                }
            }
        }
    }
    clear_screen();
    SetupMainScreen();
}

// ===============================================================
// Piano Tiles(C to B)
// ===============================================================

#define WHITE_TILES 7
#define BLACK_TILES 5

#define WHITE_KEY_WIDTH 40
#define WHITE_KEY_HEIGHT 81
#define BLACK_KEY_WIDTH 54
#define BLACK_KEY_HEIGHT 25

#define GRAY 0x7BEF

typedef struct{
  U16 xstart;
  U16 xend;
  U16 ystart;
  U16 yend;
} KeyInfo;

KeyInfo White_key[WHITE_TILES];
KeyInfo Black_key[BLACK_TILES];

unsigned char IsWhiteKeyTouching[WHITE_TILES] = {0};
unsigned char IsBlackKeyTouching[BLACK_TILES] = {0};

void Piano_TILES(void){
  White_key_Init();
  Black_key_Init();
  Delay_ms(10);
  FFT_REC();
  Draw_Keys();
}

void White_key_Init(void){
  uint8_t x = 36;
  uint8_t y = 127;

  for(int i = 0; i < WHITE_TILES; i++){
    White_key[i].xstart = x;
    White_key[i].xend = x + WHITE_KEY_WIDTH;
    White_key[i].ystart = y;
    White_key[i].yend = y + WHITE_KEY_HEIGHT;
    x += WHITE_KEY_WIDTH + 2; 
  }
}
void Black_key_Init(void){
  uint8_t x = 36 + WHITE_KEY_WIDTH - (BLACK_KEY_WIDTH / 2);
  uint8_t y = 127;

  //��� ��ġ
  int Black_key_pos[] = {0,1,3,4,5};

  for(int i = 0; i < BLACK_TILES; i++){
    Black_key[i].xstart = x + (Black_key_pos[i] * (WHITE_KEY_WIDTH + 2));
    Black_key[i].xend = x + Black_key[i].xstart + BLACK_KEY_WIDTH;
    Black_key[i].ystart = y;
    Black_key[i].yend = y + BLACK_KEY_HEIGHT;
    x += WHITE_KEY_WIDTH + 2; 
  }
}

void FFT_REC(void){
    // �簢�� �׵θ�
    Rectangle(36, 15, 328, 96, THEME_HEADER);
    // �Լ� ���� ��
}

void Draw_Keys(void) {
    // �� �ǹ� �׸���
    for (int i = 0; i < 7; i++) {
        block(White_key[i].xstart, White_key[i].ystart,
                     WHITE_KEY_WIDTH, WHITE_KEY_HEIGHT, Black, White);
    }
    // ���� �ǹ� �׸���
    for (int i = 0; i < 5; i++) {
        block(Black_key[i].xstart, Black_key[i].ystart,
                     BLACK_KEY_WIDTH, BLACK_KEY_HEIGHT, White);
    }
    Rectangle(36, 127, 328, 208, THEME_HEADER); // �׵θ� �簢��    
}

void Key_Touch(U16 touch_x, U16 touch_y){
  for(int i = 0; i < WHITE_TILES; i++){
    if(x_touch >= White_key[i].xstart && x_touch <= White_key[i].xend && y_touch >= White_key[i].ystart && y_touch <= White_key[i].yend){
      if(!IsWhiteKeyTouching[i]){
        IsWhiteKeyTouching[i] = 1;
        block(White_key[i].xstart, White_key[i].ystart,
                     WHITE_KEY_WIDTH, WHITE_KEY_HEIGHT, Black, GRAY);
        // ��� ����(Toggle) ex) playWhite(i);
      }
    } else {
      IsWhiteKeyTouching[i] = 0;
      // ��� ���� ex) stopWhite(i);
    }
  }

  for(int i = 0; i < BLACK_TILES; i++){
    if(x_touch >= Black_key[i].xstart && x_touch <= Black_key[i].xend && y_touch >= Black_key[i].ystart && y_touch <= Black_key[i].yend){
      if(!IsBlackKeyTouching[i]){
        IsBlackKeyTouching[i] = 1;
        block(Black_key[i].xstart, Black_key[i].ystart,
                     BLACK_KEY_WIDTH, BLACK_KEY_HEIGHT, Black, GRAY);
        // ��� ����(Toggle) ex) playBlack(i)'
      }
    } else {
      IsBlackKeyTouching[i] = 0;
      // ��� ���� ex) stopBlack(i);
    }
  }  
}

void Key_input_handler(void){ // SysTick �����ؼ� �߰�����
  uint16_t touch_x, touch_y;
  if(graph_piano_mode == 1){
    if(TFT_getTouch(&x_touch,&y_touch)){
      Key_Touch(touch_x, touch_y);
    } else {
      for(int i = 0; i < WHITE_TILES; i++){
        IsWhiteKeyTouching[i] = 0;
        // stop playing ���� �߰� ex) stopWhite(i);
      }
      for(int i = 0; i < BLACK_TILES; i++){
        IsBlackKeyTouching[i] = 0;
        // stop playing ���� �߰� ex) stopBlack(i);
      }
    }
  }
}
*/ 