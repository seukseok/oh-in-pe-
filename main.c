// main.c
#include "stm32f767xx.h"
#include "OK-STM767.h"
#include "OK-STM767_SD_card.h"
#include "OK-STM767_VS1053b.h"
#include "arm_math.h"
#include "arm_const_structs.h"

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
#define INITIAL_VOLUME  200
#define BUFFER_SIZE     512    // WAV 데이터 버퍼 크기 (샘플 수 * 2바이트)
#define BUFFER_COUNT    2
#define UPDATE_PERIOD   20            // 스펙트럼 업데이트 주기(ms)

#define FFT_SIZE        1024          // FFT 크기 (2의 거듭제곱)

#define WAV_HEADER_SIZE 44            // WAV 헤더 크기

// WAV 파일 재생을 위한 추가 상수
#define WAV_HEADER_SIZE 44
#define SAMPLE_RATE     48000
#define CHANNELS        2
#define BITS_PER_SAMPLE 16

/*******************************************************************************
 * 함수 선언
 ******************************************************************************/
void TFT_filename(void);                          // 파일 이름, 번호, 크기 표시
void TFT_volume(void);                            // 볼륨 표시
void TFT_bass(void);                              // 베이스 표시
void TFT_treble(void);                            // 트레블 표시
void Check_valid_increment_file(void);            // 유효한 다음 파일 확인
void Check_valid_decrement_file(void);            // 유효한 이전 파일 확인
void TFT_MP3_bitrate(U16 highbyte, U16 lowbyte);
unsigned char Icon_input(void);                   // 터치 스크린 아이콘 입력

void SetupMainScreen(void);                       // 메인 화면 구성 함수

// WAV 파일 관련 함수
uint8_t ParseWAVHeader(uint32_t sector);
void ConfigureVS1053ForWAV(void);
void SendWAVData(uint8_t* buffer, uint16_t* index);

// FFT 관련 함수
void FFT_Init(void);
void PrepareFFTInput(uint8_t* buffer);
void PerformFFT(void);
void DrawSpectrum(void);
void DrawSpectrumBar(uint8_t index, uint16_t height);
void Initialize_VS1053b(void);                    // VS1053B 초기화 함수

/*******************************************************************************
 * 전역 변수
 ******************************************************************************/
unsigned char total_file;                         // 총 파일 수
unsigned char file_number = 0;                    // 현재 파일 번호

volatile uint32_t SysTick_Count = 0;
volatile uint8_t currentBuffer = 0;
uint32_t WAV_start_sector[MAX_FILE];
uint32_t WAV_start_backup[MAX_FILE];
uint32_t WAV_end_sector;
uint8_t play_flag = 0;                            // 재생 또는 정지 플래그
uint8_t WAVbuffer[BUFFER_COUNT][BUFFER_SIZE];
uint32_t file_start_cluster[MAX_FILE];
uint32_t file_size[MAX_FILE];
uint16_t volume = INITIAL_VOLUME;
uint8_t FFT_mode = 0; // FFT 모드 변수 추가

// WAV 파일 헤더 정보를 저장할 구조체
typedef struct {
    uint32_t sampleRate;
    uint16_t numChannels;
    uint16_t bitsPerSample;
    uint32_t dataSize;
} WAV_Header;

WAV_Header wavHeader;

// FFT 입력 및 출력 버퍼
float32_t fftInput[FFT_SIZE * 2];    // 복소수 입력 (실수부와 허수부)
float32_t fftOutput[FFT_SIZE];       // 크기 계산 결과

// FFT 인스턴스
const arm_cfft_instance_f32 *S;     // FFT 구조체

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

    Initialize_MCU();                             // MCU 및 키트 초기화
    Delay_ms(50);                                 // 시스템 안정화 대기
    Initialize_TFT_LCD();                         // TFT-LCD 모듈 초기화
    Initialize_touch_screen();                    // 터치 스크린 초기화

    TFT_string(0, 4, Green, Black, "****************************************");
    TFT_string(0, 6, White, Black, "                OH-IN-PE-               ");
    TFT_string(0, 8, Green, Black, "****************************************");
    TFT_string(0, 23, Cyan, Black, "           SD 카드 초기화...            ");
    Beep();
    Delay_ms(1000);
    TFT_clear_screen();

    Initialize_SD();                              // SD 카드 초기화
    Initialize_FAT32();                           // FAT32 파일 시스템 초기화
    Initialize_VS1053b();                         // VS1053b 초기화
    SysTick_Initialize();                         // SysTick 초기화
    Delay_ms(1000);

    volume = 175;                       // 볼륨 변수
    bass = 10;                          // 베이스 변수
    treble = 5;                         // 트레블 변수

    VS1053b_SetVolume(volume);
    Delay_ms(1);
    VS1053b_SetBassTreble(bass, treble);

    total_file = fatGetDirEntry(FirstDirCluster); // 총 파일 수 가져오기

    for (i = 0; i < total_file; i++) {            // 모든 파일의 시작 섹터 주소 가져오기
        WAV_start_sector[i] = fatClustToSect(file_start_cluster[i]);
        WAV_start_backup[i] = WAV_start_sector[i];
    }

    file_number = 0;                              // 파일 번호 초기화

    // 메인 화면 구성 함수 호출
    SetupMainScreen();

    while (1) {
        if (((GPIOC->IDR & 0x0080) == 0x0080) && (play_flag == 1)) {
            if (index == 512) {
                if (WAV_start_sector[file_number] >= WAV_end_sector) {
                    if (file_number != (total_file - 1))
                        file_number++;
                    else
                        file_number = 0;

                    TFT_filename();
                    Check_valid_increment_file();

                    WAV_start_sector[file_number] = WAV_start_backup[file_number];
                    WAV_end_sector = (file_size[file_number] >> 9) + WAV_start_sector[file_number];
                    VS1053b_software_reset();  // VS1053b software reset to change music file
                }
                index = 0;
                SD_read_sector(WAV_start_sector[file_number]++, WAVbuffer[currentBuffer]);
            }

            for (i = 0; i < 32; i++) {           // Send 32 data bytes
                GPIOC->BSRR = 0x00400000;        // -MP3_DCS = 0
                SPI3_write(WAVbuffer[currentBuffer][index++]);  // Write a byte of MP3 data to VS1053b
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

            playPercentage = WAV_end_sector - WAV_start_sector[file_number];
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


        key = Key_input();                       // 키 입력
        if (key == no_key)                       // 키 입력이 없으면 터치 스크린 아이콘 입력 읽기
            key = Icon_input();

        switch (key) {
            case KEY1:
                play_flag ^= 0x01;               // 재생 또는 정지 토글
                if (play_flag == 1) 
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
                // 볼륨, 베이스, 트레블 조절 또는 다음 파일 선택
                if (func_mode == 0) {            // 다음 음악 선택
                    if (file_number != (total_file - 1))
                        file_number++;
                    else
                        file_number = 0;

                    VS1053b_software_reset();
                    TFT_filename();
                    Check_valid_increment_file();

                    WAV_start_sector[file_number] = WAV_start_backup[file_number];
                    WAV_end_sector = (file_size[file_number] >> 9) + WAV_start_sector[file_number];
                    index = 512;
                    VS1053b_software_reset();
                } else if (func_mode == 1) {     // 볼륨 증가
                    if (volume < 250)
                        volume++;
                    else
                        volume = 5;
                    VS1053b_SetVolume(volume);
                    TFT_volume();
                } else if (func_mode == 2) {     // 베이스 증가
                    if (bass < 15)
                        bass++;
                    else
                        bass = 0;
                    VS1053b_SetBassTreble(bass, treble);
                    TFT_bass();
                } else {                         // 트레블 증가
                    if (treble < 7)
                        treble++;
                    else
                        treble = -8;
                    VS1053b_SetBassTreble(bass, treble);
                    TFT_treble();
                }
                break;

            case KEY4:
                // FFT 모드 토글
                FFT_mode ^= 1;
                if (FFT_mode == 1) {
                    TFT_string(0, 23, THEME_HIGHLIGHT, THEME_BG, "FFT 스펙트럼 활성화");
                } else {
                    TFT_string(0, 23, THEME_TEXT, THEME_BG, "FFT 스펙트럼 비활성화");
                    // 스펙트럼 영역 지우기 (필요시 구현)
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
    TFT_string(0,27, THEME_ACCENT, THEME_BG, "  (PLAY)   (select)     (up)     (mode) ");
    
    // 버튼 영역
    Rectangle(12, 196, 67, 235, THEME_BUTTON);    
    Rectangle(92, 196, 147, 235, THEME_BUTTON);
    Rectangle(176, 196, 231, 235, THEME_BUTTON);
    Rectangle(256, 196, 311, 235, THEME_BUTTON);

    TFT_volume();                                 // 초기 볼륨 표시
    TFT_bass();
    TFT_treble();

    TFT_xy(22, 9);                                // 총 파일 수 표시
    TFT_color(Yellow, Black);
    TFT_unsigned_decimal(total_file, 1, 3);
    TFT_filename();
    Check_valid_increment_file();

    // WAV 파일 헤더 파싱 및 종료 섹터 설정
    WAV_end_sector = (file_size[file_number] >> 9) + WAV_start_sector[file_number];
}

/*******************************************************************************
 * WAV 파일 관련 함수
 ******************************************************************************/

// WAV 파일 헤더 파싱
uint8_t ParseWAVHeader(uint32_t sector) {
    uint8_t header[WAV_HEADER_SIZE];
    SD_read_sector(sector, header);

    // WAV 파일 시그니처 확인 ("RIFF" & "WAVE")
    if (header[0] != 'R' || header[1] != 'I' || header[2] != 'F' || header[3] != 'F' ||
        header[8] != 'W' || header[9] != 'A' || header[10] != 'V' || header[11] != 'E') {
        return 0;
    }

    // 헤더 정보 추출
    wavHeader.sampleRate = *(uint32_t*)&header[24];
    wavHeader.numChannels = *(uint16_t*)&header[22];
    wavHeader.bitsPerSample = *(uint16_t*)&header[34];
    wavHeader.dataSize = *(uint32_t*)&header[40];

    return 1;
}

// VS1053B WAV 재생 설정
void ConfigureVS1053ForWAV(void) {
    VS1053b_software_reset();
    Delay_ms(10);
    
    // Native SPI 모드 활성화
    VS1053b_SCI_Write(0x00, 0x0820);
    Delay_ms(1);
    
    // 샘플링 레이트 설정
    VS1053b_SCI_Write(0x05, 0xAC45);  // 48kHz, 스테레오
    
    // 볼륨 및 음질 설정
    VS1053b_SetVolume(volume);
    VS1053b_SetBassTreble(8, 3);
}

// WAV 데이터 전송
void SendWAVData(uint8_t* buffer, uint16_t* index) {
    GPIOC->BSRR = 0x00400000;      // -MP3_DCS = 0
    
    for(uint8_t i = 0; i < 32; i++) {
        while((GPIOC->IDR & 0x0080) == 0); // DREQ 대기
        SPI3_write(buffer[(*index)++]);
    }
    
    GPIOC->BSRR = 0x00000040;      // -MP3_DCS = 1
}

/*******************************************************************************
 * FFT 관련 함수
 ******************************************************************************/

// FFT 초기화 함수
void FFT_Init(void) {
    switch (FFT_SIZE) {
        case 16:
            S = &arm_cfft_sR_f32_len16;
            break;
        case 32:
            S = &arm_cfft_sR_f32_len32;
            break;
        case 64:
            S = &arm_cfft_sR_f32_len64;
            break;
        case 128:
            S = &arm_cfft_sR_f32_len128;
            break;
        case 256:
            S = &arm_cfft_sR_f32_len256;
            break;
        case 512:
            S = &arm_cfft_sR_f32_len512;
            break;
        case 1024:
            S = &arm_cfft_sR_f32_len1024;
            break;
        case 2048:
            S = &arm_cfft_sR_f32_len2048;
            break;
        case 4096:
            S = &arm_cfft_sR_f32_len4096;
            break;
        default:
            // 지원되지 않는 FFT 크기
            S = NULL;
            break;
    }
}

// FFT 입력 데이터 준비
void PrepareFFTInput(uint8_t* buffer) {
    for (int i = 0; i < FFT_SIZE; i++) {
        // 16비트 PCM 데이터를 부동소수점 실수부로 변환, 허수부는 0???로 설정
        int16_t sample = (buffer[i * 2 + 1] << 8) | buffer[i * 2];
        fftInput[2 * i] = (float32_t)sample;       // 실수부
        fftInput[2 * i + 1] = 0.0f;                // 허수부
    }
}

// FFT 수행 함수
void PerformFFT(void) {
    // FFT 수행
    arm_cfft_f32(S, fftInput, 0, 1);
    // 복소수 결과에서 크기 계산
    arm_cmplx_mag_f32(fftInput, fftOutput, FFT_SIZE);
}

// 스펙트럼 그리기 함수
void DrawSpectrum(void) {
    uint16_t height;
    uint8_t index = 0;
    uint16_t bin_size = FFT_SIZE / 16;    // 16개의 주파수 대역으로 나눔

    for (int i = 0; i < FFT_SIZE / 2; i += bin_size) {
        float32_t sum = 0.0f;
        for (int j = 0; j < bin_size; j++) {
            sum += fftOutput[i + j];
        }
        // 스펙트럼 크기 값을 이용하여 막대 높이 계산
        height = (uint16_t)(sum / bin_size / 10000);  // 스케일 조정
        if (height > MAX_HEIGHT)
            height = MAX_HEIGHT;

        // 화면에 막대 그래프 그리기
        DrawSpectrumBar(index, height);
        index++;
    }
}

// 스펙트럼 바를 그리는 함수
void DrawSpectrumBar(uint8_t index, uint16_t height) {
    uint16_t x = 20 + index * (BAR_WIDTH + BAR_GAP);
    uint16_t color;

    // 주파수 대역별 고정 색상 사용
    if (index < 5) {
        color = Blue;         // 저주파 - 파란색
    }
    else if (index < 11) {
        color = Magenta;      // 중주파 - 마젠타
    }
    else {
        color = Cyan;         // 고주파 - 시안
    }

    // 이전 막대 지우기
    Rectangle(x, START_Y - MAX_HEIGHT, x + BAR_WIDTH, START_Y, THEME_BG);

    // 새로운 막대 그리기
    Rectangle(x, START_Y - height, x + BAR_WIDTH, START_Y, color);
}

/*******************************************************************************
 * 기타 함수
 ******************************************************************************/

void TFT_filename(void) {
    unsigned char file_flag;
    unsigned short file_KB;

    TFT_string(0, 7, Cyan, Black, "----------------------------------------");
    TFT_string(3, 5, Green, Black, "                                     ");

    file_flag = Get_long_filename(file_number);   // 파일 이름 확인

    if (file_flag == 0)                           // 짧은 파일 이름 (8.3 형식)
        TFT_short_filename(3, 5, White, Black);
    else if (file_flag == 1)                      // 긴 파일 이름
        TFT_long_filename(3, 5, White, Black);
    else if (file_flag == 2)                      // 파일 이름이 195자 이상인 경우
        TFT_string(3, 5, Red, Black, "* 파일명 길이 초과 *");
    else                                          // 파일 이름 오류
        TFT_string(3, 5, Red, Black, "*** 파일명 오류 ***");

    file_KB = file_size[file_number] / 1024;      // 파일 크기(KB) 계산
    if ((file_size[file_number] % 1024) != 0)
        file_KB++;

    if (file_flag != 3) {
        TFT_xy(18, 9);                            // 파일 번호
        TFT_color(Magenta, Black);
        TFT_unsigned_decimal(file_number + 1, 1, 3);
        TFT_xy(17,11);                            // 파일 크기
        TFT_color(Magenta, Black);
        TFT_unsigned_decimal(file_KB, 0, 5);
    }

    TFT_string(27, 9, Yellow, Black, "000");      // 비트레이트 초기화

    TFT_xy(24, 13);                               // 재생 퍼센트 초기화
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

// 파일 확장자 체크 수정
void Check_valid_increment_file(void) {
    unsigned char file_OK_flag = 0;
    do {
        // WAV 파일 확장자 추가 (0x00574156)
        if ((extension != 0x004D5033) &&    // MP3
            (extension != 0x00564157) &&    // WAV (ASCII 'WAV' 역순)
            (extension != 0x00414143) &&    // AAC
            (extension != 0x00574D41) &&    // WMA
            (extension != 0x004D4944)) {    // MIDI
            if (file_number < total_file - 1)
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
        if ((extension != 0x004D5033) && (extension != 0x00564157) &&
            (extension != 0x00414143) && (extension != 0x00574D41) &&
            (extension != 0x004D4944)) {
            if (file_number > 0)
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