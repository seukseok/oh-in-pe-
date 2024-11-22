#include "stm32f767xx.h"
#include "OK-STM767.h"
#include "OK-STM767_SD_card.h"
#include "OK-STM767_VS1053b.h"
#include "arm_math.h"

// FFT 관련 상수 수정
#define FFT_SIZE 256  // FFT 크기 축소하여 처리 부하 감소
#define SAMPLE_FREQ 44100
#define FFT_DISPLAY_HEIGHT 80
#define FFT_DISPLAY_WIDTH 320
#define FFT_DISPLAY_Y_START 80  // UI와 겹치지 않는 위치

// 이중 버퍼링을 위한 구조체
typedef struct {
    float32_t buffer[512];
    volatile uint16_t writeIdx;
    volatile uint16_t readIdx;
    volatile uint8_t isFull;
} AudioBuffer;

AudioBuffer audioBuffer = {0};

arm_rfft_fast_instance_f32 S;
float32_t FFT_Input[FFT_SIZE];  
float32_t FFT_Output[FFT_SIZE];
float32_t FFT_Mag[FFT_SIZE/2];

// FFT 관련 전역 변수 추가
#define FFT_UPDATE_PERIOD 50  // FFT 업데이트 주기 (ms)

// FFT 화면 모드 플래그 추가
volatile uint8_t func_mode = 0;        // 기능 선택 모드
volatile uint8_t fft_display_mode = 0;  // 0: 일반 화면, 1: FFT 화면
volatile uint8_t fft_update_flag = 0;   // FFT 업데이트 플래그

void TFT_filename(void);                          // Display MP3 file name, number, size
void TFT_volume(void);                            // Display volume
void TFT_bass(void);                              // Display bass
void TFT_treble(void);                            // Display treble
void Check_valid_increment_file(void);            // Check if valid file for increment
void Check_valid_decrement_file(void);            // Check if valid file for decrement
void TFT_MP3_bitrate(U16 highbyte, U16 lowbyte);  // Display MP3 file bitrate

void FFT_Init(void);                              // FFT 초기화
void Process_FFT(void);                           // FFT 처리
void Draw_Spectrum(float32_t* data, uint16_t size, uint16_t color);// FFT 결과 그래픽 출력
void PushAudioData(uint8_t* data, uint16_t len);  // 오디오 데이터 버퍼에 저장
unsigned char Icon_input(void);                   // Input touch screen icon

void Display_Normal_Screen(void);                 // 일반 화면 표시 함수
void Init_FFT_Screen(void);                       // FFT 화면 초기화

unsigned char total_file;                         // Total file number
unsigned char file_number = 0;                    // Current file number

// 시스템 타이머 틱 카운트 변수
static volatile uint32_t SystemTicks = 0;

// SysTick 인터럽트 핸들러
void SysTick_Handler(void) {
    static uint32_t fft_tick_count = 0;
    SystemTicks++;
    
    // FFT 업데이트 타이밍 제어
    fft_tick_count++;
    if(fft_tick_count >= FFT_UPDATE_PERIOD) {
        fft_update_flag = 1;
        fft_tick_count = 0;
    }
}

// 시스템 타이머 초기화 함수
void SystemTick_Init(void) {
    // SysTick 설정 (1ms 간격)
    SysTick->LOAD = (SystemCoreClock / 1000) - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
}

// GetTick 함수 구현
uint32_t GetTick(void) {
    return SystemTicks;
}

int main(void) {
  unsigned char i, key;
  unsigned char func_mode = 0;                  // Function key mode
  unsigned char play_flag = 0;                  // Play or stop flag
  unsigned short index = 512;                   // Byte index in a sector
  unsigned short loop = 0;                      // MP3 play loop counter
  unsigned int playPercentage;                  // Play percentage

  unsigned int MP3_start_sector[MAX_FILE];      // MP3 file start sector
  unsigned int MP3_start_backup[MAX_FILE];
  unsigned char MP3buffer[512];                 // MP3 data from SD card
  unsigned int MP3_end_sector;                  // MP3 file end sector

  unsigned short time, stereo;                  // Decode time and mono/stereo
  unsigned short HDAT1, HDAT0;                  // MP3 file frame header information data

  Initialize_MCU();                             // Initialize MCU and kit
  Delay_ms(50);                                 // Wait for system stabilization
  Initialize_LCD();                             // Initialize text LCD module
  Initialize_TFT_LCD();                         // Initialize TFT-LCD module
  Initialize_touch_screen();                    // Initialize touch screen

  LCD_string(0x80, " OK-STM746 V1.0 ");         // Display title
  LCD_string(0xC0, "   Exp20_2.c    ");

  TFT_string(0, 4, Green, Black, "****************************************");
  TFT_string(0, 6, White, Black, "                OH-IN-PE-               ");
  TFT_string(0, 8, Green, Black, "****************************************");
  TFT_string(0, 23, Cyan, Black, "           SD 카드 초기화...            ");
  Beep();
  Delay_ms(1000);
  TFT_clear_screen();

  Initialize_SD();                              // Initialize SD card
  Initialize_FAT32();                           // Initialize FAT32 file system
  Initialize_VS1053b();                         // Initialize VS1053b
  Delay_ms(1000);

  volume = 175;                                 // Initial volume = 175/250(70%)
  VS1053b_SetVolume(volume);
  Delay_ms(1);
  bass = 10;                                    // Initial bass = 10 and treble = 5
  treble = 5;
  VS1053b_SetBassTreble(bass, treble);

  TFT_clear_screen();                           // Display basic screen
  TFT_string(0, 0, White, Magenta, "  OH-IN-PE-  ");
  TFT_string(0, 3, Cyan, Black, "----------------------------------------");
  TFT_string(0, 5, Magenta, Black, ">>");
  TFT_string(0, 7, Cyan, Black, "----------------------------------------");
  TFT_string(0, 9, Cyan, Black, "      파일 번호 : 000/000 (   kbps)     ");
  TFT_string(0,11, Cyan, Black, "      파일 용량 : 0000KB  (     Hz)     ");
  TFT_string(0,13, Cyan, Black, "      연주 진행 : 00:00(000%)   (      )");
  TFT_string(0,15, Cyan, Black, "----------------------------------------");
  TFT_string(0,17, Green, Black, "   음량(Volume) : 000%(000/250)         ");
  TFT_string(0,19, Green, Black, "   저음(Bass)   :  00 (00 ~ 15)         ");
  TFT_string(0,21, Green, Black, "   고음(Treble) :  00 (-8 ~ +7)         ");
  TFT_string(0,23, Cyan, Black, "----------------------------------------");
  TFT_string(0,25, Cyan, Black, "   KEY1      KEY2      KEY3      KEY4   ");
  TFT_string(0,27, Magenta, Black, "  (PLAY)    (FUNC)     (INC)     (DEC)  ");
  TFT_string(27,11, Yellow, Black, "00000");
  TFT_string(18,13, Magenta, Black, "00");
  TFT_string(21,13, Magenta, Black, "00");
  TFT_string(33,13, Magenta, Black, " 정지 ");

  Rectangle(12, 196, 67, 235, Yellow);          // Display touch key outline
  Rectangle(92, 196, 147, 235, Yellow);
  Rectangle(176, 196, 231, 235, Yellow);
  Rectangle(256, 196, 311, 235, Yellow);

  TFT_volume();                                 // Display initial play value
  TFT_bass();
  TFT_treble();

  total_file = fatGetDirEntry(FirstDirCluster); // Get total file number

  for (i = 0; i < total_file; i++) {            // Get start address of all files
    MP3_start_sector[i] = fatClustToSect(file_start_cluster[i]);
      MP3_start_backup[i] = MP3_start_sector[i];
    }

  TFT_xy(22, 9);                                // Display total file number
  TFT_color(Yellow, Black);
  TFT_unsigned_decimal(total_file, 1, 3);
  file_number = 0;                              // Display first file name
  TFT_filename();
  Check_valid_increment_file();

  MP3_end_sector = (file_size[file_number] >> 9) + MP3_start_sector[file_number];

  while (1) {
    // main 루프에서 화면 업데이트 조건 수정
    if (((GPIOC->IDR & 0x0080) == 0x0080) && (play_flag == 1)) {
        if (index == 512) {
            if (MP3_end_sector == MP3_start_sector[file_number]) {
                if (file_number != (total_file - 1))
                    file_number++;
                else if (file_number == (total_file - 1))
                    file_number = 0;

                TFT_filename();
                Check_valid_increment_file();

                MP3_start_sector[file_number] = MP3_start_backup[file_number];
                MP3_end_sector = (file_size[file_number] >> 9) + MP3_start_sector[file_number];
                VS1053b_software_reset();  // VS1053b software reset to change music file
            }
            index = 0;
            SD_read_sector(MP3_start_sector[file_number]++, MP3buffer);
        }

        for (i = 0; i < 32; i++) {           // Send 32 data bytes
            GPIOC->BSRR = 0x00400000;        // -MP3_DCS = 0
            SPI3_write(MP3buffer[index++]);  // Write a byte of MP3 data to VS1053b
            GPIOC->BSRR = 0x00000040;        // -MP3_DCS = 1
        }
    }

    // 일반 화면 업데이트는 FFT 모드가 아닐 때만 수행
    if (!fft_display_mode) {
        loop++;
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
    }

    key = Key_input();                       // Key input
    if (key == no_key)                       // If no key input, read touch screen icon
        key = Icon_input();

    switch (key) {
      case KEY1:
        play_flag ^= 0x01;               // Toggle play or stop
        if (play_flag == 1)
          TFT_string(33, 13, Yellow, Black, "재생중");
        else
          TFT_string(33, 13, Magenta, Black, " 정지 ");
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

      // key3 입력 시 각 기능의 값 증가, 최대값에 도달하면 최소값으로 돌아감
      case KEY3:
          if (func_mode == 0) {            // 다음 음악 선택
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

      // KEY4 처리 수정
      case KEY4:
          if (func_mode == 0) {            // 이전 음악 선택
              if (file_number != 0)
                  file_number--;
              else
                  file_number = total_file - 1;
      
              TFT_filename();
              Check_valid_decrement_file();
      
              MP3_start_sector[file_number] = MP3_start_backup[file_number];
              MP3_end_sector = (file_size[file_number] >> 9) + MP3_start_sector[file_number];
              index = 512;
              VS1053b_software_reset();
          } 
          else if (play_flag == 1) {  // 재생 중일 때만 FFT 처리
              fft_display_mode ^= 1;  // FFT 화면 모드 토글
              
              if (fft_display_mode) {
                  // FFT 화면으로 전환
                  Init_FFT_Screen();
                  FFT_Init();  // FFT 초기화
              } else {
                  // 일반 화면으로 복귀
                  Display_Normal_Screen();
              }
              loop = 0;  // 일반 화면 업데이트 카운터 초기화
          }
          break;

      default:
        break;
	  }

    // main 루프의 끝부분에 FFT 처리 추가
    if (play_flag == 1 && fft_display_mode) {
        if (fft_update_flag) {
            fft_update_flag = 0;
            
            // FFT 처리 및 표시
            PushAudioData(MP3buffer, 512);
            Process_FFT();
            
            switch(func_mode) {
                case 1:  // 전체 스펙트럼
                    Draw_Spectrum(FFT_Mag, FFT_SIZE/4, Magenta);
                    break;
                case 2:  // 저주파 영역
                    Draw_Spectrum(FFT_Mag, FFT_SIZE/8, Blue);
                    break;
                case 3:  // 고주파 영역
                    Draw_Spectrum(&FFT_Mag[FFT_SIZE/8], FFT_SIZE/8, Red);
                    break;
            }
        }
    }
  }
}

/* User-defined functions */

void TFT_filename(void) {
  unsigned char file_flag;
  unsigned short file_KB;

  TFT_string(0, 7, Cyan, Black, "----------------------------------------");
  TFT_string(3, 5, Green, Black, "                                     ");

  file_flag = Get_long_filename(file_number);   // Check file name

  if (file_flag == 0)                           // Short file name (8.3 format)
    TFT_short_filename(3, 5, Green, Black);
  else if (file_flag == 1)                      // Long file name
    TFT_long_filename(3, 5, Green, Black);
  else if (file_flag == 2)                      // File name is longer than 195 characters
    TFT_string(3, 5, Red, Black, "* 파일명 길이 초과 *");
  else                                          // File name error
    TFT_string(3, 5, Red, Black, "*** 파일명 오류 ***");

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

void Check_valid_increment_file(void) {
  unsigned char file_OK_flag = 0;
  do {
    if ((extension != 0x004D5033) && (extension != 0x00414143) &&
      (extension != 0x00574D41) && (extension != 0x004D4944)) {
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
    // Rectangle(12, 196, 67, 235, Magenta);

  } else if ((icon_flag == 0) && (x_touch >= 92) && (x_touch <= 147) &&
         (y_touch >= 196) && (y_touch <= 235)) {
    keyPressed = KEY2;
      icon_flag = 1;
    // Rectangle(92, 196, 147, 235, Magenta);

  } else if ((icon_flag == 0) && (x_touch >= 176) && (x_touch <= 231) &&
         (y_touch >= 196) && (y_touch <= 235)) {
    keyPressed = KEY3;
      icon_flag = 1;
    // Rectangle(176, 196, 231, 235, Magenta);

  } else if ((icon_flag == 0) && (x_touch >= 256) && (x_touch <= 311) &&
         (y_touch >= 196) && (y_touch <= 235)) {
    keyPressed = KEY4;
      icon_flag = 1;
    // Rectangle(256, 196, 311, 235, Magenta);

  } else if ((icon_flag == 1) && (x_touch == 0) && (y_touch == 0)) {
    keyPressed = no_key;
      icon_flag = 0;
    // Rectangle(12, 196, 67, 235, Yellow);
    // Rectangle(92, 196, 147, 235, Yellow);
    // Rectangle(176, 196, 231, 235, Yellow);
    // Rectangle(256, 196, 311, 235, Yellow);
      Delay_ms(50);
  } else {
    keyPressed = no_key;
  }

  return keyPressed;
}

// FFT 분석을 위한 함수들
void FFT_Init(void) {
    arm_rfft_fast_init_f32(&S, FFT_SIZE);
}

// 오디오 데이터 버퍼링 함수
void PushAudioData(uint8_t* data, uint16_t len) {
    for(uint16_t i = 0; i < len; i++) {
        audioBuffer.buffer[audioBuffer.writeIdx] = ((float32_t)data[i] - 128.0f) / 128.0f;
        audioBuffer.writeIdx = (audioBuffer.writeIdx + 1) % 512;
        if(audioBuffer.writeIdx == audioBuffer.readIdx) {
            audioBuffer.isFull = 1;
        }
    }
}

// FFT 처리 함수 개선
void Process_FFT(void) {
    static float32_t fftInBuffer[FFT_SIZE];
    
    // 오디오 버퍼에서 데이터 복사
    for(int i = 0; i < FFT_SIZE; i++) {
        if(audioBuffer.readIdx != audioBuffer.writeIdx || audioBuffer.isFull) {
            fftInBuffer[i] = audioBuffer.buffer[audioBuffer.readIdx];
            audioBuffer.readIdx = (audioBuffer.readIdx + 1) % 512;
            audioBuffer.isFull = 0;
        }
    }

    // FFT 처리
    arm_rfft_fast_f32(&S, fftInBuffer, FFT_Output, 0);
    arm_cmplx_mag_f32(FFT_Output, FFT_Mag, FFT_SIZE/2);

    // 주파수 빈 평균화 (노이즈 감소)
    for(int i = 1; i < FFT_SIZE/2-1; i++) {
        FFT_Mag[i] = (FFT_Mag[i-1] + FFT_Mag[i] + FFT_Mag[i+1]) / 3.0f;
    }
}

// Draw_Spectrum 함수 수정
void Draw_Spectrum(float32_t* data, uint16_t size, uint16_t color) {
    static uint32_t lastDrawTime = 0;
    const uint32_t DRAW_INTERVAL = 50;
    
    uint32_t currentTime = GetTick();
    if(currentTime - lastDrawTime < DRAW_INTERVAL) {
        return;
    }
    lastDrawTime = currentTime;

    // FFT 표시 영역 클리어
    Rectangle(0, FFT_DISPLAY_Y_START, 
             FFT_DISPLAY_WIDTH, 
             FFT_DISPLAY_Y_START + FFT_DISPLAY_HEIGHT, 
             Black);

    float32_t maxVal = 0.0f;
    uint16_t barWidth = FFT_DISPLAY_WIDTH / size;
    
    // 최대값 찾기
    for(int i = 0; i < size; i++) {
        if(data[i] > maxVal) maxVal = data[i];
    }
    
    maxVal = maxVal > 0 ? maxVal : 1.0f;
    
    // 스펙트럼 그리기
    for(int i = 0; i < size; i++) {
        uint16_t barHeight = (uint16_t)((data[i] * FFT_DISPLAY_HEIGHT) / maxVal);
        if(barHeight > FFT_DISPLAY_HEIGHT) {
            barHeight = FFT_DISPLAY_HEIGHT;
        }
        
        // 막대 그래프 그리기
        Rectangle(i * barWidth, 
                 FFT_DISPLAY_Y_START + FFT_DISPLAY_HEIGHT - barHeight,
                 (i + 1) * barWidth - 1,
                 FFT_DISPLAY_Y_START + FFT_DISPLAY_HEIGHT,
                 color);
    }
}

// 일반 화면 표시 함수
void Display_Normal_Screen(void) {
    TFT_clear_screen();
    TFT_string(0, 0, White, Magenta, "  OH-IN-PE-  ");
    TFT_string(0, 3, Cyan, Black, "----------------------------------------");
    TFT_string(0, 5, Magenta, Black, ">>");
    TFT_string(0, 7, Cyan, Black, "----------------------------------------");
    TFT_string(0, 9, Cyan, Black, "      파일 번호 : 000/000 (   kbps)     ");
    TFT_string(0,11, Cyan, Black, "      파일 용량 : 0000KB  (     Hz)     ");
    TFT_string(0,13, Cyan, Black, "      연주 진행 : 00:00(000%)   (      )");
    TFT_string(0,15, Cyan, Black, "----------------------------------------");
    TFT_string(0,17, Green, Black, "   음량(Volume) : 000%(000/250)         ");
    TFT_string(0,19, Green, Black, "   저음(Bass)   :  00 (00 ~ 15)         ");
    TFT_string(0,21, Green, Black, "   고음(Treble) :  00 (-8 ~ +7)         ");
    TFT_string(0,23, Cyan, Black, "----------------------------------------");
    TFT_string(0,25, Cyan, Black, "   KEY1      KEY2      KEY3      KEY4   ");
    TFT_string(0,27, Magenta, Black, "  (PLAY)    (FUNC)     (INC)     (DEC)  ");
    
    // 현재 상태 업데이트
    TFT_filename();
    TFT_volume();
    TFT_bass();
    TFT_treble();
}

// FFT 화면 초기화 함수 수정
void Init_FFT_Screen(void) {
    TFT_clear_screen();
    
    // 상단 타이틀
    TFT_string(0, 0, White, Magenta, "  FFT Analyzer  ");
    TFT_string(0, 2, Cyan, Black, "----------------------------------------");
    
    // FFT 모드 표시
    switch(func_mode) {
        case 1:
            TFT_string(0, 4, Yellow, Black, "Mode: Full Spectrum Analysis");
            break;
        case 2:
            TFT_string(0, 4, Blue, Black, "Mode: Low Frequency Analysis");
            break;
        case 3:
            TFT_string(0, 4, Red, Black, "Mode: High Frequency Analysis");
            break;
    }
    
    // FFT 표시 영역 초기화
    Rectangle(0, FFT_DISPLAY_Y_START, 
             FFT_DISPLAY_WIDTH, 
             FFT_DISPLAY_Y_START + FFT_DISPLAY_HEIGHT, 
             Black);

    // 하단 안내 메시지
    TFT_string(0, 27, White, Black, "Press KEY4 to return to normal mode");
}