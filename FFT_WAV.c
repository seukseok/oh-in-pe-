#include "stm32f767xx.h"
#include "OK-STM767.h"
#include <stdlib.h>
#include <arm_math.h>				// 반드시 마지막에 인클루드할 것.
#include "fft.h"
#include "complex.h"
// 만약 보여줄 주파수를 좁히고 싶으면 ctrl+F -> 주파수 범위 설정 값
// #define SAMPLE_SIZE 512 // 주파수 범위 설정 값
#define SAMPLE_SIZE 2048
#define FFT_SIZE SAMPLE_SIZE/2

// complex_f *fft_data;
complex_f fft_buffer_pool[SAMPLE_SIZE];

void TIM7_IRQHandler(void);			/* TIM7 interrupt(25.6kHz) for ADC input */
void Display_FFT_screen(void);			/* display FFT screen */
void do_fft(void);
void Draw_FFT(U16 index, float value);		/* draw bar of FFT result */
void apply_hamming_window(float* buffer, int size);
void low_pass_filter(complex_f* fft_data, int size, int cutoff_index);

unsigned char FFT_mode, FFT_flag;
unsigned short FFT_count;

float ADC_buffer[SAMPLE_SIZE];
float FFT_buffer[SAMPLE_SIZE], FFT_input[SAMPLE_SIZE], FFT_output[SAMPLE_SIZE/2], max_value;
// 복소수, 크기(실수), inv fft를 위한 시간영역 버퍼
unsigned int max_index;

/* ----- 인터럽트 처리 프로그램 ----------------------------------------------- */

void TIM7_IRQHandler(void)			/* TIM7 interrupt(25.6kHz) for ADC input */
{
  TIM7->SR = 0x0000;				// clear pending bit of TIM7 interrupt

  ADC1->CR2 |= 0x40000000;			// start conversion by software
  while(!(ADC1->SR & 0x00000002));		// wait for end of conversion
  FFT_buffer[FFT_count] = ((float)ADC1->DR - 2048.)/2048.;
  // FFT_buffer[FFT_count + 1] = 0.;
  FFT_count++;
  
    if(FFT_count >= SAMPLE_SIZE)
    { 
      FFT_count = 0;
      FFT_flag = 1;
    }
}

/* ----- 메인 프로그램 -------------------------------------------------------- */

int main(void)
{
  Initialize_MCU();			
  Delay_ms(50);	
  Initialize_LCD();			
  Initialize_TFT_LCD();
  GPIOA->MODER |= 0x00003000;			// PA6 = analog mode(ADC12_IN6)
  RCC->APB2ENR |= 0x00000100;			// enable ADC1 clock
  ADC->CCR = 0x00000000;			// ADCCLK = 54MHz/2 = 27MHz
  ADC1->SMPR2 = 0x00040000;			// sampling time of channel 6 = 15 cycle
  ADC1->CR1 = 0x00000000;			// 12-bit resolution
  ADC1->CR2 = 0x00000001;			// right alignment, single conversion, ADON = 1
  ADC1->SQR1 = 0x00000000;			// total regular channel number = 1
  ADC1->SQR3 = 0x00000006;			// channel 6

  RCC->APB1ENR |= 0x00000020;			// enable TIM7 clock
  TIM7->PSC = 9;				// 108MHz/(9+1) = 10.8MHz
  TIM7->ARR = 421;				// 10.8MHz/(421+1) = 25.6kHz
  TIM7->CNT = 0;				// clear counter
  TIM7->DIER = 0x0001;				// enable update interrupt
  TIM7->CR1 = 0x0085;				// enable TIM7, update event, and preload
  NVIC->ISER[1] |= 0x00800000;			// enable (55)TIM7 interrupt

  GPIOA->MODER |= 0x00000F00;                         // PA5, PA4 = analog mode
  RCC->APB1ENR |= 0x20000000;                         // enable DAC clock
  DAC->CR = 0x00030003; // DAC channel 1~2 enable
  
  Display_FFT_screen();     
  TFT_string(5,0,White,Magenta," 오디오 FFT 분석 결과 ");

  while(1)
    {   

      if(FFT_flag == 1)
      {
        FFT_flag = 0;
        do_fft();
      }
       Delay_us(1);
      
  }

}
/* ----- 사용자 정의 함수 ----------------------------------------------------- */

void do_fft(void)
{
  // fft_data = (complex_f *)malloc(SAMPLE_SIZE* sizeof(complex_f));
  complex_f *fft_data = fft_buffer_pool;

  unsigned r = log2(SAMPLE_SIZE);
	unsigned N = 1 << r; // N = 512
	float max_data = 0.0;
  // char cstr[128];



  for(int i = 0; i < SAMPLE_SIZE; i++)
  {
    fft_data[i].re = FFT_buffer[i];
  
    fft_data[i].im = 0.0;
  }

  ffti_f(fft_data, r, FFT_FORWARD); // fft 진행
  // low_pass_filter(fft_data, SAMPLE_SIZE, FFT_SIZE / 4); // Filter to 1/4 of the frequency range

  // for(int i = 1; i < N; i++) // 주파수 범위 설정 값
  // int frequency_ranges[19] = {0, 31, 63, 94, 125, 187, 250, 375, 500, 750, 1000, 1500, 2000, 3000, 4000, 6000, 8000, 12000, 16000}; // 2048 개를 16khz로 분할 해야함
  int frequency_ranges[18] = {0, 8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 640, 768, 896, 960, 1024};
    int num_bars = 18;
    float averaged_values[18] = {0};
    int count[18] = {0};

    for (int i = 1; i < SAMPLE_SIZE/ 2; i++)
    {
        float magnitude = sqrt(pow(fft_data[i].re, 2) + pow(fft_data[i].im, 2));

        for (int j = 0; j < num_bars; j++)
        {
            if (i >= frequency_ranges[j] && i < frequency_ranges[j + 1])
            {
                averaged_values[j] += magnitude;
                count[j]++;
                break;
            }
        }

        if (max_data < magnitude)
        {
            max_data = magnitude;
        }
    }

    for (int i = 0; i < num_bars; i++)
    {
        if (count[i] != 0)
        {
            averaged_values[i] /= count[i];
        }
    }

    for (int i = 0; i < num_bars; i++)
    {
        Draw_FFT(i, averaged_values[i] * 180 / max_data);
    }
  ///////////////////////////////////////////
  //Debugging
  // sprintf(cstr,"N(%d),%.3f,%.3f,%.3f,%.3f,%.3f", 
  //   N,
  //   FFT_output[0],
  //   FFT_output[1],
  //   FFT_output[2],
  //   FFT_output[3],
  //   FFT_output[4]);
  // TFT_string(0,20,Yellow,Black,cstr);
  // sprintf(cstr,",%.3f,%.3f,%.3f,%.3f,%.3f", 
  //   FFT_output[5],
  //   FFT_output[6],
  //   FFT_output[2],
  //   FFT_output[8],
  //   FFT_output[9]);
  // TFT_string(0,22,Yellow,Black,cstr);
  //Debugging
  ///////////////////////////////////////////
  // free(fft_data); // 사용 안 함

 // ----- Perform Inverse FFT -----
  ffti_f(fft_data, r, FFT_INVERSE);


  for (int i = 0; i < SAMPLE_SIZE; i++)
  {
    fft_data[i].re /= SAMPLE_SIZE;
  }


  for (int i = 0; i < SAMPLE_SIZE; i++)
  {
    uint16_t dac_value = (uint16_t)((fft_data[i].re + 1.0f) * 2048.0f);
    DAC->DHR12R1 = dac_value; // DAC channel 1 (PA4)

    Delay_us(39); //샘플링 rate ~25.6 kHz
  }

}


void low_pass_filter(complex_f* fft_data, int size, int cutoff_index)
{
    for (int i = cutoff_index; i < size - cutoff_index; i++)
    {
        fft_data[i].re = 0.0;
        fft_data[i].im = 0.0;
    }
}

void Display_FFT_screen(void)			/* display FFT screen */
{
  unsigned short x, y;

  TFT_clear_screen();				// clear screen

  TFT_color(White,Black);
  TFT_English_pixel(18,213, '0');               // 0
  TFT_English_pixel(10,195, '1');               // 10
  TFT_English_pixel(18,195, '0');
  TFT_English_pixel(10,177, '2');               // 20
  TFT_English_pixel(18,177, '0');
  TFT_English_pixel(10,159, '3');               // 30
  TFT_English_pixel(18,159, '0');
  TFT_English_pixel(10,141, '4');               // 40
  TFT_English_pixel(18,141, '0');
  TFT_English_pixel(10,123, '5');               // 50
  TFT_English_pixel(18,123, '0');
  TFT_English_pixel(10,105, '6');               // 60
  TFT_English_pixel(18,105, '0');
  TFT_English_pixel(10, 87, '7');               // 70
  TFT_English_pixel(18, 87, '0');
  TFT_English_pixel(10, 69, '8');               // 80
  TFT_English_pixel(18, 69, '0');
  TFT_English_pixel(10, 51, '9');               // 90
  TFT_English_pixel(18, 51, '0');
  TFT_English_pixel(2,  33, '1');		// 100
  TFT_English_pixel(10, 33, '0');
  TFT_English_pixel(18, 33, '0');
  TFT_color(Magenta,Black);
  TFT_English_pixel(2,  16, '[');               // [%]
  TFT_English_pixel(10, 16, '%');
  TFT_English_pixel(18, 16, ']');

  TFT_color(White,Black);
  TFT_English_pixel( 26,222, '0');		// 0
  TFT_English_pixel( 46,222, '2');		// 2
  TFT_English_pixel( 66,222, '4');		// 4
  TFT_English_pixel( 86,222, '6');		// 6
  TFT_English_pixel(106,222, '8');		// 8
  TFT_English_pixel(118,222, '1');		// 1
  TFT_English_pixel(126,222, '0');		// 0
  TFT_English_pixel(138,222, '1');		// 1
  TFT_English_pixel(146,222, '2');		// 2
  TFT_English_pixel(158,222, '1');		// 1
  TFT_English_pixel(166,222, '4');		// 4
  TFT_English_pixel(178,222, '1');		// 1
  TFT_English_pixel(186,222, '6');		// 6
  TFT_English_pixel(199,222, '1');		// 1
  TFT_English_pixel(207,222, '8');		// 8
  TFT_English_pixel(220,222, '2');		// 2
  TFT_English_pixel(228,222, '0');		// 0
  TFT_English_pixel(241,222, '2');		// 2
  TFT_English_pixel(249,222, '2');		// 2
  TFT_English_pixel(262,222, '2');		// 2
  TFT_English_pixel(270,222, '4');		// 4

  TFT_color(Magenta,Black);
  TFT_English_pixel(280,223, '[');		// [kHz]
  TFT_English_pixel(288,223, 'k');
  TFT_English_pixel(296,223, 'H');
  TFT_English_pixel(304,223, 'z');
  TFT_English_pixel(312,223, ']');

  Line(30, 220, 310, 220, White);		// x-axis line
  Line(305,215, 310, 220, White);
  Line(305,225, 310, 220, White);
  for(x = 50; x <= 309; x += 20)		// x-axis scale
    Line(x,218, x,222, White);

  Line(30,  28,  30, 220, White);		// y-axis line
  Line(35,  33,  30,  28, White);
  Line(25,  33,  30,  28, White);
  for(y = 40; y <= 202; y += 18)		// y-axis scale
    Line(28,y, 32,y, White);
}

void Draw_FFT(U16 index, float value)
{
    unsigned short height;
    if (index >= 10)  // 고주파 영역에 대해 높이를 스케일링
    {
      height = (unsigned short)(value * 1.7);
    }
    else if(index >= 5)
    {
      height = (unsigned short)(value * 1.4);
    }
    else
    {
      height = (unsigned short)value;
    }

    if (height >= 180.) height = 180.;
    int x_pos = 30 + (index * 15);  // 15 픽셀 간격

    for (int i = 0; i < 7; i++)
    {
        Line(x_pos + i, 219, x_pos + i, 219 - 180, Black);  // 기존 막대 지우기
        Line(x_pos + i, 219, x_pos + i, 219 - height, Red);  // 새 막대 그리기
    }
}