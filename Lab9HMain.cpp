// Lab9HMain.cpp
// Runs on MSPM0G3507
// Lab 9 ECE319H
// Your name
// Last Modified: January 12, 2026

#include <stdio.h>
#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "../inc/ST7735.h"
#include "../inc/Clock.h"
#include "../inc/LaunchPad.h"
#include "../inc/TExaS.h"
#include "../inc/Timer.h"
#include "../inc/SlidePot.h"
#include "../inc/DAC5.h"
#include "SmallFont.h"
#include "LED.h"
#include "Switch.h"
#include "Sound.h"
#include "images/images.h"
extern "C" void __disable_irq(void);
extern "C" void __enable_irq(void);
extern "C" void TIMG12_IRQHandler(void);
// ****note to ECE319K students****
// the data sheet says the ADC does not work when clock is 80 MHz
// however, the ADC seems to work on my boards at 80 MHz
// I suggest you try 80MHz, but if it doesn't work, switch to 40MHz
void PLL_Init(void){ // set phase lock loop (PLL)
  // Clock_Init40MHz(); // run this line for 40MHz
  Clock_Init80MHz(0);   // run this line for 80MHz
}

uint32_t M=1;
uint32_t Random32(void){
  M = 1664525*M+1013904223;
  return M;
}
uint32_t Random(uint32_t n){
  return (Random32()>>16)%n;
}

SlidePot Sensor(1500,0); // copy calibration from Lab 7

typedef enum {dead, alive} status_t;

struct sprite {
  int32_t x;      // x coordinate on screen (0 to 127)
  int32_t y;      // y coordinate on screen (0 to 159)
  int32_t vy;     // vertical velocity (how fast it scrolls down)
  const uint16_t *image; // Pointer to the bitmap array
  status_t life;  // Is it currently active on screen?
};
typedef struct sprite sprite_t;

// Declare our global game objects
sprite_t Skater;
sprite_t Obstacles[4]; // Max 4 obstacles on screen at once
sprite_t EnergyDrink;  // 1 collectible on screen at a time

uint32_t Score = 0;
uint32_t Lives = 3;
uint8_t Semaphore = 0; // Flag to trigger LCD update

// games  engine runs at 30Hz
void TIMG12_IRQHandler(void){uint32_t pos,msg;
  if((TIMG12->CPU_INT.IIDX) == 1){ // this will acknowledge timer interrupt
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)

//this ISR will sample slide-pot to move skater left/right, check buttons for Ollie/Duck commands, 
//and move obstacles down the screen

// game engine goes here

//commit test

    // 1) sample slide pot for skater x position
    //Sensor.In() returns 12-bit ADC value (0 to 4095)
    //map 0-4095 to LCD width (0-110, leaving room for sprite width)

    uint32_t adc_val = Sensor.In(); 
    Skater.x = (110 * adc_val) / 4095;

    // 2) read input switches for jump/duck
    uint32_t buttons = Switch_In();
    // Assuming PA28 is bit 3 in your Switch_In return, and PA27 is bit 2
    bool isJumping = (buttons & 0x08); 
    bool isDucking = (buttons & 0x04);
    
    // Swap skater image based on action (assuming these arrays exist in images.h)
    if(isJumping) {
      // Skater.image = SkaterJumpImage;
    } else if (isDucking) {
      // Skater.image = SkaterDuckImage;
    } else {
      // Skater.image = SkaterNormalImage;
    }

    // 3) move obstacles (scrolling effect)
    for(int i = 0; i < 4; i++){
      if(Obstacles[i].life == alive){
        Obstacles[i].y += Obstacles[i].vy; // Move down the screen
        
        // If it goes off the bottom of the screen, kill it and score a point
        if(Obstacles[i].y > 159){
          Obstacles[i].life = dead;
          Score += 10; 
        }
      }
    }

    // 4) Collision Detection would go here
    //will write this

    // 5) set semaphore
    Semaphore = 1;

    // NO LCD OUTPUT IN INTERRUPT SERVICE ROUTINES


    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
  }
}

uint8_t TExaS_LaunchPadLogicPB27PB26(void){
  return (0x80|((GPIOB->DOUT31_0>>26)&0x03));
}

typedef enum {English, Spanish} Language_t;

Language_t myLanguage=English;

//phrases needed for pump track pro
typedef enum {START_GAME, GAME_OVER, SCORE, LIVES} phrase_t;

const char Start_English[] = "Press Jump to Start";
const char Start_Spanish[] = "Salta para Iniciar";

const char Over_English[]  = "GAME OVER";
const char Over_Spanish[]  = "FIN DEL JUEGO";

const char Score_English[] = "Score: ";
const char Score_Spanish[] = "Puntos: ";

const char Lives_English[] = "Lives: ";
const char Lives_Spanish[] = "Vidas: ";

//2D Array to map [phrase_t][Language_t]
const char *Phrases[4][2] = {
  {Start_English, Start_Spanish},
  {Over_English,  Over_Spanish},
  {Score_English, Score_Spanish},
  {Lives_English, Lives_Spanish}
};

//old starter code phrases just in case
// const char Hello_English[] ="Hello";
// const char Hello_Spanish[] ="\xADHola!";
// const char Hello_Portuguese[] = "Ol\xA0";
// const char Hello_French[] ="All\x83";
// const char Goodbye_English[]="Goodbye";
// const char Goodbye_Spanish[]="Adi\xA2s";
// const char Goodbye_Portuguese[] = "Tchau";
// const char Goodbye_French[] = "Au revoir";
// const char Language_English[]="English";
// const char Language_Spanish[]="Espa\xA4ol";
// const char Language_Portuguese[]="Portugu\x88s";
// const char Language_French[]="Fran\x87" "ais";
// const char *Phrases[3][4]={
//   {Hello_English,Hello_Spanish,Hello_Portuguese,Hello_French},
//   {Goodbye_English,Goodbye_Spanish,Goodbye_Portuguese,Goodbye_French},
//   {Language_English,Language_Spanish,Language_Portuguese,Language_French}
// };


// use main1 to observe special characters
int main(void){ // main1
    char l;
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  ST7735_InitPrintf(INITR_BLACKTAB); // INITR_REDTAB for AdaFruit, INITR_BLACKTAB for HiLetGo
  ST7735_FillScreen(0x0000);            // set screen to black

  for(int myPhrase = 0; myPhrase <= 3; myPhrase++){
    for(int myL=0; myL<= 1; myL++){
        //  ST7735_OutString((char *)Phrases[LANGUAGE][myL]);
        // ST7735_OutChar(' ');

        //print language name first just for clarity
        if(myL == English) ST7735_OutString((char *)"ENG: ");
        if(myL == Spanish) ST7735_OutString((char *)"ESP: ");

        //print actual phrase
        ST7735_OutString((char *)Phrases[myPhrase][myL]);
        ST7735_OutChar(13);
    }
  }
  
  Clock_Delay1ms(3000);
  ST7735_FillScreen(0x0000);       // set screen to black
  l = 128;
  while(1){
    Clock_Delay1ms(2000);
    for(int j=0; j < 3; j++){
      for(int i=0;i<16;i++){
        ST7735_SetCursor(7*j+0,i);
        ST7735_OutUDec(l);
        ST7735_OutChar(' ');
        ST7735_OutChar(' ');
        ST7735_SetCursor(7*j+4,i);
        ST7735_OutChar(l);
        l++;
      }
    }
  }
}




// use main2 to observe graphics
int main2(void){ // main2
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  ST7735_InitPrintf(INITR_REDTAB); // INITR_REDTAB for AdaFruit, INITR_BLACKTAB for HiLetGo
  ST7735_FillScreen(ST7735_BLACK);
  ST7735_DrawBitmap(22, 159, PlayerShip0, 18,8); // player ship bottom
  ST7735_DrawBitmap(53, 151, Bunker0, 18,5);
  ST7735_DrawBitmap(42, 159, PlayerShip1, 18,8); // player ship bottom
  ST7735_DrawBitmap(62, 159, PlayerShip2, 18,8); // player ship bottom
  ST7735_DrawBitmap(82, 159, PlayerShip3, 18,8); // player ship bottom
  ST7735_DrawBitmap(0, 9, SmallEnemy10pointA, 16,10);
  ST7735_DrawBitmap(20,9, SmallEnemy10pointB, 16,10);
  ST7735_DrawBitmap(40, 9, SmallEnemy20pointA, 16,10);
  ST7735_DrawBitmap(60, 9, SmallEnemy20pointB, 16,10);
  ST7735_DrawBitmap(80, 9, SmallEnemy30pointA, 16,10);

  for(uint32_t t=500;t>0;t=t-5){
    SmallFont_OutVertical(t,104,6); // top left
    Clock_Delay1ms(50);              // delay 50 msec
  }
  ST7735_FillScreen(0x0000);   // set screen to black
  ST7735_SetCursor(1, 1);
  ST7735_OutString((char *)"GAME OVER");
  ST7735_SetCursor(1, 2);
  ST7735_OutString((char *)"Nice try,");
  ST7735_SetCursor(1, 3);
  ST7735_OutString((char *)"Earthling!");
  ST7735_SetCursor(2, 4);
  ST7735_OutUDec(1234);
  while(1){
  }
}



// use main3 to test switches and LEDs
int main3(void){ // main3
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  Switch_Init(); // initialize switches
  LED_Init(); // initialize LED
  while(1){
    // write code to test switches and LEDs

  }
}



// use main4 to test sound outputs
int main4(void){ uint32_t last=0,now;
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  Switch_Init(); // initialize switches
  LED_Init(); // initialize LED
  Sound_Init();  // initialize sound
  TExaS_Init(ADC0,6,0); // ADC1 channel 6 is PB20, TExaS scope
  __enable_irq();
  while(1){
    now = Switch_In(); // one of your buttons
    if((last == 0)&&(now == 1)){
      Sound_Shoot(); // call one of your sounds
    }
    if((last == 0)&&(now == 2)){
      Sound_Killed(); // call one of your sounds
    }
    if((last == 0)&&(now == 4)){
      Sound_Explosion(); // call one of your sounds
    }
    if((last == 0)&&(now == 8)){
      Sound_Fastinvader1(); // call one of your sounds
    }
    // modify this to test all your sounds
  }
}



// ALL ST7735 OUTPUT MUST OCCUR IN MAIN
int main5(void){ // final main
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  ST7735_InitPrintf(INITR_REDTAB); // INITR_REDTAB for AdaFruit, INITR_BLACKTAB for HiLetGo
  ST7735_FillScreen(ST7735_BLACK);
  Sensor.Init(); // PB18 = ADC1 channel 5, slidepot
  Switch_Init(); // initialize switches
  LED_Init();    // initialize LED
  Sound_Init();  // initialize sound
  TExaS_Init(0,0,&TExaS_LaunchPadLogicPB27PB26); // PB27 and PB26
    // initialize interrupts on TimerG12 at 30 Hz
  
  // initialize all data structures
  __enable_irq();

  while(1){
    // wait for semaphore
    if (Semaphore == 1) {
      Semaphore = 0;  //clear semaphore

      //call drawing functions here
      //ST7735_DrawBitMap(Skater.x, skater.y, Skater.image, 18, 18);
      //loop to draw obstacles.....

    }
  }
}

