#include "stm32f767xx.h"
#include "OK-STM767.h"
#include "OK-STM767_SD_card.h"
#include "OK-STM767_VS1053b.h"

// 상수 정의
#define BAR_WIDTH 15 
#define BAR_GAP 4
#define START_Y 200
#define MAX_HEIGHT 150
#define INITIAL_VOLUME 180  // 음량 증가 (0-255)

// 전역 변수
static uint16_t prevHeight[16] = {0};
uint32_t MP3_start_sector[MAX_FILE];
uint32_t MP3_start_backup[MAX_FILE];
uint32_t MP3_end_sector;
uint8_t file_number = 0;
uint8_t total_file = 0;
uint8_t play_flag = 1;

// VS1053B 스펙트럼 분석기 초기화 
void InitSpectrum(void) {
    VS1053b_software_reset();
    Delay_ms(10);
    
    // VS1053B 기본 설정
    VS1053b_SetVolume(INITIAL_VOLUME);  // 음량 증가
    VS1053b_SetBassTreble(10, 5);       // 베이스와 트레블 증가
    
    // 디코더 설정
    VS1053b_SCI_Write(0x03, 0xC000);    // 클록 설정 
    VS1053b_SCI_Write(0x00, 0x0820);    // 스펙트럼 분석기 모드
    VS1053b_SCI_Write(0x0B, 0x2020);    // 좌/우 채널 볼륨 설정
    VS1053b_SCI_Write(0x05, 0xAC45);    // 48kHz, 스테레오
    Delay_ms(10);
}

// MP3 데이터 재생 및 스펙트럼 표시
void PlayAndDrawSpectrum(void) {
    static uint16_t index = 512;
    uint8_t MP3buffer[512];
    uint16_t i, x, height;
    
    if((GPIOC->IDR & 0x0080) && play_flag) {  // DREQ = High이고 재생 중일 때
        if(index >= 512) {
            // 현재 파일의 끝에 도달했는지 확인
            if(MP3_end_sector == MP3_start_sector[file_number]) {
                file_number = (file_number + 1) % total_file;
                MP3_start_sector[file_number] = MP3_start_backup[file_number];
                MP3_end_sector = (file_size[file_number] >> 9) + MP3_start_sector[file_number];
                VS1053b_software_reset();
                InitSpectrum();  // 새 파일 시작시 초기화
            }
            SD_read_sector(MP3_start_sector[file_number]++, MP3buffer);
            index = 0;
        }
        
        // 32바이트씩 VS1053B로 전송
        for(i = 0; i < 32; i++) {
            while((GPIOC->IDR & 0x0080) == 0);  // DREQ 대기
            GPIOC->BSRR = 0x00400000;  // -MP3_DCS = 0
            SPI3_write(MP3buffer[index++]);
            GPIOC->BSRR = 0x00000040;  // -MP3_DCS = 1
        }
        
        // 스펙트럼 데이터 읽기 및 표시 
        if((index % 128) == 0) {  // 업데이트 주기 조절
            for(i = 0; i < 16; i++) {
                uint16_t value = VS1053b_SCI_Read(0x0C + i);
                height = (value * MAX_HEIGHT) / 255;
                if(height > MAX_HEIGHT) height = MAX_HEIGHT;
                
                x = 20 + i * (BAR_WIDTH + BAR_GAP);
                
                // 이전 막대 지우기
                Rectangle(x, START_Y - prevHeight[i],
                         x + BAR_WIDTH, START_Y, Black);
                
                // 새 막대 그리기
                if(i < 5)      
                    Rectangle(x, START_Y - height,
                             x + BAR_WIDTH, START_Y, Blue);
                else if(i < 11) 
                    Rectangle(x, START_Y - height,
                             x + BAR_WIDTH, START_Y, Magenta);
                else           
                    Rectangle(x, START_Y - height,
                             x + BAR_WIDTH, START_Y, Cyan);
                             
                prevHeight[i] = height;
            }
        }
    }
}

int main(void) {
    // 초기화
    Initialize_MCU();
    Initialize_TFT_LCD();
    Initialize_SD();
    Initialize_FAT32();
    Initialize_VS1053b();
    
    // 화면 초기화
    TFT_clear_screen();
    TFT_string(0,0,White,Magenta,"VS1053B Spectrum Analyzer");
    
    // MP3 파일 정보 읽기
    total_file = fatGetDirEntry(FirstDirCluster);
    
    // MP3 파일 시작 섹터 저장
    for(uint8_t i = 0; i < total_file; i++) {
        MP3_start_sector[i] = fatClustToSect(file_start_cluster[i]);
        MP3_start_backup[i] = MP3_start_sector[i];
    }
    
    // 첫 번째 파일 준비
    file_number = 0;
    MP3_end_sector = (file_size[file_number] >> 9) + MP3_start_sector[file_number];
    
    InitSpectrum();
    
    while(1) {
        PlayAndDrawSpectrum();
        Delay_ms(1);
    }
}