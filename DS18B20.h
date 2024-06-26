/* current setbacks :
 * read function not solid
 */
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include"stm32f3xx.h"
#include"stm32f3xx_hal.h"
#include<stdlib.h>
#include<string.h>


//#define BAUDRATE 9600

// Scratchpad locations
#define TEMP_LSB        0
#define TEMP_MSB        1
#define HIGH_ALARM_TEMP 2
#define LOW_ALARM_TEMP  3
#define CONFIGURATION   4
#define INTERNAL_BYTE   5
#define COUNT_REMAIN    6
#define COUNT_PER_C     7
#define SCRATCHPAD_CRC  8

// OneWire commands
#define STARTCONVO      0x44  // Tells device to take a temperature reading and put it on the scratchpad
#define COPYSCRATCH     0x48  // Copy scratchpad to EEPROM
#define READSCRATCH     0xBE  // Read from scratchpad
#define WRITESCRATCH    0x4E  // Write to scratchpad
#define RECALLSCRATCH   0xB8  // Recall from EEPROM to scratchpad
#define READPOWERSUPPLY 0xB4  // Determine if device needs parasite power
#define ALARMSEARCH     0xEC  // Query bus for devices with an alarm condition

// Device resolution
#define TEMP_9_BIT  0x1F //  9 bit
#define TEMP_10_BIT 0x3F // 10 bit
#define TEMP_11_BIT 0x5F // 11 bit
#define TEMP_12_BIT 0x7F // 12 bit


#define MAX_CONVERSION_TIMEOUT 750
//
//void MX_GPIO_DeInit(GPIO_TypeDef* , uint16_t ) ;
//void BUS_REINIT(GPIO_TypeDef* , uint16_t , uint8_t );

typedef struct{
	GPIO_TypeDef* GPIOx;
	int16_t Pin;
	TIM_HandleTypeDef htim1;
} Wire;


UART_HandleTypeDef * huart;

void MX_GPIO_DeInit(Wire*);
void delay (Wire*, uint32_t);
uint8_t PINSTATE(Wire*);
void BUS_REINIT(Wire*,uint8_t);
void WIRE_AS_OUTPUT(Wire *);
void WIRE_AS_INPUT(Wire *);
int8_t Wire_INIT(Wire *);

void setuart(UART_HandleTypeDef * huartx){
	huart = huartx;
}

char newline[]  = "\r\n";
char zer[] = "0";
char one[] = "1";


void init_wire(Wire * wire, GPIO_TypeDef* gpiox, int16_t pin, TIM_HandleTypeDef htim1) {
  memcpy(&(wire->GPIOx), &gpiox, sizeof(GPIO_TypeDef*));
  memcpy(&(wire->Pin), &pin, sizeof(int16_t));
  memcpy(&(wire->htim1), &htim1,sizeof(TIM_HandleTypeDef));
//  Wire_INIT(&wire);
}

// Function to transmit a single byte using single-wire UART
		void WRITE(Wire *wire,uint8_t data) {
			int t ;
			uint8_t bitstream[14] ="";
		  // Data bits (LSB first)
		  for (int i = 7; i >= 0 ; i-- ) {
			  t = (data>>i)&1;
			if (t){
			  strcat(bitstream , one);
			  WIRE_AS_OUTPUT(wire);			//write 1
			  HAL_GPIO_WritePin(wire->GPIOx,wire->Pin,GPIO_PIN_RESET);
			  delay(wire,1);
			  WIRE_AS_INPUT(wire);
			  delay(wire,70);

			} else {
			  strcat(bitstream, zer);
			  WIRE_AS_OUTPUT(wire);			//write 0
			  HAL_GPIO_WritePin(wire->GPIOx,wire->Pin,GPIO_PIN_RESET);
			  delay(wire,70);

			  WIRE_AS_INPUT(wire);
			}
			delay(wire,5);
		  }
		  strcat((char*)bitstream, newline);
		  HAL_UART_Transmit(huart, bitstream, sizeof(bitstream), 100);
		}


// Function to receive a single byte using single-wire UART (for simulation purposes)
		uint8_t READ(Wire *wire) {

		  // Simulate receiving data (replace with actual receiver implementation)
		  uint8_t received_data = 0;

		  for(int i =0; i<8; i++){
			  WIRE_AS_OUTPUT(wire);
			  HAL_GPIO_WritePin(wire->GPIOx, wire->Pin, GPIO_PIN_RESET);
			  delay(wire, 1);
			  WIRE_AS_INPUT(wire);
//			  while(HAL_GPIO_ReadPin(wire->GPIOx,wire->Pin)==GPIO_PIN_RESET){
//				  __NOP();
//			  }
//			  delay(wire , 2);
			  delay(wire ,15);
			  if(HAL_GPIO_ReadPin(wire->GPIOx,wire->Pin)== GPIO_PIN_SET)
				  received_data |=1<<i;
			  delay(wire, 60);
		  }

		  return received_data;  // Replace with actual pin reading
		}

//function to reset wire and recieve presence pulse
		int8_t Wire_INIT(Wire *wire){
			WIRE_AS_OUTPUT(wire);
			HAL_GPIO_WritePin(wire->GPIOx,wire->Pin,GPIO_PIN_RESET);
			delay(wire,500);

			WIRE_AS_INPUT(wire);
			while(HAL_GPIO_ReadPin(wire->GPIOx, wire->Pin)==GPIO_PIN_RESET){
				__NOP();
			}
			delay(wire,30);

		    if(HAL_GPIO_ReadPin(wire->GPIOx, wire->Pin)==GPIO_PIN_RESET){
		    	delay(wire,480);
		    	return 1;
		    }
		    else{
		    	delay(wire,480);
				return 0;
		    }
		}
//function to reinitialise a pin to desired mode
		void BUS_REINIT(Wire *wire,uint8_t type){

			  GPIO_InitTypeDef GPIO_InitStruct = {0};

			  HAL_GPIO_WritePin(wire->GPIOx, wire->Pin, GPIO_PIN_RESET);
			if(type == 1){
				  GPIO_InitStruct.Pin = wire->Pin;
				  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
				  GPIO_InitStruct.Pull = GPIO_NOPULL;
				  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
				  HAL_GPIO_Init(wire->GPIOx, &GPIO_InitStruct);
			}

			else if(type==0) {
				  GPIO_InitStruct.Pin = wire->Pin;
				  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
				  GPIO_InitStruct.Pull = GPIO_NOPULL;
				  HAL_GPIO_Init(wire->GPIOx, &GPIO_InitStruct);
			}
		}

//funciton to deinitialise a pin
		void MX_GPIO_DeInit(Wire *wire) {
		  /* Check if the peripheral is still instantiated (optional) */
		  // You can remove this check or implement a simpler check using GPIOx

		  /* De-initialize the GPIO pin by setting its MODER register bit to '0' */
		  CLEAR_BIT(wire->GPIOx->MODER, (1 << (wire->Pin * 2))); // Set MODER bit for the pin

		  /* Reset remaining GPIO configuration registers */
		  CLEAR_BIT(wire->GPIOx->OTYPER, (1 << wire->Pin));
		  CLEAR_BIT(wire->GPIOx->OSPEEDR, (1 << (wire->Pin * 2))); // Use OSPEEDR for F3

		  /* Clear Alternate Function bits (assuming 4 bits per AFR field) */
		  uint32_t afr_mask = 0xF << ((wire->Pin % 8) * 4); // Mask for specific AFR field
		  CLEAR_BIT(wire->GPIOx->AFR[wire->Pin >> 3], afr_mask); // Clear relevant bits in AFR register

		  CLEAR_BIT(wire->GPIOx->PUPDR, (3 << (wire->Pin * 2)));
		}

//function to check state of pin
		uint8_t PINSTATE(Wire *wire){
			if(HAL_GPIO_ReadPin(wire->GPIOx,wire->Pin) == GPIO_PIN_SET)
				return 1;
			else if(HAL_GPIO_ReadPin(wire->GPIOx,wire->Pin) == GPIO_PIN_RESET)
				return 0;

				else{
					HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
					HAL_Delay(5000);

				return 0;
				}
		}

//microsecond delay function
		void delay (Wire *wire, uint32_t us)
		{
			__HAL_TIM_SET_COUNTER(&(wire->htim1),0);
			while ((__HAL_TIM_GET_COUNTER(&(wire->htim1)))<(us/2))
				__NOP();
		}

		void WIRE_AS_OUTPUT(Wire * wire){
			MX_GPIO_DeInit(wire);
			BUS_REINIT(wire,1);
		}
		void WIRE_AS_INPUT(Wire * wire){
			MX_GPIO_DeInit(wire);
			BUS_REINIT(wire,0);
		}
