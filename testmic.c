#include "stm32f767xx.h"
#include "OK-STM767.h"
#include <arm_math.h>  // CMSIS DSP 라이브러리 추가
#include <stdbool.h>   // bool 타입을 사용하기 위해 추가
#include "arm_const_structs.h"    // FFT 구조체 인스턴스를 위해 추가

#define SAMPLES 256       // 샘플 버퍼 크기
#define DC_OFFSET 2048   // DAC 출력의 DC offset (12비트 DAC의 중간값)
#define GAIN 2.0f       // 소리 신호 증폭 계수
#define FFT_SIZE 256         // FFT 크기
#define FFT_DISPLAY_HEIGHT 120  // FFT 막대 최대 높이
#define FFT_BAR_WIDTH 4      // FFT 막대 너비
#define FFT_BAR_GAP 1        // FFT 막대 간격

float sound_samples[SAMPLES];  // 사운드 샘플 버퍼
uint32_t sample_index = 0;    // 현재 샘플 인덱스

// TFT-LCD 표시를 위한 변수들
uint16_t adc_display_value = 0;
uint16_t dac_display_value = 0;

// FFT 관련 변수
const arm_cfft_instance_f32* S;   // FFT 구조체 포인터로 변경
float32_t fftBuffer[FFT_SIZE*2];  // 실수부와 허수부를 위해 2배 크기
float32_t fftResult[FFT_SIZE];    // FFT 결과 저장
uint32_t fftIndex = 0;
bool fftMode = false;

void ADC_Config(void) {
    RCC->APB2ENR |= 0x00000100;        // ADC1 클럭 활성화
    GPIOA->MODER |= 0x00003000;        // PA6를 아날로그 모드로 설정
    
    // 레지스터 초기화 추가
    ADC1->CR1 = 0;
    ADC1->CR2 = 0;
    
    ADC1->CR1 = 0x00000000;            // 12비트 해상도
    ADC1->CR2 = 0x00000001;            // ADC 활성화
    
    // ADC 채널 6번에 대한 샘플링 시간을 더 짧게 조정
    ADC1->SMPR2 = 0x00000000;  // 3 사이클로 변경 (가장 빠른 변환)
    ADC1->SQR1 = 0x00000000;           
    ADC1->SQR3 = 0x00000006;  

    // 연속 변환 모드 활성화
    ADC1->CR2 |= 0x00000002;   // CONT 비트 설정

    // ADC 시작
    ADC1->CR2 |= 0x40000000;   // SWSTART 비트 설정         
}

void DAC_Config(void) {
    RCC->APB1ENR |= 0x20000000;        // DAC 클럭 활성화
    GPIOA->MODER |= 0x00000C00;        // PA4를 아날로그 모드로 설정
    
    DAC->CR = 0x00000001;              // DAC 채널 1 활성화, 버퍼 활성화
}

void SysTick_Init(void) {
    NVIC_SetPriority(SysTick_IRQn, 0);
    // 44.1kHz 샘플링으로 변경
    SysTick->LOAD = (216000000/44100) - 1;  // 44.1kHz 샘플링
    SysTick->VAL = 0;
    SysTick->CTRL = 0x07;
}

uint16_t Read_ADC(void) {
    ADC1->CR2 |= 0x40000000;           // 변환 시작
    while(!(ADC1->SR & 0x02));         // 변환 완료 대기
    return ADC1->DR;                    // 변환 결과 반환
}

void Write_DAC(uint16_t value) {
    DAC->DHR12R1 = value;              // DAC 출력 설정
}

void Display_Init(void) {
    Initialize_TFT_LCD();              // TFT-LCD 초기화
    TFT_clear_screen();
    
    // 화면 타이틀
    TFT_string(0, 0, White, Magenta, "Sound Analyzer");
    
    // ADC/DAC 정보 표시 영역
    TFT_string(0, 2, Yellow, Black, "ADC Value: ");
    TFT_string(0, 4, Cyan, Black, "DAC Value: ");
    TFT_string(0, 6, Green, Black, "Gain: ");
    TFT_string(0, 8, White, Black, "Sample Rate: 44.1kHz");
}

// 디버그 정보 표시 함수 추가
void Display_Debug_Info(void) {
    char str[32];
    
    // DC 오프셋 표시
    sprintf(str, "DC: %4d", (adc_display_value - DC_OFFSET));
    TFT_string(0, 10, Red, Black, str);
    
    // 신호 진폭 표시
    static uint16_t min_value = 4095;
    static uint16_t max_value = 0;
    
    if(adc_display_value < min_value) min_value = adc_display_value;
    if(adc_display_value > max_value) max_value = adc_display_value;
    
    sprintf(str, "Min: %4d Max: %4d", min_value, max_value);
    TFT_string(0, 12, Green, Black, str);
}

void Update_Display(void) {
    char str[20];
    
    // ADC 값 표시
    sprintf(str, "%4d", adc_display_value);
    TFT_string(12, 2, White, Black, str);
    
    // DAC 값 표시  
    sprintf(str, "%4d", dac_display_value);
    TFT_string(12, 4, White, Black, str);
    
    // 게인 값 표시
    sprintf(str, "%.1f", GAIN);
    TFT_string(8, 6, White, Black, str);

    Display_Debug_Info();
}

// FFT 초기화 함수 수정
void FFT_Init(void) {
    // FFT 크기에 따라 적절한 구조체 인스턴스 선택
    switch (FFT_SIZE) {
        case 256:
            S = &arm_cfft_sR_f32_len256;  // 256-point FFT 구조체 사용
            break;
        // 다른 크기의 FFT가 필요한 경우 추가 가능
        default:
            // 기본값으로 256-point FFT 사용
            S = &arm_cfft_sR_f32_len256;
            break;
    }
}

// FFT 표시 함수 추가
void DrawFFT(void) {
    uint16_t barX = 0;
    uint16_t displayWidth = 240;  // TFT-LCD 가로 크기
    
    // 화면 지우기
    Rectangle(0, 120, displayWidth, 240, Black);
    
    // FFT 결과값 필터링 및 스무딩 추가
    static float prevMagnitudes[FFT_SIZE/4] = {0};  // 이전 값 저장용 버퍼
    const float alpha = 0.3f;  // 스무딩 계수 (0.0 ~ 1.0)
    
    for(int i = 0; i < FFT_SIZE/4; i++) {
        // 진폭 계산 및 정규화
        float magnitude = sqrtf(fftBuffer[2*i] * fftBuffer[2*i] + 
                              fftBuffer[2*i+1] * fftBuffer[2*i+1]);
                              
        // 노이즈 필터링 (임계값 이하는 제거)
        const float threshold = 0.01f;
        if(magnitude < threshold) magnitude = 0;
        
        // 지수 이동 평균 필터 적용
        magnitude = alpha * magnitude + (1.0f - alpha) * prevMagnitudes[i];
        prevMagnitudes[i] = magnitude;
        
        // 로그 스케일 변환 및 스케일링
        magnitude = log10f(magnitude + 1) * 30;  // 스케일 팩터 조정
        
        // 높이 계산 및 제한
        uint16_t height = (uint16_t)magnitude;
        if(height > FFT_DISPLAY_HEIGHT) height = FFT_DISPLAY_HEIGHT;
        
        // 막대 그리기 (더 뚜렷한 색상으로)
        uint16_t color;
        if(i < FFT_SIZE/12)      color = Red;     // 저주파
        else if(i < FFT_SIZE/6)  color = Green;   // 중주파
        else                     color = Blue;     // 고주파
        
        Rectangle(barX, 240-height, barX+FFT_BAR_WIDTH-FFT_BAR_GAP, 240, color);
        barX += FFT_BAR_WIDTH;
    }
}

// SysTick_Handler 수정
void SysTick_Handler(void) {
    uint16_t adc_value = Read_ADC();
    adc_display_value = adc_value;
    
    // 윈도우 함수 적용 (Hanning window)
    float window = 0.5f * (1.0f - cosf(2.0f * PI * fftIndex / FFT_SIZE));
    float32_t sample = (float32_t)(adc_value - DC_OFFSET) / 4096.0f;
    sample *= window;
    
    fftBuffer[2*fftIndex] = sample;
    fftBuffer[2*fftIndex+1] = 0.0f;
    
    fftIndex++;
    if(fftIndex >= FFT_SIZE) {
        arm_cfft_f32(S, fftBuffer, 0, 1);
        DrawFFT();
        fftIndex = 0;
    }
    
    Update_Display();
}
// main 함수 수정
int main(void) {
    Initialize_MCU();
    ADC_Config();
    DAC_Config(); 
    Display_Init();
    FFT_Init();      
    SysTick_Init(); 
    
    // 초기에 FFT 모드로 시작
    fftMode = true;
    TFT_string(0, 14, White, Blue, "FFT Mode ON ");
    Rectangle(0, 120, 240, 240, Black);  // FFT 표시 영역 초기화
    
    while(1) {
        Update_Display();
        Delay_ms(50);
    }
}