#include "stm32f767xx.h"            // STM32F767 마이크로컨트롤러 헤더 파일 포함
#include "OK-STM767.h"              // OK-STM767 보드 관련 헤더 파일 포함
#include <stdlib.h>                 // 표준 라이브러리 함수 사용을 위한 헤더 파일 포함
#include <arm_math.h>               // ARM의 수학 라이브러리 함수 사용을 위한 헤더 파일 포함
#include "fft.h"                    // FFT 함수 헤더 파일 포함
#include "complex.h"                // 복소수 연산을 위한 헤더 파일 포함

#define SAMPLE_SIZE 2048            // 샘플 데이터의 크기를 2048로 정의
#define FFT_SIZE (SAMPLE_SIZE/2)    // FFT 크기를 샘플 크기의 절반으로 정의

complex_f fft_buffer_pool[SAMPLE_SIZE];   // FFT에 사용할 복소수 버퍼 풀 선언

void Display_FFT_screen(void);            // FFT 화면을 표시하는 함수 선언
void do_fft(void);                        // FFT 연산을 수행하는 함수 선언
void Draw_FFT(U16 index, float value);    // FFT 결과를 바 형태로 그리는 함수 선언

unsigned char FFT_mode, FFT_flag;         // FFT 모드와 플래그를 위한 변수 선언
unsigned short FFT_count;                 // FFT 샘플 카운트를 위한 변수 선언

uint16_t ADC_buffer[SAMPLE_SIZE];            // ADC로부터 수집한 샘플 데이터를 저장하는 버퍼
float FFT_buffer[SAMPLE_SIZE];            // FFT 입력 데이터를 저장하는 버퍼
float FFT_output[SAMPLE_SIZE/2];          // FFT 결과를 저장하는 버퍼
float max_value;                          // FFT 결과 중 최대값을 저장하는 변수
unsigned int max_index;                   // 최대값의 인덱스를 저장하는 변수

/* ----- DMA 완료 인터럽트 핸들러 -------------------------------------------------- */

void DMA2_Stream0_IRQHandler(void) {
    if(DMA2->LISR & DMA_LISR_TCIF0) {
        DMA2->LIFCR |= DMA_LIFCR_CTCIF0;
        
        // ADC 데이터를 float로 변환하고 정규화
        for(int i = 0; i < SAMPLE_SIZE; i++) {
            FFT_buffer[i] = ((float)(ADC_buffer[i] & 0x0FFF) - 2048.0f)/2048.0f;
        }
        FFT_flag = 1;
    }
}

/* ----- 메인 함수 --------------------------------------------------------------- */

int main(void) {
    Initialize_MCU();
    Delay_ms(50);
    Initialize_LCD();
    Initialize_TFT_LCD();

    // GPIO 설정
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;  // GPIOA 클럭 활성화
    GPIOA->MODER |= 0x00003000;            // PA6를 아날로그 모드로 설정

    // ADC 설정
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;    // ADC1 클럭 활성화
    ADC->CCR = 0x00000000;                  // ADCCLK = 54MHz/2 = 27MHz
    
    // ADC 샘플링 시간 설정 - 더 빠른 샘플링을 위해 수정
    ADC1->SMPR2 = 0x00000000;              // 채널 6 샘플링 시간 = 3 사이클
    
    // ADC 설정
    ADC1->CR1 = 0x00000000;                // 12비트 해상도
    ADC1->CR2 = ADC_CR2_ADON |             // ADC 활성화
                ADC_CR2_CONT |              // 연속 변환 모드
                ADC_CR2_DMA |               // DMA 모드
                ADC_CR2_DDS;                // DMA 요청 활성화
    
    ADC1->SQR1 = 0x00000000;               // 변환 채널 수 = 1
    ADC1->SQR3 = 0x00000006;               // 채널 6 선택

    // DMA 설정
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;    // DMA2 클럭 활성화
    
    // DMA 스트림 비활성화
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    while(DMA2_Stream0->CR & DMA_SxCR_EN);
    
    // DMA 설정 초기화
    DMA2->LIFCR = 0x0F400000;              // 플래그 클리어
    
    // DMA 스트림 설정
    DMA2_Stream0->PAR = (uint32_t)&ADC1->DR;
    DMA2_Stream0->M0AR = (uint32_t)ADC_buffer;
    DMA2_Stream0->NDTR = SAMPLE_SIZE;
    
    DMA2_Stream0->CR = 0;
    DMA2_Stream0->CR |= (0 << 25);          // 채널 0
    DMA2_Stream0->CR |= DMA_SxCR_PL_1;      // 우선순위 높음
    DMA2_Stream0->CR |= DMA_SxCR_MSIZE_0;   // 메모리 16비트
    DMA2_Stream0->CR |= DMA_SxCR_PSIZE_0;   // 주변장치 16비트
    DMA2_Stream0->CR |= DMA_SxCR_MINC;      // 메모리 증가
    DMA2_Stream0->CR |= DMA_SxCR_CIRC;      // 순환 모드
    DMA2_Stream0->CR |= DMA_SxCR_TCIE;      // 전송 완료 인터럽트

    // DMA 인터럽트 설정
    NVIC_SetPriority(DMA2_Stream0_IRQn, 0);
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);

    // DMA 활성화
    DMA2_Stream0->CR |= DMA_SxCR_EN;

    Display_FFT_screen();
    TFT_string(5,0,White,Magenta," 실시간 FFT 변환 및 표시 ");

    // ADC 변환 시작
    ADC1->CR2 |= ADC_CR2_SWSTART;

    while(1) {
        if(FFT_flag) {
            FFT_flag = 0;
            do_fft();
        }
        Delay_us(10);  // 딜레이 값 조정
    }
}
/* ----- FFT 연산 함수 ---------------------------------------------------------- */

void do_fft(void)
{
  complex_f *fft_data = fft_buffer_pool;  // FFT에 사용할 버퍼 포인터 설정

  unsigned r = log2(SAMPLE_SIZE);         // 샘플 크기의 로그2 값 계산
  unsigned N = 1 << r;                    // FFT 크기 N = 2^r 계산
  float max_data = 0.0;                   // 최대값 변수 초기화

  for(int i = 0; i < SAMPLE_SIZE; i++)
  {
    fft_data[i].re = FFT_buffer[i];       // 실수부에 FFT 입력 데이터 저장
    fft_data[i].im = 0.0;                 // 허수부를 0으로 초기화
  }

  ffti_f(fft_data, r, FFT_FORWARD);       // FFT 수행
  for(int i = 1; i < 128; i++)            // 특정 범위의 스펙트럼 사용
  {
    FFT_output[i-1] = sqrt(pow(fft_data[i].re,2)+pow(fft_data[i].im,2)); // 스펙트럼의 크기 계산
    if(max_data < FFT_output[i-1])
      max_data = FFT_output[i-1];         // 최대값 업데이트
  }
  
  for(int i = 0; i < 128; i++)
  {
    Draw_FFT(i, FFT_output[i] * 180 / max_data); // FFT 결과를 바 형태로 표시
  }
}

/* ----- FFT 화면 표시 함수 ----------------------------------------------------- */

void Display_FFT_screen(void)             /* FFT 화면 표시 함수 */
{
  unsigned short x, y;

  TFT_clear_screen();                     // 화면 초기화

  TFT_color(White,Black);
  TFT_English_pixel(18,213, '0');         // y축 레이블 표시 (0%)
  TFT_English_pixel(10,195, '1');         // y축 레이블 표시 (10%)
  TFT_English_pixel(18,195, '0');
  TFT_English_pixel(10,177, '2');         // y축 레이블 표시 (20%)
  TFT_English_pixel(18,177, '0');
  TFT_English_pixel(10,159, '3');         // y축 레이블 표시 (30%)
  TFT_English_pixel(18,159, '0');
  TFT_English_pixel(10,141, '4');         // y축 레이블 표시 (40%)
  TFT_English_pixel(18,141, '0');
  TFT_English_pixel(10,123, '5');         // y축 레이블 표시 (50%)
  TFT_English_pixel(18,123, '0');
  TFT_English_pixel(10,105, '6');         // y축 레이블 표시 (60%)
  TFT_English_pixel(18,105, '0');
  TFT_English_pixel(10, 87, '7');         // y축 레이블 표시 (70%)
  TFT_English_pixel(18, 87, '0');
  TFT_English_pixel(10, 69, '8');         // y축 레이블 표시 (80%)
  TFT_English_pixel(18, 69, '0');
  TFT_English_pixel(10, 51, '9');         // y축 레이블 표시 (90%)
  TFT_English_pixel(18, 51, '0');
  TFT_English_pixel(2,  33, '1');         // y축 레이블 표시 (100%)
  TFT_English_pixel(10, 33, '0');
  TFT_English_pixel(18, 33, '0');
  TFT_color(Magenta,Black);
  TFT_English_pixel(2,  16, '[');         // y축 단위 표시 ([%])
  TFT_English_pixel(10, 16, '%');
  TFT_English_pixel(18, 16, ']');

  TFT_color(White,Black);
  TFT_English_pixel( 26,222, '0');        // x축 레이블 표시 (0kHz)
  TFT_English_pixel( 46,222, '2');        // x축 레이블 표시 (2kHz)
  TFT_English_pixel( 66,222, '4');        // x축 레이블 표시 (4kHz)
  TFT_English_pixel( 86,222, '6');        // x축 레이블 표시 (6kHz)
  TFT_English_pixel(106,222, '8');        // x축 레이블 표시 (8kHz)
  TFT_English_pixel(118,222, '1');        // x축 레이블 표시 (10kHz)
  TFT_English_pixel(126,222, '0');
  TFT_English_pixel(138,222, '1');        // x축 레이블 표시 (12kHz)
  TFT_English_pixel(146,222, '2');
  TFT_English_pixel(158,222, '1');        // x축 레이블 표시 (14kHz)
  TFT_English_pixel(166,222, '4');
  TFT_English_pixel(178,222, '1');        // x축 레이블 표시 (16kHz)
  TFT_English_pixel(186,222, '6');
  TFT_English_pixel(199,222, '1');        // x축 레이블 표시 (18kHz)
  TFT_English_pixel(207,222, '8');
  TFT_English_pixel(220,222, '2');        // x축 레이블 표시 (20kHz)
  TFT_English_pixel(228,222, '0');
  TFT_English_pixel(241,222, '2');        // x축 레이블 표시 (22kHz)
  TFT_English_pixel(249,222, '2');
  TFT_English_pixel(262,222, '2');        // x축 레이블 표시 (24kHz)
  TFT_English_pixel(270,222, '4');

  TFT_color(Magenta,Black);
  TFT_English_pixel(280,223, '[');        // x축 단위 표시 ([kHz])
  TFT_English_pixel(288,223, 'k');
  TFT_English_pixel(296,223, 'H');
  TFT_English_pixel(304,223, 'z');
  TFT_English_pixel(312,223, ']');

  Line(30, 220, 310, 220, White);         // x축 선 그리기
  Line(305,215, 310, 220, White);         // x축 화살표 그리기
  Line(305,225, 310, 220, White);
  for(x = 50; x <= 309; x += 20)          // x축 눈금 그리기
    Line(x,218, x,222, White);

  Line(30,  28,  30, 220, White);         // y축 선 그리기
  Line(35,  33,  30,  28, White);         // y축 화살표 그리기
  Line(25,  33,  30,  28, White);
  for(y = 40; y <= 202; y += 18)          // y축 눈금 그리기
    Line(28,y, 32,y, White);
}

/* ----- FFT 결과 바 그래프 그리기 함수 ------------------------------------------ */

void Draw_FFT(U16 index, float value)     /* FFT 결과를 바 형태로 그리는 함수 */
{
  unsigned short height;

  height = (unsigned short)value;         // 바의 높이 계산
  if(height >= 180.) height = 180.;       // 최대 높이 제한

  Line(30+2*index, 219, 30+2*index, 219 - 180, Black);    // 이전 바 삭제
  Line(30+2*index, 219, 30+2*index, 219 - height, Red);   // 새로운 바 그리기
}