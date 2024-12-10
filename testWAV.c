#include "stm32f767xx.h"
#include "OK-STM767.h"
#include "OK-STM767_SD_card.h"
#include "OK-STM767_VS1053b.h"

/*******************************************************************************
 * 상수 정의
 ******************************************************************************/
#define THEME_BG        Black       // 배경색
#define THEME_HEADER    0x861D      // 진한 회색
#define THEME_TEXT      0xBDF7      // 밝은 회색
#define THEME_HIGHLIGHT 0x05FF      // 하이라이트 색상
#define THEME_ACCENT    0xFD20      // 강조 색상
#define THEME_BUTTON    0x2DB7      // 버튼 색상
#define THEME_WARNING   0xFBE0      // 경고 색상

#define BAR_WIDTH       15 
#define BAR_GAP         4
#define START_Y         220
#define MAX_HEIGHT      140
#define INITIAL_VOLUME  200
#define BUFFER_SIZE     512
#define BUFFER_COUNT    2
#define WAV_HEADER_SIZE 44

/*******************************************************************************
 * 함수 선언
 ******************************************************************************/
void TFT_filename(void);                          // Display MP3 file name, number, size
void TFT_volume(void);                            // Display volume
void TFT_bass(void);                              // Display bass
void TFT_treble(void);                            // Display treble
void Check_valid_increment_file(void);            // Check if valid file for increment
void Check_valid_decrement_file(void);            // Check if valid file for decrement
void TFT_MP3_bitrate(U16 highbyte, U16 lowbyte);  // Display MP3 file bitrate
unsigned char Icon_input(void);                   // Input touch screen icon

void SetupMainScreen(void);                       // 메인 화면 구성
void Initialize_VS1053b(void);                    // VS1053B 초기화

/*******************************************************************************
 * WAV 관련 함수
 ******************************************************************************/
uint8_t ParseWAVHeader(uint32_t sector);
void ConfigureVS1053ForWAV(void);

/*******************************************************************************
 * 전역 변수
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

typedef struct {
    uint32_t sampleRate;
    uint16_t numChannels;
    uint16_t bitsPerSample;
    uint32_t dataSize;
} WAV_Header;

WAV_Header wavHeader;


/*******************************************************************************
 * 메인 함수
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
    Delay_ms(1000);

    volume = 175;                       // 볼륨
    bass = 10;                          // 베이스
    treble = 5;                         // 트레블

    VS1053b_SetVolume(volume);
    Delay_ms(1);
    VS1053b_SetBassTreble(bass, treble);

    total_file = fatGetDirEntry(FirstDirCluster); // Get total file number

    for (i = 0; i < total_file; i++) {            // Get start address of all files
        MP3_start_sector[i] = fatClustToSect(file_start_cluster[i]);
        MP3_start_backup[i] = MP3_start_sector[i];
    }

    file_number = 0;                              // Initialize file number

    // 메인 화면 구성
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
                if (func_mode == 0) {            // 파일 번호 증가
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
                } else if (func_mode == 1) {     // 볼륨 증가
                    volume++;
                    if (volume > 250) {
                        volume = 5;  // 최소값으로 돌아감
                    }
                    VS1053b_SetVolume(volume);
                    TFT_volume();
                } else if (func_mode == 2) {     // 베이스 증가
                    bass++;
                    if (bass > 15) {
                        bass = 0;  // 최소값으로 돌아감
                    }
                    VS1053b_SetBassTreble(bass, treble);
                    TFT_bass();
                } else {                         // 트레블 증가
                    treble++;
                    if (treble > 7) {
                        treble = -8;  // 최소값으로 돌아감
                    }
                    VS1053b_SetBassTreble(bass, treble);
                    TFT_treble();
                }
                break;

            case KEY4:
                // 기능 없음
                break;

            default:
                break;
        }
    }
}

/* 메인 화면 구성*/
void SetupMainScreen(void) {
    // 메인 화면 구성
    TFT_clear_screen();                           
    TFT_string(0, 0, Black, THEME_HEADER, "  OH-IN-PE-  ");
    TFT_string(0, 3, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0, 5, THEME_HIGHLIGHT, THEME_BG, ">>");
    TFT_string(0, 7, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0, 9, THEME_TEXT, THEME_BG, "      파일 번호 : 000/000 (   kbps)     ");
    TFT_string(0,11, THEME_TEXT, THEME_BG, "      파일 용량 : 0000KB  (     Hz)     ");
    TFT_string(0,13, THEME_TEXT, THEME_BG, "     Music play : 00:00(000%)   (      )");
    TFT_string(0,15, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0,17, THEME_TEXT, THEME_BG, "   음량(Volume) : 000%(000/250)         ");
    TFT_string(0,19, THEME_TEXT, THEME_BG, "   저음(Bass)   :  00 (00 ~ 15)         ");
    TFT_string(0,21, THEME_TEXT, THEME_BG, "   고음(Treble) :  00 (-8 ~ +7)         ");
    TFT_string(0,23, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0,25, THEME_TEXT, THEME_BG, "   KEY1      KEY2      KEY3      KEY4   ");
    TFT_string(0,27, THEME_ACCENT, THEME_BG, "  (PLAY)   (select)     (up)     (mode) ");
    
    // 버튼 테두리
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

// WAV 헤더 파싱
uint8_t ParseWAVHeader(uint32_t sector) {
    uint8_t header[WAV_HEADER_SIZE];
    SD_read_sector(sector, header);

    // Wave 포맷 체크 (RIFF, WAVE) - front 4bit is ChunkID, back 4bit is Format 
    if(header[0] != 'R' || header[1] != 'I' || header[2] != 'F' || header[3] != 'F' ||
       header[8] != 'W' || header[9] != 'A' || header[10] != 'V' || header[11] != 'E') {
        return 0;
    }

    // Wave 포맷 정보 추출
    wavHeader.sampleRate = *(uint32_t*)&header[24];
    wavHeader.numChannels = *(uint16_t*)&header[22];
    wavHeader.bitsPerSample = *(uint16_t*)&header[34];
    wavHeader.dataSize = *(uint32_t*)&header[40];

    return 1;
}

// VS1053B WAV 인코딩 모드 설정
void ConfigureVS1053ForWAV(void) {
    VS1053b_software_reset();
    Delay_ms(10);
    
    // Native SPI 모드 설정
    VS1053b_SCI_Write(0x00, 0x0820);
    Delay_ms(1);
    
    // WAV 인코딩 모드 설정
    VS1053b_SCI_Write(0x05, 0xAC45);  // 48kHz, stereo, 16-bit
    
    // 볼륨 및 음질 설정
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

// 파일의 유효성을 검사하여 파일을 증가시킴
void Check_valid_increment_file(void) {
    unsigned char file_OK_flag = 0;
    do {
        // WAV 추가 (0x00574156)
        if ((extension != 0x004D5033) &&    // MP3
            (extension != 0x00574156) &&    // WAV 추가
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