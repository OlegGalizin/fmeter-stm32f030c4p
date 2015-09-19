#include <string.h>
#include "event.h"

uint16_t Event;     // Generated event
volatile uint8_t EvCounter; // Press delay counter

MenuFunction_t CurrentFunction = NULL; // Current function
MenuFunction_t PrevFunc = NULL;

volatile uint16_t EventQueue; // Generated event
static uint16_t   PrevKey; // Previous keys pressed
static uint8_t    RealizeCounter; // Realize delay counter


#define TIMER_NUM 17    /* It is timer that is used to init event check */

#define _TIM_IRQ(NUM, TAIL) TIM##NUM##TAIL
#define TIM_IRQ(NUM, TAIL) _TIM_IRQ(NUM, TAIL)



//#define TIMER_NUM(ABC) _TIMER_NUM(ABC)

#define _TIM_EVENT(PREFIX, NUMBER) PREFIX##NUMBER
#define TIM_EVENT1(TIM,NUM) _TIM_EVENT(TIM, NUM)
#define TIM_EVENT TIM_EVENT1(TIM, TIMER_NUM)
#define EVENT_IRQ(TIMER_N, SECOND)  TIM##TIMER_N##_IRQn

void EventInit(void)
{
  /* Switch interrupt soource to PB. PA no need change - it default */
  SYSCFG->EXTICR[0] = SYSCFG_EXTICR1_EXTI1_PB; // PB1
//  SYSCFG->EXTICR[1] |= SYSCFG_EXTICR2_EXTI6_PB|SYSCFG_EXTICR2_EXTI7_PB;
//  SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI8_PB;
   
  NVIC_EnableIRQ(TIM_IRQ(TIMER_NUM,_IRQn)); /* Timer IRQ */

  /* External input IRQ */
//  NVIC_EnableIRQ(EXTI15_10_IRQn);
//  NVIC_SetPriority(EXTI15_10_IRQn, 14); /* Low priority */
  NVIC_EnableIRQ(EXTI0_1_IRQn);
  NVIC_SetPriority(EXTI0_1_IRQn, 14); /* Low priority */
  NVIC_EnableIRQ(EXTI4_15_IRQn);
  NVIC_SetPriority(EXTI4_15_IRQn, 14); /* Low priority */
//  NVIC_EnableIRQ(EXTI0_IRQn);
//  NVIC_SetPriority(EXTI0_IRQn, 14); /* Low priority */
//  NVIC_EnableIRQ(EXTI1_IRQn);
//  NVIC_SetPriority(EXTI1_IRQn, 14); /* Low priority */
//  NVIC_EnableIRQ(EXTI2_IRQn);
//  NVIC_SetPriority(EXTI2_IRQn, 14); /* Low priority */

  /* Check interval */
  TIM_EVENT->DIER |= TIM_DIER_UIE; /* enable update IRQ */
  TIM_EVENT->PSC = MAIN_F/1000000 - 1; /* 1 MHz  after prescaler*/
  TIM_EVENT->ARR = 5000-1; /* 1MHz / 5000 = 200Hz  - 5mSec */
//  TIM_EVENT->CR1 |= TIM_CR1_CEN;    /* It should not be started */ 

  EXTI->IMR |= KEY_MASK_SYS; /* Ext interrupt mask */
  EXTI->FTSR |= KEY_MASK_SYS; /* failing trigger */
  EXTI->PR |= KEY_MASK_SYS; /* Clear pending register */
}

/* !!!! EXT IRQ vecrtors should be redefined to this function in startup file */
void ClearExtI(void)  // It is IRQ handler. It defined in table of vectors in ASM
{
  EXTI->IMR &= ~KEY_MASK_SYS; /* Disable external event interrupts */
  EXTI->PR |= KEY_MASK_SYS; /* Clear pending register */
  TIM_EVENT->CR1 |= TIM_CR1_CEN; /* Run timer if it stoped */
}


void EventKeys(void)
{
  uint16_t Key;
  
  if ( (EventQueue & KEY_MASK_SYS) != 0 ) /* Previous event hasn't been handled. The key hase highiest  priority */
    return;
  if (CurrentFunction != PrevFunc ) /* The function was changed */
    return;

//  Key = ((~GPIOB->IDR) & KEY_MASK_SYS); /* Read the port */
  Key = ((~GPIO_PORT(BUT3)->IDR) & GPIO_BIT(BUT3)) | ((~GPIO_PORT(BUT1)->IDR) & (GPIO_BIT(BUT2)|GPIO_BIT(BUT1))); /* Read the port */

  if ( Key == 0 ) // All keys was released
  {
    if ( PrevKey == 0 ) // no any key was pressed before
    {
      EvCounter = 0;
      goto StopTimer;
    }
    if ( EvCounter > KEY_PRESSED_VALUE )
    {
      RealizeCounter++; // increase timer counter
      if ( RealizeCounter > KEY_REALIZE_VALUE ) // expired realise timeout
      {
        if ( EvCounter != 0xFF ) /* There is no switch to new function. New function should not get previos function event */
        {
          EventQueue = EV_KEY_REALIZED | PrevKey; /* Realized event - the last event */
        }
        EvCounter = 0; // Reset interval timer value
        PrevKey = 0;   // Reset key pressed value
        RealizeCounter = 0;  // Reset realise counter
        goto StopTimer;
      }
    }
    else
    {
      EvCounter = 0; // Reset interval timer value
      PrevKey = 0;   // Reset key pressed value
      RealizeCounter = 0;  // Reset realise counter
      goto StopTimer;
    }
    return;

StopTimer:
    EXTI->IMR |= KEY_MASK_SYS; /* enable event interrupts */
    TIM_EVENT->CR1 &= ~TIM_CR1_CEN; /* Stop the timer */
    return;
  }
  else // Some keys are pressed
  {
    RealizeCounter = 0; //reset realise delay
   
    if ( EvCounter == 0xFF ) /* Locked - new function has been set */
      return; 
    
    if ( Key & (~PrevKey) ) //there are some new keys
    {
      PrevKey |= Key;       // adding the new keys
      if ( EvCounter == 0 )
      {
      /* Generate KEY TOUCH event */
        EventQueue = EV_KEY_TOUCH + PrevKey;
      }
      else if ( EvCounter > KEY_LONG_VALUE ) // Delay after first press is not long
        EventQueue = EV_KEY_LONG | PrevKey; //generate key press event
    }
    else // the same keys are pressed
    {
      if ( EvCounter == KEY_PRESSED_VALUE ) // Delay after first press is not long
        EventQueue = EV_KEY_PRESSED | PrevKey; //generate key press event
      else if ( EvCounter == KEY_LONG_VALUE )  // Long press timeout has expired
      {
        EventQueue = EV_KEY_LONG | PrevKey; // Generate Long press event
      }
      else if ( EvCounter == KEY_REPEATE_VALUE ) // After long press the key is stil pressed
      {
        EventQueue = EV_KEY_REPEATE | PrevKey; // Generate repeate press event
        EvCounter = KEY_LONG_VALUE; // Reset time counter for next delay
      }
      EvCounter++; // Delay counter increasing
    }
  }
}

void EventCheck(void)
{
      if ( CurrentFunction != PrevFunc ) // Function was changed
      {
        Event = EV_FUNC_FIRST;       // Generate FUNC_FIRST event
        PrevFunc = CurrentFunction;      // Save the function
    		__disable_irq();
        if ( EvCounter )             /// Some keys are stil pressed
          EvCounter = 0xFF;            // Lock any key events until all keys are not realized
    		__enable_irq();
      }
      else
      {
    		__disable_irq();
        Event = EventQueue;          // Read event thar was generated in interrupt handlers
        EventQueue = 0;              // The interrupt handlers can write new value
    		__enable_irq();
      }
      CurrentFunction();      // Run the current menu function
      EventIdle();
}

#define _EVENT_HANDLE_NAME(NUM, TAIL) TIM##NUM##TAIL
#define EVENT_HANDLE_NAME(TIMER, SECOND)  _EVENT_HANDLE_NAME(TIMER,SECOND)
void EVENT_HANDLE_NAME(TIMER_NUM,_IRQHandler)(void)
{
  TIM_EVENT->SR = 0; /* Clear pending flag */
  EXTI->PR = KEY_MASK_SYS; /* Clear prnding key interrupts */
  EventKeys();
}


#if defined(MENU_DEMO)

#include "n1202.h"

void SystemInit()
{
}


uint8_t MenuCounter;

void MenuSelected(void);
void Contrast(void);

void MainMenu()
{
  if ( Event == 0 )
    return;

  if ( Event == EV_FUNC_FIRST )
  {
    MenuCounter = 0;
    LcdClear();
    goto RedrawMenu;
  }

  if ( (Event & EV_MASK) == EV_KEY_PRESSED )
  {
    switch (Event & KEY_MASK)
    {
      case KEY_UP:
        if ( MenuCounter == 0 )
          MenuCounter = 6;
        else
          MenuCounter = MenuCounter - 1;
        break;
      case KEY_DOWN:
        if ( MenuCounter == 6 )
          MenuCounter = 0;
        else
          MenuCounter = MenuCounter + 1;
        break;
      case KEY_ENTER:
        switch(MenuCounter)
        {
          case 4: // ��������� �������������
            CurrentFunc(Contrast);
            break;
          case 5:
//            GPIO_TOGGLE(LCD_LED);
            break;
          case 6:
//            GPIO_RESET(PWR_ON);
            break;
          default:
            CurrentFunc(MenuSelected;)
            return;
        }
    }
  }
  else
    return;
RedrawMenu:
  LcdChr(X_POSITION*0+Y_POSITION*1+16 + (0==MenuCounter)*INVERSE, "Menu1");
  LcdChr(X_POSITION*0+Y_POSITION*2+16 + (1==MenuCounter)*INVERSE, "Menu2");
#if defined(FLOAT_DEMO)
  LcdChr(X_POSITION*0+Y_POSITION*3+16 + (2==MenuCounter)*INVERSE, "Float demo");
#else
  LcdChr(X_POSITION*0+Y_POSITION*3+16 + (2==MenuCounter)*INVERSE, "Menu3");
#endif  
  LcdChr(X_POSITION*0+Y_POSITION*4+16 + (3==MenuCounter)*INVERSE, "Menu4");
  LcdChr(X_POSITION*0+Y_POSITION*5+16 + (4==MenuCounter)*INVERSE, "Contrast");
  LcdChr(X_POSITION*0+Y_POSITION*6+16 + (5==MenuCounter)*INVERSE, "Light");
  LcdChr(X_POSITION*0+Y_POSITION*7+16 + (6==MenuCounter)*INVERSE, "Off");
}

void MenuSelected(void)
{
  if ( (Event&EV_MASK) == EV_FUNC_FIRST)
  {
    LcdChr(X_POSITION*0+Y_POSITION*1+14,  "Press key     ");
    LcdChr(X_POSITION*0+Y_POSITION*2+14,  "ENTER for a   ");
    LcdChr(X_POSITION*0+Y_POSITION*3+14,  "long time to  ");
    LcdChr(X_POSITION*0+Y_POSITION*4+14,  "return main   ");
    LcdChr(X_POSITION*0+Y_POSITION*5+14,  "menu");
    return;
  }
  
  if ( Event == (EV_KEY_LONG + KEY_ENTER) )
  { /* Return back */
    CurrentFunc(MainMenu);
  } 
}

void Contrast(void)
{
  if (Event == EV_FUNC_FIRST)
  {
    MenuCounter = 16;
    LcdClear(); /* Clear screen */
    goto redraw;
  }
  if ( (Event & EV_MASK) == EV_KEY_PRESSED )
  {
    switch (Event & KEY_MASK)
    {
      case KEY_UP:
        if ( MenuCounter < 31 )
          MenuCounter++;
        break;
      case KEY_DOWN:
        if ( MenuCounter > 0 )
          MenuCounter--;
        break;
      case KEY_ENTER:
         CurrentFunc(MainMenu;)
         return;
    }
redraw:
    LcdContrast ( MenuCounter );
    {
      char Buf[2];
      uint8_t Counter = MenuCounter;
      
      Buf[0] = Counter/10 + '0';
      Counter = Counter%10;
      Buf[1] = Counter + '0';
      LcdChr(MUL4+Y_POSITION*2 + X_POSITION * 4 + 2, Buf);
    }
  }
}

int LongCounter;
void EventIdle(void) {};

void StartFunction(void)
{
  char* OutString = "";
  char  KeyArray[5];
  
  if (Event == 0)
    return;
  
  switch ( Event & EV_MASK )
  {
    case EV_KEY_TOUCH:
      OutString = "Touch";
//      return;
      break;
    case EV_KEY_PRESSED:
      OutString = "Press";
      break;
    case EV_KEY_LONG:
      OutString = "Long";
      if ( Event & KEY_ENTER )
	  {
	    if (LongCounter > 3 )
         CurrentFunc(MainMenu);
        LongCounter++;
	  }
      break;
    case EV_KEY_REPEATE:
      OutString = "Repeate";
      break;
    case EV_KEY_REALIZED:
      OutString = "Realize";
      break;
    case EV_FUNC_FIRST:
      OutString = "First";
  }
    
  LcdChr(Y_POSITION*(MenuCounter%8+1) + 7, OutString);
  Event &= KEY_MASK;
  KeyArray[0] = Event & KEY1 ? '*':'-';
  KeyArray[1] = Event & KEY2 ? '*':'-';
  KeyArray[2] = Event & KEY3 ? '*':'-';
//  KeyArray[3] = Event & KEY4 ? '*':'-';
//  KeyArray[4] = Event & KEY5 ? '*':'-';
  LcdChr(Y_POSITION*(MenuCounter%8+1) + X_POSITION * 7 + 5, KeyArray);

  {
    uint8_t Counter = MenuCounter;
    KeyArray[0] = Counter/100 + '0';
    Counter = Counter%100;
    KeyArray[1] = Counter/10 + '0';
    Counter = Counter%10;
    KeyArray[2] = Counter + '0';
    LcdChr(Y_POSITION*(MenuCounter%8+1) + X_POSITION * 12 + 3, KeyArray);
  }
  MenuCounter++;
}




int main()
{
  CurrentFunc(StartFunction);

  RCC->AHBENR |= RCC_AHBENR_GPIOAEN|RCC_AHBENR_GPIOBEN;
  RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN|RCC_APB2ENR_TIM17EN|RCC_APB2ENR_TIM1EN|RCC_APB2ENR_DBGMCUEN;
  RCC->APB1ENR = RCC_APB1ENR_TIM3EN;
  GPIOA->MODER = TO_GPIO_MODER(A, PINS);
  GPIOA->OTYPER = TO_GPIO_OTYPER(A, PINS);
  GPIOA->OSPEEDR = TO_GPIO_OSPEEDR(A, PINS);  
  GPIOA->PUPDR = TO_GPIO_PUPDR(A, PINS);
  GPIOA->ODR = TO_GPIO_ODR(A, PINS);
//  GPIOA->AFR[0] = TO_GPIO_AFRL(A, PINS);
//  GPIOA->AFR[1] = TO_GPIO_AFRH(A, PINS);
  GPIOB->MODER = TO_GPIO_MODER(B, PINS);
  GPIOB->PUPDR = TO_GPIO_PUPDR(B, PINS);

  
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
  EventInit();
   
  do
  {
  	EventCheck();
//    __WFI(); // It decreased power but turn off the SWD!!!!
  }
  while(1);
}
#endif /*MENU_DEMO*/

