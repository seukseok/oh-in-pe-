// main.c
#include "stm32f767xx.h"
#include "OK-STM767.h"
#include "OK-STM767_SD_card.h"
#include "OK-STM767_VS1053b.h"
#include "arm_math.h"

/*******************************************************************************
 * 상수 정의
 ******************************************************************************/
#define THEME_BG        Black       // 배경색
#define THEME_HEADER    0x861D      // 진한 보라색
#define THEME_TEXT      0xBDF7      // 밝은 회색
#define THEME_HIGHLIGHT 0x05FF      // 청록색
#define THEME_ACCENT    0xFD20      // 산호색
#define THEME_BUTTON    0x2DB7      // 청남색
#define THEME_WARNING   0xFBE0      // amber색

#define BAR_WIDTH       15 
#define BAR_GAP         4
#define START_Y         220
#define MAX_HEIGHT      140
#define INITIAL_VOLUME  180
#define BUFFER_SIZE     512
#define BUFFER_COUNT    2
#define UPDATE_PERIOD   20    /* 스펙트럼 업데이트 주기(ms) */

// 디버그 정보 표시를 위한 상수 추가
#define DEBUG_MODE      1
#define FREQ_BANDS      16

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

void SetupMainScreen(void);                       // 메인 화면 구성 함수 추가

/* 추가된 함수 */
void InitSpectrum(void);
void SendMP3Data(uint8_t* buffer, uint16_t* index);
void PlayAndDrawSpectrum(void);
void UpdateSpectrum(void);
void DrawSpectrumBar(uint8_t index, uint16_t height);
void DebugSpectrum(void);

/*******************************************************************************
 * 전역 변수
 ******************************************************************************/
unsigned char total_file;                         // Total file number
unsigned char file_number = 0;                    // Current file number

/* 추가된 전역 변수 */
volatile uint32_t SysTick_Count = 0;
volatile uint8_t currentBuffer = 0;
static uint16_t prevHeight[16] = {0};
uint32_t MP3_start_sector[MAX_FILE];
uint32_t MP3_start_backup[MAX_FILE];
uint32_t MP3_end_sector;
uint8_t play_flag = 0;                            // Play or stop flag
uint8_t MP3buffer[BUFFER_COUNT][BUFFER_SIZE];

/*******************************************************************************
 * SysTick 관련 함수
 ******************************************************************************/
void SysTick_Handler(void) {
    SysTick_Count++;
}

void SysTick_Initialize(void) {
    SysTick->LOAD = 216000 - 1;    /* 216MHz/1000 = 216000 (1ms 주기) */
    SysTick->VAL = 0;              /* 카운터 초기화 */
    SysTick->CTRL = 0x07;          /* 클록 소스, 인터럽트, 타이머 활성화 */
}

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
    TFT_string(0, 23, Cyan, Black, "           SD 카드 초기화...            ");
    Beep();
    Delay_ms(1000);
    TFT_clear_screen();

    Initialize_SD();                              // Initialize SD card
    Initialize_FAT32();                           // Initialize FAT32 file system
    Initialize_VS1053b();                         // Initialize VS1053b
    SysTick_Initialize();                         // SysTick 초기화
    Delay_ms(1000);

    volume = 175;                       // 볼륨 변수
    bass = 10;                          // 베이스 변수
    treble = 5;                         // 트레블 변수

    VS1053b_SetVolume(volume);
    Delay_ms(1);
    VS1053b_SetBassTreble(bass, treble);

    total_file = fatGetDirEntry(FirstDirCluster); // Get total file number

    for (i = 0; i < total_file; i++) {            // Get start address of all files
        MP3_start_sector[i] = fatClustToSect(file_start_cluster[i]);
        MP3_start_backup[i] = MP3_start_sector[i];
    }

    file_number = 0;                              // Initialize file number

    // 메인 화면 구성 함수 호출
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
                    TFT_string(33, 13, THEME_HIGHLIGHT, THEME_BG, "재생중");
                else
                    TFT_string(33, 13, THEME_TEXT, THEME_BG, " 정지 ");
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
                        volume = 5;  // 최소값으로 롤오버
                    }
                    VS1053b_SetVolume(volume);
                    TFT_volume();
                } else if (func_mode == 2) {     // 베이스 증가
                    bass++;
                    if (bass > 15) {
                        bass = 0;  // 최소값으로 롤오버
                    }
                    VS1053b_SetBassTreble(bass, treble);
                    TFT_bass();
                } else {                         // 트레블 증가
                    treble++;
                    if (treble > 7) {
                        treble = -8;  // 최소값으로 롤오버
                    }
                    VS1053b_SetBassTreble(bass, treble);
                    TFT_treble();
                }
                break;

            case KEY4:
                // KEY4를 누르면 스펙트럼 분석기로 전환 또는 복귀
                if (func_mode == 0) {
                    TFT_clear_screen();
                    InitSpectrum();

                    while (1) {
                        PlayAndDrawSpectrum();

                        // 종료 조건을 위해 키 입력 확인
                        key = Key_input();
                        if (key == KEY4) {
                            // 스펙트럼 분석기 종료 후 메인 화면으로 복귀
                            TFT_clear_screen();
                            SetupMainScreen(); // 메인 화면 재구성 함수 호출
                            break;
                        }
                    }
                } else if (func_mode == 1) {     // 볼륨 감소
                    if (volume > 5)
                        volume--;
                    else
                        volume = 250;  // 최대값으로 롤오버
                    VS1053b_SetVolume(volume);
                    TFT_volume();
                } else if (func_mode == 2) {     // 베이스 감소
                    if (bass > 0)
                        bass--;
                    else
                        bass = 15;  // 최대값으로 롤오버
                    VS1053b_SetBassTreble(bass, treble);
                    TFT_bass();
                } else {                         // 트레블 감소
                    if (treble > -8)
                        treble--;
                    else
                        treble = 7;  // 최대값으로 롤오버
                    VS1053b_SetBassTreble(bass, treble);
                    TFT_treble();
                }
                break;

            default:
                break;
        }
    }
}

/* 메인 화면 구성 함수 */
void SetupMainScreen(void) {
    // 메인 화면 구성
    TFT_clear_screen();                           
    TFT_string(0, 0, Black, THEME_HEADER, "  OH-IN-PE-  ");
    TFT_string(0, 3, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0, 5, THEME_HIGHLIGHT, THEME_BG, ">>");
    TFT_string(0, 7, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0, 9, THEME_TEXT, THEME_BG, "      파일 번호 : 000/000 (   kbps)     ");
    TFT_string(0,11, THEME_TEXT, THEME_BG, "      파일 용량 : 0000KB  (     Hz)     ");
    TFT_string(0,13, THEME_TEXT, THEME_BG, "     Music Play : 00:00(000%)   (      )");
    TFT_string(0,15, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0,17, THEME_TEXT, THEME_BG, "   음량(Volume) : 000%(000/250)         ");
    TFT_string(0,19, THEME_TEXT, THEME_BG, "   저음(Bass)   :  00 (00 ~ 15)         ");
    TFT_string(0,21, THEME_TEXT, THEME_BG, "   고음(Treble) :  00 (-8 ~ +7)         ");
    TFT_string(0,23, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0,25, THEME_TEXT, THEME_BG, "   KEY1      KEY2      KEY3      KEY4   ");
    TFT_string(0,27, THEME_ACCENT, THEME_BG, "  (PLAY)   (select)    (up)     (mode)  ");
    
    // 버튼 영역
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

/* 추가된 함수들 */

// 스펙트럼 분석기 초기화 함수
void InitSpectrum(void) {
    VS1053b_software_reset();
    Delay_ms(50);
    
    /* 스펙트럼 분석기 모드 활성화 */
    uint16_t mode = VS1053b_SCI_Read(0x00);
    mode |= 0x0020;
    VS1053b_SCI_Write(0x00, mode);
    
    /* 기본 설정 */
    VS1053b_SCI_Write(0x03, 0xC000);    /* 클록 설정 */
    VS1053b_SCI_Write(0x05, 0xAC45);    /* 48kHz, 스테레오 */
    VS1053b_SetVolume(INITIAL_VOLUME);  /* 볼륨 설정 */
    VS1053b_SetBassTreble(10, 3);       /* 음질 설정 */
    VS1053b_SCI_Write(0x07, 0x0020);    /* 주파수 응답 보정 */
    
    Delay_ms(10);

    // 스펙트럼 화면 구성
    TFT_clear_screen();
    TFT_string(0, 0, White, Magenta, "VS1053B Spectrum Analyzer");

    // 초기 막대 상태 초기화
    for (uint8_t i = 0; i < 16; i++) {
        prevHeight[i] = 0;
    }
}

// MP3 데이터를 VS1053b로 전송하는 함수
void SendMP3Data(uint8_t* buffer, uint16_t* index) {
    GPIOC->BSRR = 0x00400000;      /* -MP3_DCS = 0 */
    
    /* 32바이트 블록 단위로 전송 */
    for(uint8_t i = 0; i < 32; i++) {
        while((GPIOC->IDR & 0x0080) == 0);  /* DREQ 대기 */
        SPI3_write(buffer[(*index)++]);
    }
    
    GPIOC->BSRR = 0x00000040;      /* -MP3_DCS = 1 */
}

// 스펙트럼 바를 그리는 함수
void DrawSpectrumBar(uint8_t index, uint16_t height) {
    uint16_t x = 20 + index * (BAR_WIDTH + BAR_GAP);
    uint16_t color;
    
    // 주파수 대역별 고정 색상 사용
    if(index < 5) {
        color = Blue;         // 저주파 - 파란색
    }
    else if(index < 11) {
        color = Magenta;      // 중주파 - 마젠타
    }
    else {
        color = Cyan;         // 고주파 - 시안
    }
    
    // 이전 막대 지우기
    Rectangle(x, START_Y - prevHeight[index],
             x + BAR_WIDTH, START_Y, Black);
             
    // 새 막대 그리기
    Rectangle(x, START_Y - height,
             x + BAR_WIDTH, START_Y, color);
    
    prevHeight[index] = height;
}

// 스펙트럼 업데이트 함수
void UpdateSpectrum(void) {
    // 주파수 대역별 감도 조정
    static const float bands[16] = {
        0.70f, 0.65f, 0.60f, 0.55f, 0.50f,    // 저주파 (20Hz-640Hz)
        0.45f, 0.40f, 0.35f, 0.30f, 0.25f,    // 중주파 (640Hz-10kHz)
        0.20f, 0.18f, 0.16f, 0.14f, 0.12f, 0.10f  // 고주파 (10kHz-20kHz)
    };
    
    // 이동 평균 필터용 버퍼
    static float prevValues[16][4] = {0};
    static uint8_t filterIndex = 0;
    
    for(uint8_t i = 0; i < 16; i++) {
        uint16_t raw_value = VS1053b_SCI_Read(0x0C + i);
        
        // 이동 평균 필터 적용
        prevValues[i][filterIndex] = (float)raw_value;
        float avgValue = 0;
        for(uint8_t j = 0; j < 4; j++) {
            avgValue += prevValues[i][j];
        }
        avgValue /= 4.0f;
        
        // 로그 스케일 변환 개선
        float normalized = log10f(avgValue + 1.0f) / log10f(256.0f);
        normalized = normalized * normalized; // 제곱하여 동적 범위 확장
        
        // 가중치 적용
        float weighted = normalized * bands[i];
        
        // 높이 계산
        uint16_t height = (uint16_t)(weighted * MAX_HEIGHT);
        
        // 히스테리시스 적용
        static uint16_t prevHeightLocal = 0;
        if(abs(height - prevHeightLocal) < (MAX_HEIGHT * 0.1f)) {
            height = (height + prevHeightLocal * 3) / 4; // 부드러운 변화
        }
        prevHeightLocal = height;
        
        // 최종 높이 제한
        if(height > MAX_HEIGHT) height = MAX_HEIGHT;
        
        DrawSpectrumBar(i, height);
    }
    
    filterIndex = (filterIndex + 1) & 0x03; // 순환 버퍼 인덱스 업데이트
    
    #if DEBUG_MODE
    DebugSpectrum();  // 디버그 정보 표시
    #endif
}

// 스펙트럼을 플레이하고 그리는 함수
void PlayAndDrawSpectrum(void) {
    static uint16_t index = 0;
    static uint32_t lastUpdate = 0;
    
    /* 오디오 데이터 처리 */
    if((GPIOC->IDR & 0x0080) && play_flag) {
        if(index >= BUFFER_SIZE) {
            uint8_t nextBuffer = currentBuffer ^ 1;
            
            if(MP3_end_sector == MP3_start_sector[file_number]) {
                file_number = (file_number + 1) % total_file;
                MP3_start_sector[file_number] = MP3_start_backup[file_number];
                MP3_end_sector = (file_size[file_number] >> 9) + 
                                MP3_start_sector[file_number];
                VS1053b_software_reset();
                InitSpectrum();
            }
            
            SD_read_sector(MP3_start_sector[file_number]++, 
                          MP3buffer[nextBuffer]);
            currentBuffer = nextBuffer;
            index = 0;
        }

        SendMP3Data(MP3buffer[currentBuffer], &index);
    }
    
    /* 스펙트럼 업데이트 */
    if(SysTick_Count - lastUpdate >= UPDATE_PERIOD) {
        UpdateSpectrum();
        lastUpdate = SysTick_Count;
    }
    
    /* 디버그 정보 업데이트 */
    if (DEBUG_MODE) {
        DebugSpectrum();
    }
}

// 스펙트럼 데이터 검증용 함수
void DebugSpectrum(void) {
    static uint32_t lastDebugTime = 0;
    
    if(SysTick_Count - lastDebugTime >= 1000) { // 1초마다 업데이트
        lastDebugTime = SysTick_Count;
        
        TFT_xy(0, 2);  // 디버그 정보 표시 위치
        
        for(uint8_t i = 0; i < FREQ_BANDS; i++) {
            uint16_t raw_value = VS1053b_SCI_Read(0x0C + i);
            char debugStr[32];
            sprintf(debugStr, "B%02d:%4d", i, raw_value);
            TFT_string(0, 2+i, White, Black, debugStr);
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