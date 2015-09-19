#include "hw.h"
#include "n1202.h"

static delay()
{
  volatile int i;
  for (i=0; i<500000;i++);
}

void main()
{
  RCC->AHBENR |= RCC_AHBENR_GPIOAEN|RCC_AHBENR_GPIOBEN;
  GPIOA->MODER = TO_GPIO_MODER(A, PINS);
  GPIOA->OTYPER = TO_GPIO_OTYPER(A, PINS);
  GPIOA->OSPEEDR = TO_GPIO_OSPEEDR(A, PINS);  
  GPIOA->PUPDR = TO_GPIO_PUPDR(A, PINS);
  GPIOA->ODR = TO_GPIO_ODR(A, PINS);
  GPIOA->AFR[0] = TO_GPIO_AFRL(A, PINS);
  GPIOA->AFR[1] = TO_GPIO_AFRH(A, PINS);

  RCC->CR |= RCC_CR_HSEON;
  while ( (RCC->CR & RCC_CR_HSERDY) == 0)
    ; /* BLANK */

  FLASH->ACR |= FLASH_ACR_LATENCY|FLASH_ACR_PRFTBE;// 1 wait state + prefetch

  RCC->CFGR = RCC_CFGR_PLLMUL_2|RCC_CFGR_PLLSRC ; // pll x6  HSE
  RCC->CR |= RCC_CR_PLLON;
  while((RCC->CR & RCC_CR_PLLRDY) == 0)
    ; /* BLANK */
  RCC->CFGR |= RCC_CFGR_SW_1; // switch to pll
  
  GPIO_SET(LCD_PWR);
  LcdInit();
  LcdClear();
  LcdChr(X_POSITION*1+ Y_POSITION*1 + MUL3 + 5, "Hello");
  LcdChr(X_POSITION*1+ Y_POSITION*4 + MUL3 + 5, "world");
}
