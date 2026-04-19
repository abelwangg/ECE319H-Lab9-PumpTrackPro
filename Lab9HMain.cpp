// Lab9HMain.cpp
// Runs on MSPM0G3507
// Lab 9 ECE319H
// Jingyuan (Abel) Wang, Geyang (Alex) Xu
// April 2026

#include <stdio.h>
#include <stdint.h>

#include <stdlib.h> //needed ts for absolute value

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

//custom init function??
void Random_Init(uint32_t seed){
  M = seed;
}

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

// uint32_t Score = 0;
uint32_t Lives = 3;
// uint8_t Semaphore = 0; // Flag to trigger LCD update


//language stuff
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

  void ST7735_OutPhrase(phrase_t message){
    // Uses the global myLanguage variable to pick the correct column
    ST7735_OutString((char *)Phrases[message][myLanguage]);
  }


//game initialization stuff
  //game states
  typedef enum {MENU, PLAYING, GAMEOVER} state_t;
  state_t GameState = MENU;

  //obstacle types
  #define LOWER 0  //requires ollie
  #define UPPER 1  //requires crouch
  #define WALL  2  //requires track switch

  struct Obstacle {
    int32_t x;
    int32_t y;            // 70 (Top Track) or 130 (Bottom Track)
    int32_t old_x;
    uint8_t type;         // LOWER, UPPER, or WALL
    const uint16_t *image; 
    status_t life;        // dead or alive
  };

  Obstacle ActiveObs[3];  //max 3 obstacles on screen at once

  uint32_t Score = 0;
  uint8_t Semaphore = 0;
  int32_t skater_x = 20;
  int32_t skater_y = 130;
  int32_t old_skater_y = 130;



// ISR game engine runs at 30Hz
void TIMG12_IRQHandler(void){uint32_t pos,msg;
  if((TIMG12->CPU_INT.IIDX) == 1){ // this will acknowledge timer interrupt
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)

//this ISR will sample slide-pot to move skater left/right, check buttons for Ollie/Duck commands, 
//and move obstacles down the screen

// game engine goes here
    uint32_t adc_val = Sensor.In(); 
    uint32_t buttons = Switch_In();

    //negative logic, if the bit is 0, the button is pressed.
    //use bitwise AND (&) with a mask shifted to the correct pin number.
    bool isJumping  = (buttons & (1 << 28)) == 0; 
    bool isDucking  = (buttons & (1 << 27)) == 0;
    bool isPausing  = (buttons & (1 << 17)) == 0; //used for language/start

    //obstacles cooldown timer
    static uint32_t SpawnCooldown = 0;
    
    if (GameState == MENU) {
      //toggle language or start game
      if (isPausing) {
        myLanguage = (myLanguage == English) ? Spanish : English;
      }
      if (isJumping) {
        GameState = PLAYING;
        Score = 0;
        //initialize obstacles to dead
        for (int i=0; i<3; i++) {
          ActiveObs[i].life = dead;
        }
      }
    } 
    
    else if (GameState == PLAYING) {
      old_skater_y = skater_y;

      //physics tuned for quick response (like 130mm Landyachtz Polar Bear trucks)
      if(adc_val < 2048) { skater_y = 130; } //snap to Bottom
      else { skater_y = 70; }                //snap to Top

      //swap skater sprite
      if(isJumping) Skater.image = SkaterJumpImage;
      else if(isDucking) Skater.image = SkaterDuckImage;
      else Skater.image = SkaterNormalImage;

      //MOVE OBSTACLES and check collisions logic bs
      for(int i = 0; i < 3; i++){
        if(ActiveObs[i].life == alive){
          ActiveObs[i].old_x = ActiveObs[i].x;
          ActiveObs[i].x -= 3; // Scroll speed to the left

          //if goes off screen, kill it
          if(ActiveObs[i].x < -16) {
              ActiveObs[i].life = dead;
          }

          // COLLISION DETECTION
          // 1. Are they on the exact same track?
          // 2. Are they overlapping horizontally? (Distance < 12 pixels)
          if((skater_y == ActiveObs[i].y) && (abs(skater_x - ActiveObs[i].x) < 12)){
              
            if(ActiveObs[i].type == WALL) {
              GameState = GAMEOVER; //total wipeout
              Sound_GameOver(); //play sound
            }

            else if(ActiveObs[i].type == LOWER && !isJumping) {
              GameState = GAMEOVER; //clipped a cone
              Sound_GameOver();
            }

            else if(ActiveObs[i].type == UPPER && !isDucking) {
              GameState = GAMEOVER; //hit your head
              Sound_GameOver();
            }
          }
        }
      }

      //decrease cooldown timer every frame
      if(SpawnCooldown > 0) {
        SpawnCooldown--;
      }
      //only allow spawning of obstacles if cooldown is finished
      else {
        //randomly spawn new obstacles
        if (Random(100) < 25) {  //25% chance every frame?
          for (int i = 0; i < 3; i++) {
            if (ActiveObs[i].life == dead) {
              ActiveObs[i].life = alive;
              ActiveObs[i].x = 128; //spawn on right edge

              //fifty fifty chance for top or bottom track
              ActiveObs[i].y = (Random(2) == 0) ? 70 : 130;

              //pick type: 0 = lower, 1 = upper, 2 = wall
              ActiveObs[i].type = Random(3);

              //stuff
              if (ActiveObs[i].type == LOWER) ActiveObs[i].image = ConeImage;
              else if (ActiveObs[i].type == UPPER) ActiveObs[i].image = SignImage;
              else ActiveObs[i].image = WallImage;

              //RESET COOLDOWN
              //wait at least 40 frames (1.3 s) before spawning next one
              //could lower this as score gets higher to increase difficulty?
              SpawnCooldown = 40;

              break;  //only spawn one per frame
            }
          }
        }
      }

      //increment score
      Score++;
    }
    
    else if (GameState == GAMEOVER) {
      if (isPausing) {
        GameState = MENU; //go back to start screen
      }
    }

    //set semaphore (basically tell main to draw)
    Semaphore = 1;

    //NOOOOOOOOO LCD OUTPUT IN INTERRUPT SERVICE ROUTINES

    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
  }
}




uint8_t TExaS_LaunchPadLogicPB27PB26(void){
  return (0x80|((GPIOB->DOUT31_0>>26)&0x03));
}


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
int main1(void){ // main1
    char l;
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  ST7735_InitPrintf(INITR_BLACKTAB); // INITR_REDTAB for AdaFruit, INITR_BLACKTAB for HiLetGo
  ST7735_FillScreen(0x0000);            // set screen to black

  while(1){
    // Reset cursor to top-left corner
    ST7735_SetCursor(0, 0); 
    
    // Test English Output
    myLanguage = English;
    ST7735_OutString((char *)"--- English ---\n");
    ST7735_OutPhrase(START_GAME); ST7735_OutChar('\n');
    ST7735_OutPhrase(GAME_OVER);  ST7735_OutChar('\n');
    ST7735_OutPhrase(SCORE);      ST7735_OutString((char *)"0\n");
    ST7735_OutPhrase(LIVES);      ST7735_OutString((char *)"3\n\n");

    // Test Spanish Output
    myLanguage = Spanish;
    ST7735_OutString((char *)"--- Spanish ---\n");
    ST7735_OutPhrase(START_GAME); ST7735_OutChar('\n');
    ST7735_OutPhrase(GAME_OVER);  ST7735_OutChar('\n');
    ST7735_OutPhrase(SCORE);      ST7735_OutString((char *)"0\n");
    ST7735_OutPhrase(LIVES);      ST7735_OutString((char *)"3\n");
    
    // Delay to make it readable, then clear screen and repeat
    Clock_Delay(80000000); // Approx 1-2 seconds
    ST7735_FillScreen(ST7735_BLACK);
  }

  // for(int myPhrase = 0; myPhrase <= 3; myPhrase++){
  //   for(int myL=0; myL<= 1; myL++){
  //       //  ST7735_OutString((char *)Phrases[LANGUAGE][myL]);
  //       // ST7735_OutChar(' ');

  //       //print language name first just for clarity
  //       if(myL == English) ST7735_OutString((char *)"ENG: ");
  //       if(myL == Spanish) ST7735_OutString((char *)"ESP: ");

  //       //print actual phrase
  //       ST7735_OutString((char *)Phrases[myPhrase][myL]);
  //       ST7735_OutChar(13);
  //   }
  // }
  
  // Clock_Delay1ms(3000);
  // ST7735_FillScreen(0x0000);       // set screen to black
  // l = 128;
  // while(1){
  //   Clock_Delay1ms(2000);
  //   for(int j=0; j < 3; j++){
  //     for(int i=0;i<16;i++){
  //       ST7735_SetCursor(7*j+0,i);
  //       ST7735_OutUDec(l);
  //       ST7735_OutChar(' ');
  //       ST7735_OutChar(' ');
  //       ST7735_SetCursor(7*j+4,i);
  //       ST7735_OutChar(l);
  //       l++;
  //     }
  //   }
  // }

}




// use main2 to observe graphics
int main2(void){ // main2
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  ST7735_InitPrintf(INITR_BLACKTAB); // INITR_REDTAB for AdaFruit, INITR_BLACKTAB for HiLetGo
  ST7735_FillScreen(ST7735_BLACK);

  // Test the three Skater sprites
  // Syntax: X, Y, Pointer to Array, Width, Height
  // Remember: Y is the BOTTOM-left corner of the image in this specific library!
  
  // Draw Normal Skater on the left
  ST7735_DrawBitmap(10, 100, SkaterNormalImage, 16, 16); 
  
  // Draw Jumping Skater in the middle
  ST7735_DrawBitmap(50, 100, SkaterJumpImage, 16, 16); 
  
  // Draw Ducking Skater on the right
  ST7735_DrawBitmap(90, 100, SkaterDuckImage, 16, 16); 

  while(1){
    // Do nothing, just leave the images on the screen
  }


  // ST7735_DrawBitmap(22, 159, PlayerShip0, 18,8); // player ship bottom
  // ST7735_DrawBitmap(53, 151, Bunker0, 18,5);
  // ST7735_DrawBitmap(42, 159, PlayerShip1, 18,8); // player ship bottom
  // ST7735_DrawBitmap(62, 159, PlayerShip2, 18,8); // player ship bottom
  // ST7735_DrawBitmap(82, 159, PlayerShip3, 18,8); // player ship bottom
  // ST7735_DrawBitmap(0, 9, SmallEnemy10pointA, 16,10);
  // ST7735_DrawBitmap(20,9, SmallEnemy10pointB, 16,10);
  // ST7735_DrawBitmap(40, 9, SmallEnemy20pointA, 16,10);
  // ST7735_DrawBitmap(60, 9, SmallEnemy20pointB, 16,10);
  // ST7735_DrawBitmap(80, 9, SmallEnemy30pointA, 16,10);

  // for(uint32_t t=500;t>0;t=t-5){
  //   SmallFont_OutVertical(t,104,6); // top left
  //   Clock_Delay1ms(50);              // delay 50 msec
  // }
  // ST7735_FillScreen(0x0000);   // set screen to black
  // ST7735_SetCursor(1, 1);
  // ST7735_OutString((char *)"GAME OVER");
  // ST7735_SetCursor(1, 2);
  // ST7735_OutString((char *)"Nice try,");
  // ST7735_SetCursor(1, 3);
  // ST7735_OutString((char *)"Earthling!");
  // ST7735_SetCursor(2, 4);
  // ST7735_OutUDec(1234);
  // while(1){
  // }
}



// use main3 to test switches and LEDs
int main3(void){ // main3
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();

  Switch_Init(); //initializes PA28, PA27, PA17 as inputs
  Sensor.Init();

  ST7735_InitPrintf(INITR_BLACKTAB); 
  ST7735_FillScreen(ST7735_BLACK);
  __enable_irq();

  uint32_t buttons;
  uint32_t adc_val;

  // The skater stays fixed on the left side of the screen
  int32_t skater_x = 20; 
  int32_t skater_y = 130;     
  int32_t old_skater_y = 130;

  int32_t cursor_y = 130;
  int32_t old_cursor_y = 130;

  // Draw two white lines to represent  "Two Tracks"
  ST7735_DrawFastHLine(0, 70, 128, ST7735_WHITE);
  ST7735_DrawFastHLine(0, 130, 128, ST7735_WHITE);

  while(1){
    // 1. Read Inputs
    buttons = Switch_In(); 
    adc_val = Sensor.In(); // Returns 0 to 4095
    
    // 2. Button Logic (Negative Logic)
    bool isJumping = (buttons & (1 << 28)) == 0; 
    bool isDucking = (buttons & (1 << 27)) == 0;
    
    old_skater_y = skater_y;
    old_cursor_y = cursor_y;

    // 1. Smooth Cursor Math (Flipped)
    // 130 (Bottom) - 70 (Top) = 60 pixels of total range
    cursor_y = 130 - ((120 * adc_val) >> 12); 

    // 2. Discrete Skater Snapping
    // 2048 is exactly half of the 0-4095 ADC range
    if(adc_val < 2048) {
      skater_y = 130; // Snap to bottom track
    } else {
      skater_y = 70;  // Snap to top track
    }

    // 3. Erase old smooth cursor
    if(cursor_y != old_cursor_y){
      // Erase a 4x4 bounding box where the old red cursor was
      ST7735_FillRect(5, old_cursor_y - 3, 4, 4, ST7735_BLACK);
    }

    // 4. Erase old skater AND patch the track
    if(skater_y != old_skater_y || isJumping || isDucking){
      // Erase the old 16x16 skater
      ST7735_FillRect(skater_x, old_skater_y - 15, 16, 16, ST7735_BLACK);
      
      // Patch the white track line that we just accidentally erased
      if(old_skater_y == 70)  ST7735_DrawFastHLine(skater_x, 70, 16, ST7735_WHITE);
      if(old_skater_y == 130) ST7735_DrawFastHLine(skater_x, 130, 16, ST7735_WHITE);
    }

    // 5. Draw the new smooth cursor (a 4x4 Red Square on the left edge)
    ST7735_FillRect(5, cursor_y - 3, 4, 4, ST7735_RED);

    // 6. Draw the new snapped skater
    if (isJumping) {
      ST7735_DrawBitmap(skater_x, skater_y, SkaterJumpImage, 16, 16);
    } else if (isDucking) {
      ST7735_DrawBitmap(skater_x, skater_y, SkaterDuckImage, 16, 16);
    } else {
      ST7735_DrawBitmap(skater_x, skater_y, SkaterNormalImage, 16, 16);
    }

    Clock_Delay1ms(33);
  }
}



// use main4 to test sound outputs
int main4(void){
  constexpr uint32_t JumpButtonMask  = (1u<<28); // PA28, negative logic
  constexpr uint32_t DuckButtonMask  = (1u<<27); // PA27, negative logic
  constexpr uint32_t PauseButtonMask = (1u<<17); // PA17, negative logic

  constexpr uint32_t SoundButtonMask = JumpButtonMask|DuckButtonMask|PauseButtonMask;
  uint32_t last, now, newPresses;
  
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  Switch_Init(); // initialize switches
  LED_Init(); // initialize LED
  Sound_Init();  // initialize sound
  TExaS_Init(ADC0,6,0); // optional scope/debug
  last = Switch_In() & SoundButtonMask;
  __enable_irq();

  // Immediate startup chirp verifies the DAC/speaker path even before buttons are tested.
  Sound_Ollie();

  while(1){
    now = Switch_In() & SoundButtonMask;
    newPresses = last & (~now); // negative logic: 1->0 means newly pressed

    if(newPresses & JumpButtonMask){
      Sound_Ollie();
    }
    if(newPresses & DuckButtonMask){
      Sound_Crouch();
    }
    if(newPresses & PauseButtonMask){
      Sound_GameOver();
    }
    
    last = now;
    Clock_Delay1ms(1);
  }
}



// ALL ST7735 OUTPUT MUST OCCUR IN MAIN
int main(void){ // final main
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
  TimerG12_IntArm(2666666, 2);
  
  // initialize all data structures
  __enable_irq();

  //human randomness seed trick lmao
  ST7735_SetCursor(0, 2);
  ST7735_OutPhrase(START_GAME);

  //wait in this loop until jump button (PA28) is pressed
  while((Switch_In() & (1 << 28)) != 0){ 
      // Do nothing, just spin and wait
  }

  //grab hardware timer value!!!!!!!
  Random_Init(TIMG12->COUNTERREGS.CTR); 
  
  GameState = PLAYING; //start game


  while(1){
    // wait for semaphore
    if (Semaphore == 1) {
      Semaphore = 0;  //clear semaphore

      if(GameState == MENU) {
        ST7735_SetCursor(0, 2);
        ST7735_OutPhrase(START_GAME); // Uses your 2D array!
        ST7735_SetCursor(0, 4);
        ST7735_OutString((char*)"Pause Btn = Language");
      } 
      else if(GameState == PLAYING) {
        // 1. Erase old Skater
        if(skater_y != old_skater_y || Skater.image != SkaterNormalImage){
            ST7735_FillRect(skater_x, old_skater_y - 15, 16, 16, ST7735_BLACK);
            if(old_skater_y == 70) ST7735_DrawFastHLine(skater_x, 70, 16, ST7735_WHITE);
            if(old_skater_y == 130) ST7735_DrawFastHLine(skater_x, 130, 16, ST7735_WHITE);
        }

        // 2. Erase and Redraw Obstacles
        for(int i = 0; i < 3; i++){
            if(ActiveObs[i].life == alive){
                // Erase trail
                ST7735_FillRect(ActiveObs[i].old_x, ActiveObs[i].y - 15, 3, 16, ST7735_BLACK);
                ST7735_DrawBitmap(ActiveObs[i].x, ActiveObs[i].y, ActiveObs[i].image, 16, 16);
            }
        }

        // 3. Draw Skater and Tracks
        ST7735_DrawFastHLine(0, 70, 128, ST7735_WHITE);
        ST7735_DrawFastHLine(0, 130, 128, ST7735_WHITE);
        ST7735_DrawBitmap(skater_x, skater_y, Skater.image, 16, 16);

        // 4. Update Score
        ST7735_SetCursor(0, 0);
        ST7735_OutUDec(Score);
      }
      else if(GameState == GAMEOVER) {
        ST7735_FillScreen(ST7735_BLACK);
        ST7735_SetCursor(2, 4);
        ST7735_OutPhrase(GAME_OVER);
        ST7735_SetCursor(2, 6);
        ST7735_OutPhrase(SCORE);
        ST7735_OutUDec(Score);
        
        Clock_Delay1ms(2000); // Prevent accidental instant restart
        
        ST7735_SetCursor(0, 8);
        ST7735_OutString((char*)"Pause Btn to Menu");
      }
    }
  }
}

//ts pmo