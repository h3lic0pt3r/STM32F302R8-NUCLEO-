/* current setbacks :
 * read function not solid
 */
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include"stm32f3xx.h"
#include"stm32f3xx_hal.h"



#define BAUDRATE 38400

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



void MX_GPIO_DeInit(Wire*);
void delay (Wire*, uint32_t);
uint8_t PINSTATE(Wire*);
void BUS_REINIT(Wire*,uint8_t);
void WIRE_AS_OUTPUT(Wire *);
void WIRE_AS_INPUT(Wire *);
int8_t Wire_INIT(Wire *);

Wire * init_wire(GPIO_TypeDef* gpiox, int16_t pin, TIM_HandleTypeDef htim1) {
  Wire wire;
  wire.GPIOx = gpiox;
  wire.Pin = pin;
  wire.htim1 = htim1;
  Wire_INIT(&wire);
  return &wire;
}

// Function to transmit a single byte using single-wire UART
		void WRITE(Wire *wire,uint8_t data) {

		  // Define timing constants based on baud rate (adjust as needed)
		  uint32_t bit_duration = 1000000 / BAUDRATE; // Time for one bit in microseconds
		  uint32_t start_bit_duration = bit_duration * 3; // Start bit duration (longer)

		  // Start bit (low for extended duration)
		  HAL_GPIO_WritePin(wire->GPIOx,wire->Pin,GPIO_PIN_RESET);
		  HAL_Delay(start_bit_duration);

		  // Data bits (LSB first)
		  for (int i = 0; i < 8; i++) {
			if (data & (1 << i)) {
			  WIRE_AS_OUTPUT(wire);
			  HAL_GPIO_WritePin(wire->GPIOx,wire->Pin,GPIO_PIN_RESET);
			  delay(wire,1);
			  WIRE_AS_INPUT(wire);
			  delay(wire,60);

			} else {
			  WIRE_AS_OUTPUT(wire);
			  HAL_GPIO_WritePin(wire->GPIOx,wire->Pin,GPIO_PIN_RESET);
			  delay(wire,60);

			  WIRE_AS_INPUT(wire);
			}
			HAL_Delay(bit_duration);
		  }

		//  // Parity bit (even parity in this example)
		//  set_pin_high(pin); // Set high for even parity (all data bits are 1 in 0x44)
		//  delay_microseconds(bit_duration);

		  // Stop bit (low for bit duration)
		  HAL_GPIO_WritePin(wire->GPIOx,wire->Pin,GPIO_PIN_RESET);

		  HAL_Delay(bit_duration);
		}


		// Function to receive a single byte using single-wire UART (for simulation purposes)
		uint8_t READ(Wire *wire) {

		  // Simulate receiving data (replace with actual receiver implementation)
		  uint8_t received_data = 0;


		  HAL_GPIO_WritePin(wire->GPIOx,wire->Pin, GPIO_PIN_RESET);
		  delay(wire,2);
		  // Sample data bits (mid-bit)
		  for (int i = 0; i < 8; i++) {
			received_data |= (PINSTATE(wire) << i);
			delay(wire,60); // Sample mid-bit
		  }

		  // Simulate parity check (even parity in this example)
		  // Implement actual parity check in a real receiver

		  return received_data;  // Replace with actual pin reading
		}


		int8_t Wire_INIT(Wire *wire){
			HAL_GPIO_WritePin(wire->GPIOx,wire->Pin,GPIO_PIN_RESET);
			delay(wire,480);

			WIRE_AS_INPUT(wire);
		    delay(wire,80);

		    if(HAL_GPIO_ReadPin(wire->GPIOx, wire->Pin)==GPIO_PIN_RESET){
		    	delay(wire,400);
		    	return 1;
		    }
		    else{
		    	delay(wire,400);
				return 0;
		    }
		}

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


		void delay (Wire *wire, uint32_t us)
		{
			__HAL_TIM_SET_COUNTER(&(wire->htim1),0);
			while ((__HAL_TIM_GET_COUNTER(&(wire->htim1)))<us)
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
