/*
 *  MC2PA8201.c
 *
 *  Author: Kestutis Bivainis
 *
 */

#include "stm32f10x_conf.h"
#include "MC2PA8201.h"
#include "delay.h"
#include "colors.h"
#include "font6x8.h"
#include "font8x8.h"
#include "font8x14.h"
#include <string.h>

static uint32_t MC2PA8201_text_foreground_color=WHITE;
static uint32_t MC2PA8201_text_background_color=BLACK;
static FONT_SIZE MC2PA8201_font_size;

static unsigned char *FontTable[] = {
    (unsigned char *)FONT6x8,
    (unsigned char *)FONT8x8,
    (unsigned char *)FONT8x14
};

static COLOR_MODE MC2PA8201_color_mode;
static ORIENTATION_MODE MC2PA8201_orientation_mode;

static uint8_t RGB12bit_size;
static uint8_t *RGB12bit;
static uint8_t RGB16bit_size;
static uint8_t *RGB16bit;
static uint8_t RGB18bit_size;
static uint8_t *RGB18bit;

static uint16_t display_width=320;
static uint16_t display_height=240;

static uint8_t MADCTL_portrait=0xA0;
static uint8_t MADCTL_landscape=0x00;
static uint8_t MADCTL_portrait_rev=0x60;
static uint8_t MADCTL_landscape_rev=0xC0;

static PIN pins[]={
  {{DB0_Pin,DB0_Speed,DB0_Mode},DB0_Port,DB0_Bus},
  {{DB1_Pin,DB1_Speed,DB1_Mode},DB1_Port,DB1_Bus},
  {{DB2_Pin,DB2_Speed,DB2_Mode},DB2_Port,DB2_Bus},
  {{DB3_Pin,DB3_Speed,DB3_Mode},DB3_Port,DB3_Bus},
  {{DB4_Pin,DB4_Speed,DB4_Mode},DB4_Port,DB4_Bus},
  {{DB5_Pin,DB5_Speed,DB5_Mode},DB5_Port,DB5_Bus},
  {{DB6_Pin,DB6_Speed,DB6_Mode},DB6_Port,DB6_Bus},
  {{DB7_Pin,DB7_Speed,DB7_Mode},DB7_Port,DB7_Bus},
  {{RW_Pin, RW_Speed, RW_Mode}, RW_Port, RW_Bus},
  {{RD_Pin, RD_Speed, RD_Mode}, RD_Port, RD_Bus},
  {{RS_Pin, RS_Speed, RS_Mode}, RS_Port, RS_Bus},
  {{CS_Pin, CS_Speed, CS_Mode}, CS_Port, CS_Bus},
  {{RST_Pin,RST_Speed,RST_Mode},RST_Port,RST_Bus},
};

static void GPIO_Configuration(void) {

  uint32_t i;

  for(i=0;i<sizeof(pins)/sizeof(PIN);i++) {
    RCC_APB2PeriphClockCmd(pins[i].GPIO_Bus,ENABLE);
    GPIO_Init(pins[i].GPIOx,&pins[i].GPIO_InitStructure);
  }
}

static void FSMC_LCD_Init(uint8_t AddressSetupTime,uint8_t DataSetupTime) {

  FSMC_NORSRAMInitTypeDef FSMC_NORSRAMInitStructure;
  FSMC_NORSRAMTimingInitTypeDef  FSMC_NORSRAMTimingInitStructure;

  /* Enable the FSMC Clock */
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);

  FSMC_NORSRAMTimingInitStructure.FSMC_AddressSetupTime = AddressSetupTime;
  FSMC_NORSRAMTimingInitStructure.FSMC_AddressHoldTime = 0x00;
  FSMC_NORSRAMTimingInitStructure.FSMC_DataSetupTime = DataSetupTime;
  FSMC_NORSRAMTimingInitStructure.FSMC_BusTurnAroundDuration = 0x00;
  FSMC_NORSRAMTimingInitStructure.FSMC_CLKDivision = 0x00;
  FSMC_NORSRAMTimingInitStructure.FSMC_DataLatency = 0x00;
  FSMC_NORSRAMTimingInitStructure.FSMC_AccessMode = FSMC_AccessMode_B;

  FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM1;
  FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
  FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_NOR;
  FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_8b;
  //FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Enable;
  FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
  FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
  FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
  FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
  FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
  FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
  FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
  //FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Enable;
  FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
  FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &FSMC_NORSRAMTimingInitStructure;
  FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &FSMC_NORSRAMTimingInitStructure;

  FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);

  /* Enable FSMC Bank1_SRAM Bank */
  FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, ENABLE);
}

static void lcd_rst(void) {

  GPIO_ResetBits(RST_Port, RST_Pin);
  DWT_Delay(10000);
  GPIO_SetBits(RST_Port, RST_Pin);
  DWT_Delay(100000);
}

static __forceinline uint8_t rd_reg(uint8_t index) {
  LCD_REG8 = index;
  return (LCD_DAT8);
}

static __forceinline uint8_t rd_dat(void) {
  return (LCD_DAT8);
}

static __forceinline void wr_cmd(uint8_t index) {
  LCD_REG8 = index;
}

static __forceinline void wr_dat(uint8_t val) {
  LCD_DAT8 = val;
}

static __forceinline void wr_reg(uint8_t index,uint8_t val) {
  LCD_REG8 = index;
  LCD_DAT8 = val;
}

uint8_t MC2PA8201_Init(uint8_t AddressSetupTime,uint8_t DataSetupTime) {

  GPIO_Configuration();
  FSMC_LCD_Init(AddressSetupTime,DataSetupTime);

  lcd_rst();

  wr_cmd(SLEEP_OUT);
  wr_cmd(DISPLAY_INVERSION_OFF);
  wr_cmd(IDLE_MODE_OFF);
  wr_cmd(NORMAL_DISPLAY_MODE_ON);
  DWT_Delay(125000);
  wr_cmd(DISPLAY_ON);

  return MC2PA8201_OK;
}

void MC2PA8201_SetLUT_params(COLOR_MODE color_mode,uint8_t LUT_size,uint8_t *LUT) {

  switch(color_mode) {
    case COLOR_12BIT:
      RGB12bit_size=LUT_size;
      RGB12bit=LUT;
      break;
    case COLOR_16BIT:
      RGB16bit_size=LUT_size;
      RGB16bit=LUT;
      break;
    case COLOR_18BIT:
      RGB18bit_size=LUT_size;
      RGB18bit=LUT;
      break;
    case COLOR_24BIT:
      break;
  }
}

void MC2PA8201_SetDimensions(uint16_t width,uint16_t height) {

  display_width=width;
  display_height=height;
}

void MC2PA8201_ColorMode(COLOR_MODE color_mode) {

  uint32_t i;

  MC2PA8201_color_mode=color_mode;
  wr_reg(INTERFACE_PIXEL_FORMAT,MC2PA8201_color_mode);

  switch(MC2PA8201_color_mode) {
    case COLOR_12BIT:
      wr_cmd(COLOUR_SET);
      for(i=0;i<RGB12bit_size;i++) {
        wr_dat(RGB12bit[i]);
      }
      break;
    case COLOR_16BIT:
      wr_cmd(COLOUR_SET);
      for(i=0;i<RGB16bit_size;i++) {
        wr_dat(RGB16bit[i]);
      }
      break;
    case COLOR_18BIT:
      wr_cmd(COLOUR_SET);
      for(i=0;i<RGB18bit_size;i++) {
        wr_dat(RGB18bit[i]);
      }
      break;
    case COLOR_24BIT:
      break;
  }
}

void MC2PA8201_SetMADCTL_params(uint8_t landscape,uint8_t portrait,uint8_t landscape_rev,uint8_t portrait_rev) {

  MADCTL_landscape=landscape;
  MADCTL_portrait=portrait;
  MADCTL_landscape_rev=landscape_rev;
  MADCTL_portrait_rev=portrait_rev;
}

void MC2PA8201_OrientationMode(ORIENTATION_MODE orientation_mode) {

  uint8_t MADCTL_param;

  switch(orientation_mode){
    case ORIENTATION_LANDSCAPE:
      MADCTL_param=MADCTL_landscape;
      break;
    case ORIENTATION_LANDSCAPE_REV:
      MADCTL_param=MADCTL_landscape_rev;
      break;
    case ORIENTATION_PORTRAIT:
      MADCTL_param=MADCTL_portrait;
      break;
    case ORIENTATION_PORTRAIT_REV:
      MADCTL_param=MADCTL_portrait_rev;
      break;
  }
  MC2PA8201_orientation_mode=orientation_mode;
  wr_reg(MEMORY_ACCESS_CONTROL,MADCTL_param);
}

void MC2PA8201_ClearScreen(uint32_t color) {

  MC2PA8201_Fill(0,0,MC2PA8201_GetWidth()-1,MC2PA8201_GetHeight()-1,color);
}

void MC2PA8201_Fill(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1,uint32_t color) {

  uint32_t i,j=(x1-x0+1)*(y1-y0+1);
  uint8_t b1,b2,b3,r,g,b;

  MC2PA8201_SetWriteWindow(x0,y0,x1,y1);

  r=color>>16;
  g=color>>8;
  b=color;

  switch(MC2PA8201_color_mode) {
    case COLOR_12BIT:
      b1=(r&0xF0)|(g>>4);
      b2=(b&0xF0)|(r>>4);
      b3=(g&0xF0)|(b>>4);
      for(i=0;i<j;i+=2) {
        wr_dat(b1);
        wr_dat(b2);
        wr_dat(b3);
      }
      break;
    case COLOR_16BIT:
      b1=(r&0xF8)|((g&0xE0)>>5);
      b2=((g&0x1C)<<3)|((b&0xF8)>>3);
      for(i=0;i<j;i++) {
        wr_dat(b1);
        wr_dat(b2);
      }
      break;
    case COLOR_18BIT:
      for(i=0;i<j;i++) {
        wr_dat(r&0xFC);
        wr_dat(g&0xFC);
        wr_dat(b&0xFC);
      }
      break;
    case COLOR_24BIT:
       for(i=0;i<j;i++) {
        wr_dat(r);
        wr_dat(g);
        wr_dat(b);
      }
      break;
  }
}

void MC2PA8201_SetWindow(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1) {

  wr_cmd(COLUMN_ADDRESS_SET);
  wr_dat(x0>>8);
  wr_dat(x0);
  wr_dat(x1>>8);
  wr_dat(x1);

  wr_cmd(PAGE_ADDRESS_SET);
  wr_dat(y0>>8);
  wr_dat(y0);
  wr_dat(y1>>8);
  wr_dat(y1);
}

void MC2PA8201_SetWriteWindow(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1) {

  MC2PA8201_SetWindow(x0,y0,x1,y1);
  wr_cmd(MEMORY_WRITE);
}

void MC2PA8201_SetReadWindow(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1) {

  MC2PA8201_SetWindow(x0,y0,x1,y1);
  wr_cmd(MEMORY_READ);
}

void MC2PA8201_FillPixel(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1,uint32_t *color) {

  uint32_t i,j=(x1-x0+1)*(y1-y0+1);

  MC2PA8201_SetWriteWindow(x0,y0,x1,y1);

  switch(MC2PA8201_color_mode) {
    case COLOR_12BIT:
      for(i=0;i<j;i+=2) {
        wr_dat(((color[i]>>16)&0xF0)|((color[i]>>12)&0x0F));
        wr_dat(((color[i])&0xF0)|((color[i+1]>>16)&0x0F));
        wr_dat(((color[i+1]>>8)&0xF0)|((color[i+1]>>4)&0x0F));
      }
      break;
    case COLOR_16BIT:
      for(i=0;i<j;i++) {
        wr_dat(((color[i]>>16)&0xF8)|(((color[i]>>13)&0x07)));
        wr_dat(((color[i]>>5)&0xE0)|((color[i]>>3)&0x1F));
      }
      break;
    case COLOR_18BIT:
      for(i=0;i<j;i++) {
        wr_dat((color[i]>>16)&0xFC);
        wr_dat((color[i]>>8)&0xFC);
        wr_dat((color[i])&0xFC);
      }
      break;
    case COLOR_24BIT:
       for(i=0;i<j;i++) {
        wr_dat(color[i]>>16);
        wr_dat(color[i]>>8);
        wr_dat(color[i]);
      }
      break;
  }
}

void MC2PA8201_FillFromBuffer(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1,uint8_t *data) {

  uint32_t i,j=(x1-x0+1)*(y1-y0+1);

  MC2PA8201_SetWriteWindow(x0,y0,x1,y1);

  switch(MC2PA8201_color_mode) {
    case COLOR_12BIT:
      // always writing even count of pixels
      for(i=0;i<j;i+=2) {
        wr_dat(*data++);
        wr_dat(*data++);
        wr_dat(*data++);
      }
      break;
    case COLOR_16BIT:
      for(i=0;i<j;i++) {
        wr_dat(*data++);
        wr_dat(*data++);
      }
      break;
    case COLOR_18BIT:
    case COLOR_24BIT:
      for(i=0;i<j;i++) {
        wr_dat(*data++);
        wr_dat(*data++);
        wr_dat(*data++);
      }
      break;
  }
}

void MC2PA8201_SetPixel(uint16_t x, uint16_t y, uint32_t color) {

  uint8_t r,g,b;

  MC2PA8201_SetWriteWindow(x,y,x,y);

  r=color>>16;
  g=color>>8;
  b=color;

  switch(MC2PA8201_color_mode) {
    case COLOR_12BIT:
      wr_dat((r&0xF0)|(g>>4));
      wr_dat((b&0xF0)|(r>>4));
      wr_dat((g&0xF0)|(b>>4));
      break;
    case COLOR_16BIT:
      wr_dat((r&0xF8)|((g>>5)&0x07));
      wr_dat(((g<<3)&0xE0)|((b>>3)&0x1F));
      break;
    case COLOR_18BIT:
      wr_dat(r&0xFC);
      wr_dat(g&0xFC);
      wr_dat(b&0xFC);
      break;
    case COLOR_24BIT:
      wr_dat(r);
      wr_dat(g);
      wr_dat(b);
      break;
  }
}

void MC2PA8201_SetScrollPosition(uint16_t pos) {
  wr_cmd(VERTICAL_SCROLLING_START_ADDRESS);
  wr_dat(pos>>8);
  wr_dat(pos);
}

void MC2PA8201_ScrollArea(uint16_t y,uint16_t pos) {
  uint16_t bfa=240-pos-y;
  wr_cmd(VERTICAL_SCROLLING_DEFINITION);
  wr_dat(y>>8);
  wr_dat(y);
  wr_dat(pos>>8);
  wr_dat(pos);
  wr_dat(bfa>>8);
  wr_dat(bfa);
}

void MC2PA8201_PartialArea(uint16_t y0,uint16_t y1) {
  wr_cmd(PARTIAL_AREA);
  wr_dat(y0>>8);
  wr_dat(y0);
  wr_dat(y1>>8);
  wr_dat(y1);
}

void MC2PA8201_PartialMode(void) {
  wr_cmd(PARTIAL_MODE_ON);
}

void MC2PA8201_NormalDisplayMode(void) {
  wr_cmd(NORMAL_DISPLAY_MODE_ON);
}

void MC2PA8201_DisplayOff(void) {
  wr_cmd(DISPLAY_OFF);
}

void MC2PA8201_DisplayOn(void) {
  wr_cmd(DISPLAY_ON);
}

void MC2PA8201_Sleep(void) {
  wr_cmd(DISPLAY_OFF);
  wr_cmd(SLEEP_IN);
  DWT_Delay(5000);
}

void MC2PA8201_Wakeup(void) {
  wr_cmd(SLEEP_OUT);
  DWT_Delay(120000);
  wr_cmd(DISPLAY_ON);
  DWT_Delay(5000);
}

void MC2PA8201_Gamma(GAMMA_VALUE val) {
  wr_reg(GAMMA_SET,val);
}

void MC2PA8201_IdleModeOn(void) {
  wr_cmd(IDLE_MODE_ON);
}

void MC2PA8201_IdleModeOff(void) {
  wr_cmd(IDLE_MODE_OFF);
}

void MC2PA8201_DisplayInversionOn(void) {
  wr_cmd(DISPLAY_INVERSION_ON);
}

void MC2PA8201_DisplayInversionOff(void) {
  wr_cmd(DISPLAY_INVERSION_OFF);
}

void MC2PA8201_TearingEffectLineOn(TE val) {
  wr_cmd(TEARING_EFFECT_LINE_ON);
  wr_dat(val);
}

void MC2PA8201_TearingEffectLineOff(void) {
  wr_cmd(TEARING_EFFECT_LINE_OFF);
}

uint16_t MC2PA8201_GetWidth(void) {

  uint16_t ret;

  switch(MC2PA8201_orientation_mode){
    case ORIENTATION_LANDSCAPE:
    case ORIENTATION_LANDSCAPE_REV:
      ret=display_width;
    break;
    case ORIENTATION_PORTRAIT:
    case ORIENTATION_PORTRAIT_REV:
      ret=display_height;
    break;
  }
  return ret;
}

uint16_t MC2PA8201_GetHeight(void) {

  uint16_t ret;

  switch(MC2PA8201_orientation_mode){
    case ORIENTATION_LANDSCAPE:
    case ORIENTATION_LANDSCAPE_REV:
      ret=display_height;
    break;
    case ORIENTATION_PORTRAIT:
    case ORIENTATION_PORTRAIT_REV:
      ret=display_width;
    break;
  }
  return ret;
}

void MC2PA8201_SetFont(FONT_SIZE font_size) {

  MC2PA8201_font_size=font_size;
}

void MC2PA8201_SetTextColors(uint32_t fColor, uint32_t bColor) {

  MC2PA8201_text_foreground_color = fColor;
  MC2PA8201_text_background_color = bColor;
}

void MC2PA8201_PutChar(char c, uint16_t x, uint16_t y) {

  uint32_t i,j;
  uint32_t nCols;
  uint32_t nRows;
  uint32_t nBytes;
  uint8_t PixelRow;
  uint8_t Mask;
  uint32_t Word0;
  uint32_t Word1;
  unsigned char *pFont;
  unsigned char *pChar;
  uint8_t r,g,b;

  // get pointer to the beginning of the selected font table
  pFont = (unsigned char *)FontTable[MC2PA8201_font_size];

  // get the nColumns, nRows and nBytes
  nCols = *pFont;
  nRows = *(pFont + 1);
  nBytes = *(pFont + 2);

  // get pointer to the first byte of the desired character
  pChar = pFont + (nBytes * (c - 0x1F));

  MC2PA8201_SetWriteWindow(x,y,x + nCols - 1,y + nRows - 1);

  // loop on each row
  for (i = 0; i < nRows; i++) {

    // copy pixel row from font table and then decrement row
    PixelRow = *pChar++;

    // loop on each pixel in the row (left to right)
    // Note: we do two pixels each loop
    Mask = 0x80;
    for (j = 0; j < nCols; j += 2) {

      // if pixel bit set, use foreground color; else use the background color
      // now get the pixel color for two successive pixels
      if (PixelRow & Mask)
        Word0 = MC2PA8201_text_foreground_color;
      else
        Word0 = MC2PA8201_text_background_color;
      Mask >>= 1;

      if (PixelRow & Mask)
        Word1 = MC2PA8201_text_foreground_color;
      else
        Word1 = MC2PA8201_text_background_color;
      Mask >>= 1;

      switch(MC2PA8201_color_mode){
        case COLOR_12BIT:
          r=Word0>>16;
          g=Word0>>8;
          wr_dat((r&0xF0)|(g>>4));
          b=Word0;
          r=Word1>>16;
          wr_dat((b&0xF0)|(r>>4));
          g=Word1>>8;
          b=Word1;
          wr_dat((g&0xF0)|(b>>4));
          break;
        case COLOR_16BIT:
          r=Word0>>16;
          g=Word0>>8;
          wr_dat((r&0xF8)|((g>>5)&0x07));
          b=Word0;
          wr_dat(((g<<3)&0xE0)|((b>>3)&0x1F));
          r=Word1>>16;
          g=Word1>>8;
          wr_dat((r&0xF8)|((g>>5)&0x07));
          b=Word1;
          wr_dat(((g<<3)&0xE0)|((b>>3)&0x1F));
          break;
        case COLOR_18BIT:
          wr_dat((Word0>>16)&0xFC);
          wr_dat((Word0>>8)&0xFC);
          wr_dat(Word0&0xFC);
          wr_dat((Word1>>16)&0xFC);
          wr_dat((Word1>>8)&0xFC);
          wr_dat(Word1&0xFC);
          break;
        case COLOR_24BIT:
          wr_dat(Word0>>16);
          wr_dat(Word0>>8);
          wr_dat(Word0);
          wr_dat(Word1>>16);
          wr_dat(Word1>>8);
          wr_dat(Word1);
          break;
      }
    }
  }
}

void MC2PA8201_PutStr(char *pString, uint16_t x, uint16_t y) {

  if(y+FontTable[MC2PA8201_font_size][1]>MC2PA8201_GetHeight())
    return;

  // loop until null-terminator is seen
  while (*pString) {
    if (x+FontTable[MC2PA8201_font_size][0]>MC2PA8201_GetWidth()) break;
    // draw the character
    MC2PA8201_PutChar(*pString++, x, y);
    x+=FontTable[MC2PA8201_font_size][0];
  }
}

void MC2PA8201_PutStrCEOL(char *pString, uint16_t x, uint16_t y) {

  if(y+FontTable[MC2PA8201_font_size][1]>MC2PA8201_GetHeight())
    return;

  // loop until null-terminator is seen
  while (*pString) {
    if (x+FontTable[MC2PA8201_font_size][0]>MC2PA8201_GetWidth()) break;
    // draw the character
    MC2PA8201_PutChar(*pString++, x, y);
    x+=FontTable[MC2PA8201_font_size][0];
  }
  while(x+FontTable[MC2PA8201_font_size][0]<=MC2PA8201_GetWidth()) {
    MC2PA8201_PutChar(' ', x, y);
    x+=FontTable[MC2PA8201_font_size][0];
  }
}

void MC2PA8201_PutStrCentered(char *pString, uint16_t y) {

  uint32_t length=strlen(pString)*FontTable[MC2PA8201_font_size][0];

  MC2PA8201_PutStr(pString,length>MC2PA8201_GetWidth()?0:(MC2PA8201_GetWidth()-length)/2,y);
}

void MC2PA8201_ReadRegister(uint8_t reg,uint8_t length,uint8_t *val) {

  uint8_t i;

  // first read is dummy read
  val[0]=rd_reg(reg);

  for(i=0;i<length;i++) {
    val[i]=rd_dat();
  }
}

void MC2PA8201_ReadMemory(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1,uint8_t *data) {

  uint8_t i,j=(x1-x0+1)*(y1-y0+1);

  MC2PA8201_SetReadWindow(x0,y0,x1,y1);
  // first read is dummy read
  *data=rd_dat();

  switch(MC2PA8201_color_mode) {
    case COLOR_12BIT:
      // always reading even count of pixels
      for(i=0;i<j;i+=2) {
        *data++=rd_dat();
        *data++=rd_dat();
        *data++=rd_dat();
      }
      break;
    case COLOR_16BIT:
      for(i=0;i<j;i++) {
        *data++=rd_dat();
        *data++=rd_dat();
      }
      break;
    case COLOR_18BIT:
    case COLOR_24BIT:
      for(i=0;i<j;i++) {
        *data++=rd_dat();
        *data++=rd_dat();
        *data++=rd_dat();
      }
      break;
  }
}
