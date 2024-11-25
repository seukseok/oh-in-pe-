/* ============================================================================ */
/*	    OK-STM767 V1.0 ????(STM32F767VGT6) VS1053b ??? ????		*/
/* ============================================================================ */
/*					Programmed by Duck_Yong Yoon in 2016.   */

// -------------------------------------------------------------------------------
//   ?? ????????? OK-STM767 ?? V1.0?? ???? ??????. ????? ????? ?????????
//  ???? ?????? ????????.
// -------------------------------------------------------------------------------
//	(1) ????? ???  : 216MHz
//	(2) C ???????   : IAR EWARM V7.80.2
//	(3) ????? ???  : High/Size
//	(4) CSTACK ???  : 0x1000
//      (5) ??????     : ?? ??? ???? OK-STM767.h?? ??? ???????? ??.
// -------------------------------------------------------------------------------

void Initialize_VS1053b(void);			// initialize VS1053b
void VS1053b_SCI_Write(U08 address, U16 data);	// write VS1053b control register
unsigned short VS1053b_SCI_Read(U08 address);	// read VS1053b control register
void VS1053b_SCI_Read_Hexadecimal(U08 address);	// read and display VS1053b control register
void VS1053b_software_reset(void);		// VS1053b software reset to change music file
void VS1053b_SetVolume(U16 volume);		// set volume to 5~250(range : 0~254)
void VS1053b_SetBassTreble(U16 bass, S16 treble);// set bass(0~15) and treble(-8 ~ +7)

/* ---------------------------------------------------------------------------- */

unsigned short volume;				// volume value 5 ~ 250(range : 0~254)
unsigned short bass;				// bass value 0 ~ 15
signed short   treble;				// treble value -8 ~ +7

/* ---------------------------------------------------------------------------- */

void Initialize_VS1053b(void)			/* initialize VS1053b */
{
    unsigned short word;

    // 기존 GPIO 설정 유지
    GPIOC->MODER &= 0xFFF000FF;
    GPIOC->MODER |= 0x00011400;
    GPIOC->ODR |= 0x00000160;

    // 하드웨어 리셋
    GPIOC->BSRR = 0x01000000;
    Delay_us(10);
    GPIOC->BSRR = 0x00000100;
    Delay_ms(10);

    SPI3_medium_speed();

    // 기본 모드 설정
    VS1053b_SCI_Write(0x00, 0x0800);  // 일반 모드
    Delay_ms(1);

    // Native SPI 모드 활성화 (WAV 재생에 필요)
    VS1053b_SCI_Write(0x00, 0x0820);
    Delay_ms(1);

    // 클록 설정
    VS1053b_SCI_Write(0x03, 0xE000);  // 5.0x XTALI
    Delay_ms(1);

    // WAV 재생을 위한 추가 설정
    VS1053b_SCI_Write(0x05, 0xAC45);  // 48kHz 샘플링, 스테레오
    VS1053b_SetVolume(200);           // 초기 볼륨 설정
    VS1053b_SetBassTreble(8, 3);      // 기본 음질 설정

    // 디코딩 설정
    VS1053b_SCI_Write(0x07, 0x0020);  // 디코딩 타임아웃 설정

    SPI3_high_speed();

    // VS1053B 칩 버전 확인 (디버깅용)
    word = VS1053b_SCI_Read(0x01);
    if((word & 0x00F0) == 0x0040) {
        // VS1053 확인됨
        Delay_ms(1);
    }
}

void VS1053b_SCI_Write(U08 address, U16 data)	/* write VS1053b control register */
{
  GPIOC->BSRR = 0x00200000;			// -MP3_CS = 0

  SPI3_write(0x02);				// write instruction
  SPI3_write(address);				// write address
  SPI3_write(data >> 8);			// write control register
  SPI3_write(data);

  GPIOC->BSRR = 0x00000020;			// -MP3_CS = 1
}

unsigned short VS1053b_SCI_Read(U08 address)	/* read VS1053b control register */
{
  unsigned short HB, LB;

  GPIOC->BSRR = 0x00200000;			// -MP3_CS = 0

  SPI3_write(0x03);				// read instruction
  SPI3_write(address);				// write address
  HB = SPI3_write(0xFF);			// read control regitser
  LB = SPI3_write(0xFF);

  GPIOC->BSRR = 0x00000020;			// -MP3_CS = 1

  return (HB << 8) + LB;			// return control register
}

void VS1053b_SCI_Read_Hexadecimal(U08 address)	/* read and display VS1053b control register */
{
  GPIOC->BSRR = 0x00200000;			// -MP3_CS = 0

  SPI3_write(0x03);				// read instruction
  SPI3_write(address);				// write address
  TFT_hexadecimal(SPI3_write(0xFF),2);		// read control regitser
  TFT_hexadecimal(SPI3_write(0xFF),2);

  GPIOC->BSRR = 0x00000020;			// -MP3_CS = 1
}

/* ---------------------------------------------------------------------------- */

void VS1053b_software_reset(void)		/* VS1053b software reset to change music file */
{
  unsigned short command;

  command = 0xFFFF;				// wait until HDAT1 =  HDAT0 = 0x0000
  while(command == 0x0000)
    { command = VS1053b_SCI_Read(0x09);
      command |= VS1053b_SCI_Read(0x08);
    }

  command = VS1053b_SCI_Read(0x00);		// software reset
  VS1053b_SCI_Write(0x00, command | 0x0004);
  Delay_ms(2);
  VS1053b_SCI_Write(0x04, 0);			// clear decode time
}

void VS1053b_SetVolume(U16 volume)		/* set volume(5~250) */
{
  unsigned short value;

  if(volume > 250) volume = 250;		// limit max volume
  if(volume < 5)   volume = 5;			// limit min volume

  value = (254 - volume);

  VS1053b_SCI_Write(0x0B, (value << 8) | value); // set left and right volume
}

void VS1053b_SetBassTreble(U16 bass, S16 treble) /* set bass(0~15) and treble(-8 ~ +7) */
{
  unsigned short value;

  value = 0x050A;				// treble limit = 5kHz, bass limit = 100Hz
  value = value + ((treble << 12) & 0xF000);	// treble = -8 ~ +7
  value = value + ((bass << 4) & 0x00F0);	// bass = 0 ~ 15

  VS1053b_SCI_Write(0x02, value);		// set bass and treble
}
