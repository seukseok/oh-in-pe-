#include "stm32f767xx.h"
#include "OK-STM767.h"
#include "OK-STM767_SD_card.h"
#include "OK-STM767_VS1053b.h"

/* 상수 정의 */
#define THEME_BG        Black
#define THEME_HEADER    0x861D
#define THEME_TEXT      0xBDF7
#define THEME_HIGHLIGHT 0x05FF
#define THEME_ACCENT    0xFD20
#define THEME_BUTTON    0x2DB7
#define THEME_WARNING   0xFBE0

#define BAR_WIDTH       15 
#define BAR_GAP         4
#define START_Y         220
#define MAX_HEIGHT      140
#define INITIAL_VOLUME  200
#define BUFFER_SIZE     512
#define BUFFER_COUNT    2
#define WAV_HEADER_SIZE 44

/* 타입 정의 */
typedef struct {
    uint32_t sampleRate;
    uint16_t numChannels;
    uint16_t bitsPerSample;
    uint32_t dataSize;
} WAV_Header;

/* 함수 선언 */
void Initialize_System(void);
void Initialize_Peripherals(void);
void Play_Audio(void);
void Update_Display(void);
void TFT_filename(void);
void Check_valid_increment_file(void);
void Check_valid_decrement_file(void);
unsigned char Icon_input(void);
uint8_t ParseWAVHeader(uint32_t sector);
void ConfigureVS1053ForWAV(void);
void SetupMainScreen(void);

/* 전역 변수 */
unsigned char total_file;
unsigned char file_number = 0;
unsigned short index = 512; 
volatile uint8_t currentBuffer = 0;
uint32_t WAV_start_sector[MAX_FILE];
uint32_t WAV_start_backup[MAX_FILE];
uint32_t WAV_end_sector;
uint8_t play_flag = 0;
uint8_t WAVbuffer[BUFFER_COUNT][BUFFER_SIZE];
uint32_t file_start[MAX_FILE];
uint32_t file_size[MAX_FILE];
uint16_t volume = INITIAL_VOLUME;
WAV_Header wavHeader;

/* 메인 함수 */
int main(void) {
    Initialize_System();
    Initialize_Peripherals();
    SetupMainScreen();

    while (1) {
        Play_Audio();
        Update_Display();
    }
}

/* 시스템 초기화 */
void Initialize_System(void) {
    Initialize_MCU();
    Delay_ms(50);
    Initialize_TFT_LCD();
    Initialize_touch_screen();
    Beep();
    Delay_ms(1000);
    TFT_clear_screen();
}

/* 주변장치 초기화 */
void Initialize_Peripherals(void) {
    Initialize_SD();
    Initialize_FAT32();
    Initialize_VS1053b();
    Delay_ms(1000);

    volume = 175;
    uint8_t bass = 10;
    uint8_t treble = 5;

    VS1053b_SetVolume(volume);
    Delay_ms(1);
    VS1053b_SetBassTreble(bass, treble);

    total_file = fatGetDirEntry(FirstDirCluster);

    for (unsigned char i = 0; i < total_file; i++) {
        WAV_start_sector[i] = fatClustToSect(file_start_cluster[i]);
        WAV_start_backup[i] = WAV_start_sector[i];
    }

    file_number = 0;
}

/* 메인 화면 설정 */
void SetupMainScreen(void) {
    TFT_clear_screen();
    TFT_string(0, 5, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(0, 7, THEME_HIGHLIGHT, THEME_BG, ">>");
    TFT_string(0, 9, THEME_TEXT, THEME_BG, "----------------------------------------");

    TFT_filename();
    Check_valid_increment_file();

    WAV_end_sector = (file_size[file_number] >> 9) + WAV_start_sector[file_number];
}

/* 오디오 재생 */
void Play_Audio(void) {
    static unsigned short index = 512;
    static unsigned char i;

    if (((GPIOC->IDR & 0x0080) == 0x0080) && (play_flag == 1)) {
        if (index == 512) {
            if (WAV_end_sector == WAV_start_sector[file_number]) {
                if (file_number != (total_file - 1))
                    file_number++;
                else
                    file_number = 0;

                TFT_filename();
                Check_valid_increment_file();

                WAV_start_sector[file_number] = WAV_start_backup[file_number];
                WAV_end_sector = (file_size[file_number] >> 9) + WAV_start_sector[file_number];
                VS1053b_software_reset();
            }
            index = 0;
            SD_read_sector(WAV_start_sector[file_number]++, WAVbuffer[currentBuffer]);
        }

        for (i = 0; i < 32; i++) {
            GPIOC->BSRR = 0x00400000;
            SPI3_write(WAVbuffer[currentBuffer][index++]);
            GPIOC->BSRR = 0x00000040;
        }
    }
}

/* 디스플레이 업데이트 */
void Update_Display(void) {
    static unsigned short loop = 0;
    unsigned short HDAT1, HDAT0;
    unsigned char key = Key_input();

    loop++;
    if ((loop == 500) && (play_flag == 1)) {
        loop = 0;
    }

    switch (key) {
        case KEY1:
            play_flag ^= 0x01;
            break;
        case KEY3:
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
            break;
        default:
            break;
    }
}

/* 파일 이름 표시 */
void TFT_filename(void) {
    unsigned char file_flag;

    TFT_string(0, 9, THEME_TEXT, THEME_BG, "----------------------------------------");
    TFT_string(3, 7, THEME_TEXT, THEME_BG, "                                     ");

    file_flag = Get_long_filename(file_number);

    if (file_flag == 0)
        TFT_short_filename(3, 7, White, Black);
    else if (file_flag == 1)
        TFT_long_filename(3, 7, White, Black);
    else if (file_flag == 2)
        TFT_string(3, 5, Red, Black, "* 파일 이름이 너무 깁니다 *");
    else
        TFT_string(3, 5, Red, Black, "*** 파일 이름 오류 ***");
}

/* 유효한 다음 파일 검사 */
void Check_valid_increment_file(void) {
    unsigned char file_OK_flag = 0;
    do {
        if ((extension != 0x004D5033) &&
            (extension != 0x00574156) &&
            (extension != 0x00414143) &&
            (extension != 0x00574D41) &&
            (extension != 0x004D4944)) {
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

/* WAV 헤더 파싱 */
uint8_t ParseWAVHeader(uint32_t sector) {
    uint8_t header[WAV_HEADER_SIZE];
    SD_read_sector(sector, header);

    if (header[0] != 'R' || header[1] != 'I' || header[2] != 'F' || header[3] != 'F' ||
        header[8] != 'W' || header[9] != 'A' || header[10] != 'V' || header[11] != 'E') {
        return 0;
    }

    wavHeader.sampleRate = *(uint32_t*)&header[24];
    wavHeader.numChannels = *(uint16_t*)&header[22];
    wavHeader.bitsPerSample = *(uint16_t*)&header[34];
    wavHeader.dataSize = *(uint32_t*)&header[40];

    return 1;
}

/* VS1053B WAV 모드 설정 */
void ConfigureVS1053ForWAV(void) {
    VS1053b_software_reset();
    Delay_ms(10);

    VS1053b_SCI_Write(0x00, 0x0820);
    Delay_ms(1);

    VS1053b_SCI_Write(0x05, 0xAC45);

    VS1053b_SetVolume(volume);
    VS1053b_SetBassTreble(8, 3);
}