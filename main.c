#include "stm32f767xx.h"
#include "OK-STM767.h"
#include "OK-STM767_SD_card.h"
#include "OK-STM767_VS1053b.h"
#include "arm_math.h"

// FFT ���� ��� ����
#define FFT_SIZE 256  // FFT ũ�� ����Ͽ� ó�� ���� ����
#define SAMPLE_FREQ 44100
#define FFT_DISPLAY_HEIGHT 80
#define FFT_DISPLAY_WIDTH 320
#define FFT_DISPLAY_Y_START 80  // UI�� ��ġ�� �ʴ� ��ġ

// ���� ���۸��� ���� ����ü
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

// FFT ���� ���� ���� �߰�
#define FFT_UPDATE_PERIOD 50  // FFT ������Ʈ �ֱ� (ms)

// FFT ȭ�� ��� �÷��� �߰�
volatile uint8_t func_mode = 0;        // ��� ���� ���
volatile uint8_t fft_display_mode = 0;  // 0: �Ϲ� ȭ��, 1: FFT ȭ��
volatile uint8_t fft_update_flag = 0;   // FFT ������Ʈ �÷���

void TFT_filename(void);                          // Display MP3 file name, number, size
void TFT_volume(void);                            // Display volume
void TFT_bass(void);                              // Display bass
void TFT_treble(void);                            // Display treble
void Check_valid_increment_file(void);            // Check if valid file for increment
void Check_valid_decrement_file(void);            // Check if valid file for decrement
void TFT_MP3_bitrate(U16 highbyte, U16 lowbyte);  // Display MP3 file bitrate

void FFT_Init(void);                              // FFT �ʱ�ȭ
void Process_FFT(void);                           // FFT ó��
void Draw_Spectrum(float32_t* data, uint16_t size, uint16_t color);// FFT ��� �׷��� ���
void PushAudioData(uint8_t* data, uint16_t len);  // ����� ������ ���ۿ� ����
unsigned char Icon_input(void);                   // Input touch screen icon

void Display_Normal_Screen(void);                 // �Ϲ� ȭ�� ǥ�� �Լ�
void Init_FFT_Screen(void);                       // FFT ȭ�� �ʱ�ȭ

unsigned char total_file;                         // Total file number
unsigned char file_number = 0;                    // Current file number

// �ý��� Ÿ�̸� ƽ ī��Ʈ ����
static volatile uint32_t SystemTicks = 0;

// SysTick ���ͷ�Ʈ �ڵ鷯
void SysTick_Handler(void) {
    static uint32_t fft_tick_count = 0;
    SystemTicks++;
    
    // FFT ������Ʈ Ÿ�̹� ����
    fft_tick_count++;
    if(fft_tick_count >= FFT_UPDATE_PERIOD) {
        fft_update_flag = 1;
        fft_tick_count = 0;
    }
}

// �ý��� Ÿ�̸� �ʱ�ȭ �Լ�
void SystemTick_Init(void) {
    // SysTick ���� (1ms ����)
    SysTick->LOAD = (SystemCoreClock / 1000) - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
}

// GetTick �Լ� ����
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
  TFT_string(0, 23, Cyan, Black, "           SD ī�� �ʱ�ȭ...            ");
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
  TFT_string(0, 9, Cyan, Black, "      ���� ��ȣ : 000/000 (   kbps)     ");
  TFT_string(0,11, Cyan, Black, "      ���� �뷮 : 0000KB  (     Hz)     ");
  TFT_string(0,13, Cyan, Black, "      ���� ���� : 00:00(000%)   (      )");
  TFT_string(0,15, Cyan, Black, "----------------------------------------");
  TFT_string(0,17, Green, Black, "   ����(Volume) : 000%(000/250)         ");
  TFT_string(0,19, Green, Black, "   ����(Bass)   :  00 (00 ~ 15)         ");
  TFT_string(0,21, Green, Black, "   ����(Treble) :  00 (-8 ~ +7)         ");
  TFT_string(0,23, Cyan, Black, "----------------------------------------");
  TFT_string(0,25, Cyan, Black, "   KEY1      KEY2      KEY3      KEY4   ");
  TFT_string(0,27, Magenta, Black, "  (PLAY)    (FUNC)     (INC)     (DEC)  ");
  TFT_string(27,11, Yellow, Black, "00000");
  TFT_string(18,13, Magenta, Black, "00");
  TFT_string(21,13, Magenta, Black, "00");
  TFT_string(33,13, Magenta, Black, " ���� ");

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
    // main �������� ȭ�� ������Ʈ ���� ����
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

    // �Ϲ� ȭ�� ������Ʈ�� FFT ��尡 �ƴ� ���� ����
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
          TFT_string(33, 13, Yellow, Black, "�����");
        else
          TFT_string(33, 13, Magenta, Black, " ���� ");
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

      // key3 �Է� �� �� ����� �� ����, �ִ밪�� �����ϸ� �ּҰ����� ���ư�
      case KEY3:
          if (func_mode == 0) {            // ���� ���� ����
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

      // KEY4 ó�� ����
      case KEY4:
          if (func_mode == 0) {            // ���� ���� ����
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
          else if (play_flag == 1) {  // ��� ���� ���� FFT ó��
              fft_display_mode ^= 1;  // FFT ȭ�� ��� ���
              
              if (fft_display_mode) {
                  // FFT ȭ������ ��ȯ
                  Init_FFT_Screen();
                  FFT_Init();  // FFT �ʱ�ȭ
              } else {
                  // �Ϲ� ȭ������ ����
                  Display_Normal_Screen();
              }
              loop = 0;  // �Ϲ� ȭ�� ������Ʈ ī���� �ʱ�ȭ
          }
          break;

      default:
        break;
	  }

    // main ������ ���κп� FFT ó�� �߰�
    if (play_flag == 1 && fft_display_mode) {
        if (fft_update_flag) {
            fft_update_flag = 0;
            
            // FFT ó�� �� ǥ��
            PushAudioData(MP3buffer, 512);
            Process_FFT();
            
            switch(func_mode) {
                case 1:  // ��ü ����Ʈ��
                    Draw_Spectrum(FFT_Mag, FFT_SIZE/4, Magenta);
                    break;
                case 2:  // ������ ����
                    Draw_Spectrum(FFT_Mag, FFT_SIZE/8, Blue);
                    break;
                case 3:  // ������ ����
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
    TFT_string(3, 5, Red, Black, "* ���ϸ� ���� �ʰ� *");
  else                                          // File name error
    TFT_string(3, 5, Red, Black, "*** ���ϸ� ���� ***");

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

// FFT �м��� ���� �Լ���
void FFT_Init(void) {
    arm_rfft_fast_init_f32(&S, FFT_SIZE);
}

// ����� ������ ���۸� �Լ�
void PushAudioData(uint8_t* data, uint16_t len) {
    for(uint16_t i = 0; i < len; i++) {
        audioBuffer.buffer[audioBuffer.writeIdx] = ((float32_t)data[i] - 128.0f) / 128.0f;
        audioBuffer.writeIdx = (audioBuffer.writeIdx + 1) % 512;
        if(audioBuffer.writeIdx == audioBuffer.readIdx) {
            audioBuffer.isFull = 1;
        }
    }
}

// FFT ó�� �Լ� ����
void Process_FFT(void) {
    static float32_t fftInBuffer[FFT_SIZE];
    
    // ����� ���ۿ��� ������ ����
    for(int i = 0; i < FFT_SIZE; i++) {
        if(audioBuffer.readIdx != audioBuffer.writeIdx || audioBuffer.isFull) {
            fftInBuffer[i] = audioBuffer.buffer[audioBuffer.readIdx];
            audioBuffer.readIdx = (audioBuffer.readIdx + 1) % 512;
            audioBuffer.isFull = 0;
        }
    }

    // FFT ó��
    arm_rfft_fast_f32(&S, fftInBuffer, FFT_Output, 0);
    arm_cmplx_mag_f32(FFT_Output, FFT_Mag, FFT_SIZE/2);

    // ���ļ� �� ���ȭ (������ ����)
    for(int i = 1; i < FFT_SIZE/2-1; i++) {
        FFT_Mag[i] = (FFT_Mag[i-1] + FFT_Mag[i] + FFT_Mag[i+1]) / 3.0f;
    }
}

// Draw_Spectrum �Լ� ����
void Draw_Spectrum(float32_t* data, uint16_t size, uint16_t color) {
    static uint32_t lastDrawTime = 0;
    const uint32_t DRAW_INTERVAL = 50;
    
    uint32_t currentTime = GetTick();
    if(currentTime - lastDrawTime < DRAW_INTERVAL) {
        return;
    }
    lastDrawTime = currentTime;

    // FFT ǥ�� ���� Ŭ����
    Rectangle(0, FFT_DISPLAY_Y_START, 
             FFT_DISPLAY_WIDTH, 
             FFT_DISPLAY_Y_START + FFT_DISPLAY_HEIGHT, 
             Black);

    float32_t maxVal = 0.0f;
    uint16_t barWidth = FFT_DISPLAY_WIDTH / size;
    
    // �ִ밪 ã��
    for(int i = 0; i < size; i++) {
        if(data[i] > maxVal) maxVal = data[i];
    }
    
    maxVal = maxVal > 0 ? maxVal : 1.0f;
    
    // ����Ʈ�� �׸���
    for(int i = 0; i < size; i++) {
        uint16_t barHeight = (uint16_t)((data[i] * FFT_DISPLAY_HEIGHT) / maxVal);
        if(barHeight > FFT_DISPLAY_HEIGHT) {
            barHeight = FFT_DISPLAY_HEIGHT;
        }
        
        // ���� �׷��� �׸���
        Rectangle(i * barWidth, 
                 FFT_DISPLAY_Y_START + FFT_DISPLAY_HEIGHT - barHeight,
                 (i + 1) * barWidth - 1,
                 FFT_DISPLAY_Y_START + FFT_DISPLAY_HEIGHT,
                 color);
    }
}

// �Ϲ� ȭ�� ǥ�� �Լ�
void Display_Normal_Screen(void) {
    TFT_clear_screen();
    TFT_string(0, 0, White, Magenta, "  OH-IN-PE-  ");
    TFT_string(0, 3, Cyan, Black, "----------------------------------------");
    TFT_string(0, 5, Magenta, Black, ">>");
    TFT_string(0, 7, Cyan, Black, "----------------------------------------");
    TFT_string(0, 9, Cyan, Black, "      ���� ��ȣ : 000/000 (   kbps)     ");
    TFT_string(0,11, Cyan, Black, "      ���� �뷮 : 0000KB  (     Hz)     ");
    TFT_string(0,13, Cyan, Black, "      ���� ���� : 00:00(000%)   (      )");
    TFT_string(0,15, Cyan, Black, "----------------------------------------");
    TFT_string(0,17, Green, Black, "   ����(Volume) : 000%(000/250)         ");
    TFT_string(0,19, Green, Black, "   ����(Bass)   :  00 (00 ~ 15)         ");
    TFT_string(0,21, Green, Black, "   ����(Treble) :  00 (-8 ~ +7)         ");
    TFT_string(0,23, Cyan, Black, "----------------------------------------");
    TFT_string(0,25, Cyan, Black, "   KEY1      KEY2      KEY3      KEY4   ");
    TFT_string(0,27, Magenta, Black, "  (PLAY)    (FUNC)     (INC)     (DEC)  ");
    
    // ���� ���� ������Ʈ
    TFT_filename();
    TFT_volume();
    TFT_bass();
    TFT_treble();
}

// FFT ȭ�� �ʱ�ȭ �Լ� ����
void Init_FFT_Screen(void) {
    TFT_clear_screen();
    
    // ��� Ÿ��Ʋ
    TFT_string(0, 0, White, Magenta, "  FFT Analyzer  ");
    TFT_string(0, 2, Cyan, Black, "----------------------------------------");
    
    // FFT ��� ǥ��
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
    
    // FFT ǥ�� ���� �ʱ�ȭ
    Rectangle(0, FFT_DISPLAY_Y_START, 
             FFT_DISPLAY_WIDTH, 
             FFT_DISPLAY_Y_START + FFT_DISPLAY_HEIGHT, 
             Black);

    // �ϴ� �ȳ� �޽���
    TFT_string(0, 27, White, Black, "Press KEY4 to return to normal mode");
}