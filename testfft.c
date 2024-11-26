#include "stm32f767xx.h"
#include "OK-STM767.h"
#include "OK-STM767_SD_card.h"
#include "OK-STM767_VS1053b.h"

/*******************************************************************************
 * 상수 정의
 ******************************************************************************/
#define BAR_WIDTH       15 
#define BAR_GAP        4
#define START_Y        220
#define MAX_HEIGHT     140
#define INITIAL_VOLUME 180
#define BUFFER_SIZE    512
#define BUFFER_COUNT   2
#define UPDATE_PERIOD  20    /* 스펙트럼 업데이트 주기(ms) */

// 디버그 정보 표시를 위한 상수 추가
#define DEBUG_MODE 1
#define FREQ_BANDS 16

// 주파수 대역 정보 (Hz)
const char* freqLabels[FREQ_BANDS] = {
    "20-40", "40-80", "80-160", "160-320", "320-640",
    "640-1.2k", "1.2k-2.5k", "2.5k-5k", "5k-10k", "10k-12k",
    "12k-14k", "14k-16k", "16k-17k", "17k-18k", "18k-19k", "19k-20k"
};

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
void DebugSpectrum(void);

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
    VS1053b_SetBassTreble(10, 3);       /* 음질 설정 */
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

void UpdateSpectrum(void) {
    // 주파수 대역별 감도 조정
    static const float bands[16] = {
        0.45f, 0.45f, 0.45f, 0.45f, 0.45f,    // 저주파 (균일한 감도)
        0.40f, 0.40f, 0.40f, 0.40f, 0.40f,    // 중주파
        0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f  // 고주파
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
        static uint16_t prevHeight = 0;
        if(abs(height - prevHeight) < (MAX_HEIGHT * 0.1f)) {
            height = (height + prevHeight * 3) / 4; // 부드러운 변화
        }
        prevHeight = height;
        
        // 최종 높이 제한
        if(height > MAX_HEIGHT) height = MAX_HEIGHT;
        
        DrawSpectrumBar(i, height);
    }
    
    filterIndex = (filterIndex + 1) & 0x03; // 순환 버퍼 인덱스 업데이트
    
    #if DEBUG_MODE
    DebugSpectrum();  // 디버그 정보 표시
    #endif
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
    
    /* 디버그 정보 업데이트 */
    if (DEBUG_MODE) {
        DebugSpectrum();
    }
}

/*******************************************************************************
 * 스펙트럼 데이터 검증용 함수
 ******************************************************************************/
void DebugSpectrum(void) {
    static uint32_t lastDebugTime = 0;
    
    if(SysTick_Count - lastDebugTime >= 1000) { // 1초마다 업데이트
        lastDebugTime = SysTick_Count;
        
        TFT_xy(0, 2);  // 디버그 정보 표시 위치
        
        for(uint8_t i = 0; i < FREQ_BANDS; i++) {
            uint16_t raw_value = VS1053b_SCI_Read(0x0C + i);
            char debugStr[32];
            sprintf(debugStr, "B%02d:%4d Hz:%s", i, raw_value, freqLabels[i]);
            TFT_string(0, 2+i, White, Black, debugStr);
        }
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