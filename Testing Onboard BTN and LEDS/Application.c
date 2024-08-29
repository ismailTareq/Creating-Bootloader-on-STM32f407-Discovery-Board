#include "Board_LED.h"
#include "Board_Buttons.h"


int main(void)
{
	uint32_t btn_state = 0 ;
	LED_Initialize();
	Buttons_Initialize();
	while(1){
		LED_On(0);
		LED_On(1);
		LED_Off(2);
		LED_Off(3);
		btn_state = Buttons_GetState();
		if(1 == btn_state)
		{
			LED_On(2);
			LED_On(3);
			LED_Off(0);
			LED_Off(1);
		}
	}
	return 0;
}