#include "stm32f767xx.h"
#include "OK-STM767.h"
#include "OK-STM767_SD_card.h"
#include "OK-STM767_VS1053b.h"

/*******************************************************************************
 * 상수 정의
 ******************************************************************************/
#define BAR_WIDTH       15 
#define BAR_GAP        4
#define START_Y        200
#define MAX_HEIGHT     150
#define INITIAL_VOLUME 180
#define BUFFER_SIZE    512
#define BUFFER_COUNT   2
#define UPDATE_PERIOD  20    /* 스펙트럼 업데이트 주기(ms) */

/*******************************************************************************
 * 전역 변수
 ******************************************************************************/
volatile uint32_t SysTick_Count = 0;
volatile uint8_t currentBuffer = 0;
static uint16_t prevHeight[16] = {0};
uint32_t MP3_start_sector[MAX_FILE];
uint32_t MP3_start_backup[MAX_FILE];
uint32_t MP3_end_sector;
uint8_t file_number = 0;
uint8_t total_file = 0;
uint8_t play_flag = 1;
uint8_t MP3buffer[BUFFER_COUNT][BUFFER_SIZE];

/*******************************************************************************
 * 함수 선언
 ******************************************************************************/
void InitSpectrum(void);
void SendMP3Data(uint8_t* buffer, uint16_t* index);
void PlayAndDrawSpectrum(void);
void UpdateSpectrum(void);
void DrawSpectrumBar(uint8_t index, uint16_t height);

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
 * VS1053B 관련 함수
 ******************************************************************************/
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
    VS1053b_SetVolume(INITIAL_VOLUME);    /* 볼륨 설정 */
    VS1053b_SetBassTreble(15, 7);       /* 음질 설정 */
    VS1053b_SCI_Write(0x07, 0x0020);    /* 주파수 응답 보정 */
    
    Delay_ms(10);
}

void SendMP3Data(uint8_t* buffer, uint16_t* index) {
    GPIOC->BSRR = 0x00400000;      /* -MP3_DCS = 0 */
    
    /* 32바이트 블록 단위로 전송 */
    for(uint8_t i = 0; i < 32; i++) {
        while((GPIOC->IDR & 0x0080) == 0);  /* DREQ 대기 */
        SPI3_write(buffer[(*index)++]);
    }
    
    GPIOC->BSRR = 0x00000040;      /* -MP3_DCS = 1 */
}

/*******************************************************************************
 * 스펙트럼 표시 함수
 ******************************************************************************/
void DrawSpectrumBar(uint8_t index, uint16_t height) {
    uint16_t x = 20 + index * (BAR_WIDTH + BAR_GAP);
    uint16_t color;
    
    /* 이전 막대 지우기 */
    Rectangle(x, START_Y - prevHeight[index],
             x + BAR_WIDTH, START_Y, Black);
    
    /* 주파수 대역별 색상 설정 */
    if(index < 5)        color = Blue;      /* 저주파 */
    else if(index < 11)  color = Magenta;   /* 중주파 */
    else                 color = Cyan;       /* 고주파 */
    
    /* 새 막대 그리기 */
    Rectangle(x, START_Y - height,
             x + BAR_WIDTH, START_Y, color);
             
    prevHeight[index] = height;
}

void UpdateSpectrum(void) {
    for(uint8_t i = 0; i < 16; i++) {
        /* 스펙트럼 데이터 읽기 */
        uint16_t value = VS1053b_SCI_Read(0x0C + i);
        
        /* 비선형 매핑 적용 */
        uint16_t height = (value * value * MAX_HEIGHT) / (255 * 255);
        if(height > MAX_HEIGHT) height = MAX_HEIGHT;
        
        /* 스펙트럼 막대 그리기 */
        DrawSpectrumBar(i, height);
    }
}

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
}

/*******************************************************************************
 * 메인 함수
 ******************************************************************************/
int main(void) {
    /* 초기화 */
    Initialize_MCU();
    Initialize_TFT_LCD();
    Initialize_SD();
    Initialize_FAT32();
    Initialize_VS1053b();
    SysTick_Initialize();
    
    /* 화면 초기화 */
    TFT_clear_screen();
    TFT_string(0,0,White,Magenta,"VS1053B Spectrum Analyzer");
    
    /* MP3 파일 설정 */
    total_file = fatGetDirEntry(FirstDirCluster);
    
    for(uint8_t i = 0; i < total_file; i++) {
        MP3_start_sector[i] = fatClustToSect(file_start_cluster[i]);
        MP3_start_backup[i] = MP3_start_sector[i];
    }
    
    file_number = 0;
    MP3_end_sector = (file_size[file_number] >> 9) + MP3_start_sector[file_number];
    
    InitSpectrum();
    
    /* 메인 루프 */
    while(1) {
        if(GPIOC->IDR & 0x0080) {  /* DREQ 체크 */
            PlayAndDrawSpectrum();
        }
    }
}