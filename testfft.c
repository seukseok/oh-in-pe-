#include "stm32f767xx.h"
#include "OK-STM767.h"
#include "OK-STM767_SD_card.h"
#include "OK-STM767_VS1053b.h"

// 기존 정의 유지
#define BAR_WIDTH 15 
#define BAR_GAP 4
#define START_Y 200
#define MAX_HEIGHT 150

static uint16_t prevHeight[16] = {0};
uint32_t MP3_start_sector[100];
uint32_t file_number = 0;
uint32_t total_file = 0;

// VS1053B 스펙트럼 분석기 초기화
void InitSpectrum(void) {
    VS1053b_software_reset();
    
    // VS1053B 기본 설정
    VS1053b_SetVolume(40);
    VS1053b_SetBassTreble(8, 3);  // 적절한 음질 설정
    
    // 디코더 설정
    VS1053b_SCI_Write(0x03, 0xC000);  // 클록 설정
    VS1053b_SCI_Write(0x00, 0x0820);  // 기본 모드 + 스펙트럼 분석기
}

// MP3 데이터 재생 및 스펙트럼 표시
void PlayAndDrawSpectrum(void) {
    uint16_t i, x, height;
    static uint32_t index = 512;
    uint8_t MP3buffer[512];
    
    // DREQ 신호 확인 (데이터 요청 대기)
    if((GPIOC->IDR & 0x0080)) {  // DREQ = High일 때
        if(index >= 512) {
            SD_read_sector(MP3_start_sector[file_number]++, MP3buffer);
            index = 0;
        }
        
        // 32바이트씩 VS1053B로 전송
        GPIOC->BSRR = 0x00400000;  // -MP3_DCS = 0
        for(i = 0; i < 32; i++) {
            while((GPIOC->IDR & 0x0080) == 0);  // DREQ 대기
            SPI3_write(MP3buffer[index++]);
        }
        GPIOC->BSRR = 0x00000040;  // -MP3_DCS = 1
        
        // 스펙트럼 데이터 읽기
        for(i = 0; i < 16; i++) {
            uint16_t value = VS1053b_SCI_Read(0x0C + i);
            
            // 스케일링 개선
            height = (value * MAX_HEIGHT) / 255;
            if(height > MAX_HEIGHT) height = MAX_HEIGHT;
            
            x = 20 + i * (BAR_WIDTH + BAR_GAP);
            
            // 이전 막대 지우기
            Rectangle(x, START_Y - prevHeight[i],
                     x + BAR_WIDTH, START_Y, Black);
            
            // 주파수 대역별 색상
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

int main(void) {
    Initialize_MCU();
    Initialize_TFT_LCD();
    Initialize_SD();
    Initialize_FAT32();
    Initialize_VS1053b();
    
    TFT_clear_screen();
    TFT_string(0,0,White,Magenta,"VS1053B Spectrum Analyzer");
    
    // MP3 파일 정보 읽기
    total_file = fatGetDirEntry(FirstDirCluster);
    MP3_start_sector[0] = fatClustToSect(file_start_cluster[0]);
    file_number = 0;
    
    InitSpectrum();
    
    while(1) {
        PlayAndDrawSpectrum();
        if((VS1053b_SCI_Read(0x09) == 0) && 
           (VS1053b_SCI_Read(0x08) == 0)) {
            // 현재 파일 재생 완료
            file_number++;
            if(file_number >= total_file) file_number = 0;
            MP3_start_sector[file_number] = 
                fatClustToSect(file_start_cluster[file_number]);
            VS1053b_software_reset();
        }
    }
}