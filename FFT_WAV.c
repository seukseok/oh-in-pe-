#include "stm32f767xx.h"
#include "OK-STM767.h"
#include <stdlib.h>
#include <arm_math.h>				// ???? ???????? ???????? ??.
#include "fft.h"
#include "complex.h"
// ???? ?????? ??????? ?????? ?????? ctrl+F -> ????? ???? ???? ??
// #define SAMPLE_SIZE 512 // ????? ???? ???? ??
#define SAMPLE_SIZE 2048
#define FFT_SIZE SAMPLE_SIZE/2

// complex_f *fft_data;
complex_f fft_buffer_pool[SAMPLE_SIZE];

void TIM7_IRQHandler(void);			/* TIM7 interrupt(25.6kHz) for ADC input */
void Display_FFT_screen(void);			/* display FFT screen */
void do_fft(void);
void Draw_FFT(U16 index, float value);		/* draw bar of FFT result */

unsigned char FFT_mode, FFT_flag;
unsigned short FFT_count;

float ADC_buffer[SAMPLE_SIZE];
float FFT_buffer[SAMPLE_SIZE], FFT_input[SAMPLE_SIZE], FFT_output[SAMPLE_SIZE/2], max_value;
// ?????, ???(???), inv fft?? ???? ???????? ????
unsigned int max_index;

/* ----- ?????? ??? ??????? ----------------------------------------------- */

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

/* ----- ???? ??????? -------------------------------------------------------- */

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
  Display_FFT_screen();     
  TFT_string(5,0,White,Magenta," ????? FFT ???? ??? ");

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
/* ----- ????? ???? ??? ----------------------------------------------------- */

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
    // fft_data[i].re = sin(2*3.141592*4*i / 512);
    fft_data[i].im = 0.0;
  }

  ffti_f(fft_data, r, FFT_FORWARD);
  // for(int i = 1; i < N; i++) // ????? ???? ???? ??
  for(int i = 1; i < 128; i++)
  {
    FFT_output[i-1] = sqrt(pow(fft_data[i].re,2)+pow(fft_data[i].im,2));
    if(max_data < FFT_output[i-1])
      max_data = FFT_output[i-1];
  }
  
  for(int i = 0; i < 128; i++)//=
  {
    Draw_FFT(i, FFT_output[i] * 180 / max_data);
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
  // free(fft_data);
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

void Draw_FFT(U16 index, float value)		/* draw bar of FFT result */
{
  unsigned short height;

  // height = (unsigned short)(180.*value/max_value + 0.5);
  height = (unsigned short)value;
  if(height >= 180.) height = 180.;

  Line(30+2*index, 219, 30+2*index, 219 - 180, Black);	    // delete old bar
  Line(30+2*index, 219, 30+2*index, 219 - height, Red); // draw new bar
}
