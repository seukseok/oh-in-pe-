/* ============================================================================ */
/*	Exp18_1_FFT(1).c : 1kHz의 정현파/삼각파/구형파에 대한 FFT 분석		*/
/* ============================================================================ */
/*					Programmed by Duck-Yong Yoon in 2016.   */

#include "stm32f767xx.h"
#include "OK-STM767.h"
#include <arm_math.h>				// 반드시 마지막에 인클루드할 것.

void SysTick_Handler(void);			/* SysTick interrupt(100kHz) for DAC output */
void TIM7_IRQHandler(void);			/* TIM7 interrupt(25.6kHz) for ADC input */
void Display_waveform_screen(void);		/* display waveform screen */
void Draw_waveform_axis(void);			/* draw axis of waveform screen */
void Clear_waveform(void);			/* clear graphic screen */
void Display_FFT_screen(void);			/* display FFT screen */
void Draw_FFT(U16 index, float value);		/* draw bar of FFT result */

unsigned char DAC_count, wave_mode, wave_flag, FFT_mode, FFT_flag;
unsigned short wave_count, FFT_count;
unsigned int time_1sec = 0;

float DAC_buffer[300], ADC_buffer[300];
float FFT_buffer[512], FFT_input[512], FFT_output[256], max_value;
unsigned int max_index;

//arm_cfft_radix4_instance_f32 S;--> DEPRECATED 된 함수를 위한 Parameter이므로  arm_cfft_instance_f32로 대체 

arm_cfft_instance_f32 S2; 


/* ----- 인터럽트 처리 프로그램 ----------------------------------------------- */

void SysTick_Handler(void)			/* SysTick interrupt(100kHz) for DAC output */
{
  unsigned short DAC_data;

  if(wave_mode == 1)                            // calculate data of sin wave
    DAC_data = (unsigned short)(2047.*arm_sin_f32(DAC_count*3.6*3.141592/180.) + 2047.);
  else if(wave_mode == 2)                       // calculate data of triangular wave
    { if(DAC_count <= 49)
        DAC_data = (unsigned short)((4095./50.)*(float)DAC_count);
      else
        DAC_data = (unsigned short)((-4095./50.)*(float)DAC_count + 8190.);
    }
  else if(wave_mode == 3)                       // calculate data of square wave
    { if(DAC_count <= 49) DAC_data = 4095;
      else                DAC_data = 0;
    }

  DAC->DHR12R1 = DAC_data;			// output data to D/A converter

  DAC_count++;
  if(DAC_count >= 100) DAC_count = 0;

  if(FFT_mode == 0)
    { DAC_buffer[wave_count] = DAC_data;	// DAC output data

      ADC1->CR2 |= 0x40000000;			// start conversion by software
      while(!(ADC1->SR & 0x00000002));		// wait for end of conversion
      ADC_buffer[wave_count] = ADC1->DR;	// ADC input data

      wave_count++;
      if(wave_count >= 300) wave_count = 0;

      time_1sec++;				// check 1sec time
      if(time_1sec >= 100000)
        { time_1sec = 0;
          wave_flag = 1;
        }
    }
}

void TIM7_IRQHandler(void)			/* TIM7 interrupt(25.6kHz) for ADC input */
{
  TIM7->SR = 0x0000;				// clear pending bit of TIM7 interrupt

  if(FFT_mode == 1)				// if FFT_mode = 1, go A/D conversion
    { ADC1->CR2 |= 0x40000000;			// start conversion by software
      while(!(ADC1->SR & 0x00000002));		// wait for end of conversion
      FFT_buffer[FFT_count] = ((float)ADC1->DR - 2048.)/2048.;
      FFT_buffer[FFT_count+1] = 0.;

      FFT_count += 2;
      if(FFT_count >= 512)
        { FFT_count = 0;
          FFT_flag = 1;
        }
    }
}

/* ----- 메인 프로그램 -------------------------------------------------------- */

int main(void)
{
  unsigned char key, start_flag;
  unsigned short i, y1, y1o = 0, y2, y2o = 0;	// graphic variable

  Initialize_MCU();				// initialize MCU and kit
  Delay_ms(50);					// wait for system stabilization
  Initialize_LCD();				// initialize text LCD module
  Initialize_TFT_LCD();				// initialize TFT-LCD modul
//  arm_cfft_radix4_init_f32(&S, 256, 0, 1);      // initialize radix-4 CFFT function --> DEPRECATED 된 함수 arm_cfft_f32()로 대체 

  LCD_string(0x80," OK-STM767 V1.0 ");          // display title
  LCD_string(0xC0,"   Exp18_1.c    ");

  TFT_string(0,4,Green,Black,"****************************************");
  TFT_string(0,6,White,Black,"     여러가지 파형(1kHz)의 FFT 분석     ");
  TFT_string(0,8,Green,Black,"****************************************");
  TFT_string(0,25,Cyan,Black, "  정현      삼각      구형");
  TFT_string(0,27,Cyan,Black, "   파        파        파 ");
  TFT_string(32,25,Magenta,Black, "FFT ");
  TFT_string(32,27,Magenta,Black, "분석");

  Rectangle( 12,196,  52,235, Yellow);		// display outline of key
  Rectangle( 92,196, 132,235, Yellow);
  Rectangle(172,196, 212,235, Yellow);
  Rectangle(252,196, 292,235, Yellow);
  Beep();

  key = no_key;					// wait for start
  while(key == no_key) key = Key_input();
  start_flag = 0;

  GPIOA->MODER |= 0x00000300;			// PA4 = analog mode(DAC_OUT1)
  RCC->APB1ENR |= 0x20000000;			// enable DAC clock
  DAC->CR = 0x00000003;				// DAC channel 1 enable, output buffer disable

  GPIOA->MODER |= 0x00003000;			// PA6 = analog mode(ADC12_IN6)
  RCC->APB2ENR |= 0x00000100;			// enable ADC1 clock
  ADC->CCR = 0x00000000;			// ADCCLK = 54MHz/2 = 27MHz
  ADC1->SMPR2 = 0x00040000;			// sampling time of channel 6 = 15 cycle
  ADC1->CR1 = 0x00000000;			// 12-bit resolution
  ADC1->CR2 = 0x00000001;			// right alignment, single conversion, ADON = 1
  ADC1->SQR1 = 0x00000000;			// total regular channel number = 1
  ADC1->SQR3 = 0x00000006;			// channel 6

  SysTick->LOAD  = 269;				// 27MHz/(269+1) = 100kHz
  SysTick->VAL   = 0;				// clear SysTick Counter
  SysTick->CTRL  = 0x00000003;			// 216MHz/8 = 27MHz, enable SysTick and interrupt

  RCC->APB1ENR |= 0x00000020;			// enable TIM7 clock
  TIM7->PSC = 9;				// 108MHz/(9+1) = 10.8MHz
  TIM7->ARR = 421;				// 10.8MHz/(421+1) = 25.6kHz
  TIM7->CNT = 0;				// clear counter
  TIM7->DIER = 0x0001;				// enable update interrupt
  TIM7->CR1 = 0x0085;				// enable TIM7, update event, and preload
  NVIC->ISER[1] |= 0x00800000;			// enable (55)TIM7 interrupt

  while(1)
    { if(start_flag == 0)			// if initial start, skip key input
        start_flag = 1;
      else
        key = Key_input();

      if(key == KEY1)				// if KEY1, sin wave mode
       { key = no_key;
         FFT_mode = 0;
         wave_mode = 1;
         wave_count = 0;
	 DAC_count = 0;
	 Display_waveform_screen();
         TFT_string(5,0,White,Magenta," 정현파(1kHz)의 입력/출력 파형 ");
       }
      else if(key == KEY2)			// if KEY2, triangular wave mode
       { key = no_key;
         FFT_mode = 0;
         wave_mode = 2;
         wave_count = 0;
	 DAC_count = 0;
	 Display_waveform_screen();
         TFT_string(5,0,White,Magenta," 삼각파(1kHz)의 입력/출력 파형 ");
       }
      else if(key == KEY3)			// if KEY3, square wave mode
       { key = no_key;
         FFT_mode = 0;
         wave_mode = 3;
         wave_count = 0;
	 DAC_count = 0;
	 Display_waveform_screen();
         TFT_string(5,0,White,Magenta," 구형파(1kHz)의 입력/출력 파형 ");
       }
      else if(key == KEY4)			// if KEY4, FFT mode
       { key = no_key;
         FFT_mode = 1;
         Display_FFT_screen();
	 if(wave_mode == 1)      
           TFT_string(5,0,White,Magenta," 정현파(1kHz)의 FFT 분석 결과 ");
         else if(wave_mode == 2) 
           TFT_string(5,0,White,Magenta," 삼각파(1kHz)의 FFT 분석 결과 ");
         else if(wave_mode == 3) 
           TFT_string(5,0,White,Magenta," 구형파(1kHz)의 FFT 분석 결과 ");
       }

      if((FFT_mode == 0) && (wave_flag == 1))	// if not FFT mode, display waveforms
        { wave_flag = 0;

	  Clear_waveform();
	  for(i = 0; i < 280; i++)
            { y1 = (unsigned short)(120. - DAC_buffer[i]*83./4095. + 0.5);
              if(i == 0) y1o = y1;
              Line(30+i,y1o, 31+i,y1, Green);
              y1o = y1;
          
              y2 = (unsigned short)(220. - ADC_buffer[i]*83./4095. + 0.5);
              if(i == 0) y2o = y2;
              Line(30+i,y2o, 31+i,y2, Cyan);
              y2o = y2;
            }
	}
      else if((FFT_mode == 1) && (FFT_flag == 1)) // if FFT mode, display FFT result
        { FFT_flag = 0;

	  for (i = 0; i < 512; i++)
            FFT_input[i] = FFT_buffer[i];

          
          arm_cfft_f32(&S2, FFT_input, 0, 1); // 변경된 함수 
//          arm_cfft_radix4_f32(&S, FFT_input);	// calculate radix-4 CFFT, --> DEPRECATED 된 함수 arm_cfft_f32()로 대체 
          arm_cmplx_mag_f32(FFT_input, FFT_output, 256);
          arm_max_f32(FFT_output, 256, &max_value, &max_index);

          for (i = 1; i < 128; i++)		// draw FFT result
            Draw_FFT(i, FFT_output[i]);
        }
    }
}

/* ----- 사용자 정의 함수 ----------------------------------------------------- */

void Display_waveform_screen(void)		/* display waveform screen */
{
  TFT_clear_screen();				// clear screen

  TFT_color(White,Black);
  TFT_English_pixel(18,213, '0');		// 0.0 for ADC waveform
  TFT_English_pixel(2, 188, '1');		// 1.0
  TFT_English_pixel(10,188, '.');
  TFT_English_pixel(18,188, '0');
  TFT_English_pixel(2, 163, '2');		// 2.0
  TFT_English_pixel(10,163, '.');
  TFT_English_pixel(18,163, '0');
  TFT_English_pixel(2, 138, '3');		// 3.0
  TFT_English_pixel(10,138, '.');
  TFT_English_pixel(18,138, '0');
  TFT_English_pixel(18,113, '0');		// 0.0 for DAC waveform
  TFT_English_pixel(2,  88, '1');		// 1.0
  TFT_English_pixel(10, 88, '.');
  TFT_English_pixel(18, 88, '0');
  TFT_English_pixel(2,  63, '2');		// 2.0
  TFT_English_pixel(10, 63, '.');
  TFT_English_pixel(18, 63, '0');
  TFT_English_pixel(2,  38, '3');		// 3.0
  TFT_English_pixel(10, 38, '.');
  TFT_English_pixel(18, 38, '0');
  TFT_color(Magenta,Black);
  TFT_English_pixel(2,  18, '[');               // [V]
  TFT_English_pixel(10, 18, 'V');
  TFT_English_pixel(18, 18, ']');

  TFT_color(White,Black);
  TFT_English_pixel(26,  222, '0');		// 0.0 for time
  TFT_English_pixel(68,  222, '0');		// 0.5
  TFT_English_pixel(76,  222, '.');
  TFT_English_pixel(84,  222, '5');
  TFT_English_pixel(118, 222, '1');		// 1.0
  TFT_English_pixel(126, 222, '.');
  TFT_English_pixel(134, 222, '0');
  TFT_English_pixel(168, 222, '1');		// 1.5
  TFT_English_pixel(176, 222, '.');
  TFT_English_pixel(184, 222, '5');
  TFT_English_pixel(218, 222, '2');		// 2.0
  TFT_English_pixel(226, 222, '.');
  TFT_English_pixel(234, 222, '0');
  TFT_color(Magenta,Black);
  TFT_English_pixel(285, 223, '[');		// [ms]
  TFT_English_pixel(293, 223, 'm');
  TFT_English_pixel(301, 223, 's');
  TFT_English_pixel(309, 223, ']');

  Draw_waveform_axis();				// draw axis of waveform screen
}

void Draw_waveform_axis(void)			/* draw axis of waveform screen */
{ 
  unsigned short x, y;
  
  Line(30,  220, 310, 220, White);		// x-axis line
  Line(305, 215, 310, 220, White);
  Line(305, 225, 310, 220, White);
  Line(30,  130, 30,  220, White);		// y-axis line
  Line(35,  135, 30,  130, White);
  Line(25,  135, 30,  130, White);

  Line(30,  120, 310, 120, White);		// x-axis line
  Line(305, 115, 310, 120, White);
  Line(305, 125, 310, 120, White);
  Line(30,  30,  30,  120, White);		// y-axis line
  Line(35,  35,  30,  30,  White);
  Line(25,  35,  30,  30,  White);
  
  for(x = 80; x <= 280; x += 50)		// x-axis scale
    { Line(x,218, x,222, White);
      for(y = 35; y <= 120; y += 5)		// vertical line
        TFT_pixel(x,y, White);

      Line(x,118, x,122, White);
      for(y = 135; y <= 220; y += 5)		// vertical line
        TFT_pixel(x,y, White);
    }

  for(y = 95; y >= 45; y -= 25)		        // y-axis scale
    { Line(28,y, 32,y, White);
      for(x = 30; x <= 310; x += 5)		// horizontal line
        TFT_pixel(x,y, White);
    }

  for(y = 195; y >= 145; y -= 25)		// y-axis scale
    { Line(28,y, 32,y, White);
      for(x = 30; x <= 310; x += 5)		// horizontal line
        TFT_pixel(x,y, White);
    }
}

void Clear_waveform(void)			/* clear waveform */
{ 
  unsigned short x, y;

  for(y = 30; y <= 120; y++)			// clear DAC waveform
    { TFT_GRAM_address(30,y);
      for(x = 30; x <= 310; x++)
        TFT_data(Black);
    }

  for(y = 130; y <= 220; y++)			// clear ADC waveform
    { TFT_GRAM_address(30,y);
      for(x = 30; x <= 310; x++)
        TFT_data(Black);
    }

  Draw_waveform_axis();				// draw axis of waveform screen
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
  TFT_English_pixel(26, 222, '0');		// 0
  TFT_English_pixel(46, 222, '1');		// 1
  TFT_English_pixel(66, 222, '2');		// 2
  TFT_English_pixel(86, 222, '3');		// 3
  TFT_English_pixel(106,222, '4');		// 4
  TFT_English_pixel(126,222, '5');		// 5
  TFT_English_pixel(146,222, '6');		// 6
  TFT_English_pixel(166,222, '7');		// 7
  TFT_English_pixel(186,222, '8');		// 8
  TFT_English_pixel(206,222, '9');		// 9
  TFT_English_pixel(222,222, '1');		// 10
  TFT_English_pixel(230,222, '0');
  TFT_English_pixel(242,222, '1');		// 11
  TFT_English_pixel(250,222, '1');
  TFT_English_pixel(262,222, '1');		// 12
  TFT_English_pixel(270,222, '2');
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

  height = (unsigned short)(180.*value/max_value + 0.5);
  if(height >= 180.) height = 180.;

  Line(30+2*index, 219, 30+2*index, 219 - 180, Black);	    // delete old bar
  Line(30+2*index, 219, 30+2*index, 219 - height, Magenta); // draw new bar
}
