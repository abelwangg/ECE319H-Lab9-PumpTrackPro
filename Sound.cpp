// Sound.cpp
// Runs on MSPM0
// Sound assets in sounds/sounds.h
// your name
// your data 
#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "Sound.h"
#include "sounds/sounds.h"
#include "../inc/DAC5.h"
#include "../inc/Clock.h"

static const uint8_t *volatile SoundPt = nullptr;
static volatile uint32_t SoundCount = 0;

enum : uint32_t{
  SOUND_PRIORITY = 1,
  SOUND_SAMPLE_RATE = 11025,
  DAC_SILENCE = 16
};


void SysTick_IntArm(uint32_t period, uint32_t priority){
  SysTick->CTRL  = 0x00;      // disable during initialization
  SysTick->LOAD  = period-1;  // set reload register
  SCB->SHP[1]    = (SCB->SHP[1]&(~0xC0000000))|(priority<<30);
  SysTick->VAL   = 0;         // clear count, cause reload
  SysTick->CTRL  = 0x07;      // enable SysTick with interrupts
}
// initialize a 11kHz SysTick, however no sound should be started
// initialize any global variables
// Initialize the 5 bit DAC
void Sound_Init(void){
  DAC5_Init();
  DAC5_Out(DAC_SILENCE);
  SoundPt = nullptr;
  SoundCount = 0;
  uint32_t period = Clock_Freq()/SOUND_SAMPLE_RATE;
  if(period == 0){
    period = 1;
  }
  SysTick_IntArm(period, SOUND_PRIORITY);
  SysTick->CTRL = 0x00; // armed, but playback starts in Sound_Start()
}
extern "C" void SysTick_Handler(void);
void SysTick_Handler(void){ // called at 11 kHz
  if(SoundCount && SoundPt){
    DAC5_Out((*SoundPt)>>3); // convert 8-bit sample to 5-bit DAC output
    SoundPt++;
    SoundCount--;
  }else{
    DAC5_Out(DAC_SILENCE);
    SysTick->CTRL = 0x00;
    SoundPt = nullptr;
    SoundCount = 0;
  }
}

//******* Sound_Start ************
// This function does not output to the DAC. 
// Rather, it sets a pointer and counter, and then enables the SysTick interrupt.
// It starts the sound, and the SysTick ISR does the output
// feel free to change the parameters
// Sound should play once and stop
// Input: pt is a pointer to an array of DAC outputs
//        count is the length of the array
// Output: none
// special cases: as you wish to implement
void Sound_Start(const uint8_t *pt, uint32_t count){
  __disable_irq();
  if((pt == nullptr) || (count == 0)){
    SoundPt = nullptr;
    SoundCount = 0;
    DAC5_Out(DAC_SILENCE);
    SysTick->CTRL = 0x00;
  }else{
    SoundPt = pt;
    SoundCount = count;
    SysTick->VAL = 0;
    SysTick->CTRL = 0x07;
  }
  __enable_irq();
}

void Sound_Shoot(void){
  Sound_Start( shoot, 4080);
}
void Sound_Killed(void){
  Sound_Start(invaderkilled, 3377);
}
void Sound_Explosion(void){
  Sound_Start(explosion, 2000);
}

void Sound_Fastinvader1(void){
  Sound_Start(fastinvader1, 982);
}
void Sound_Fastinvader2(void){
  Sound_Start(fastinvader2, 1042);
}
void Sound_Fastinvader3(void){
  Sound_Start(fastinvader3, 1054);
}
void Sound_Fastinvader4(void){
  Sound_Start(fastinvader4, 1098);
}
void Sound_Highpitch(void){
  Sound_Start(highpitch, 1802);
}
