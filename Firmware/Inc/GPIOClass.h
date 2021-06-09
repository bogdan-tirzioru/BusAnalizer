#ifndef _GPIOClass_H
#define _GPIOClass_H



struct GPIOPinInitStruct
{
  uint32_t Pin;       /*!< Specifies the GPIO pins to be configured.
                           This parameter can be any value of @ref GPIO_pins_define */

  uint32_t Mode;      /*!< Specifies the operating mode for the selected pins.
                           This parameter can be a value of @ref GPIO_mode_define */

  uint32_t Pull;      /*!< Specifies the Pull-up or Pull-Down activation for the selected pins.
                           This parameter can be a value of @ref GPIO_pull_define */

  uint32_t Speed;     /*!< Specifies the speed for the selected pins.
                           This parameter can be a value of @ref GPIO_speed_define */

  uint32_t Alternate;  /*!< Peripheral to be connected to the selected pins.
                            This parameter can be a value of @ref GPIO_Alternate_function_selection */
};

#ifdef __cplusplus


class GPIOClass{

private:
	GPIOPinInitStruct GPIO_InitStructure[2];

public:
	GPIOClass(){};
	void GPIOInit(void);
	void GPIOInit(GPIO_TypeDef  *GPIOx, GPIOPinInitStruct *GPIO_Init);
private:
	void GPIO_RegisterInit(GPIO_TypeDef  *GPIOx, GPIOPinInitStruct *GPIO_Init);
	void GPIO_RegisterDeInit(GPIO_TypeDef  *GPIOx, uint32_t GPIO_Pin);
	void GPIO_StartClock(GPIO_TypeDef  *GPIOx);
public:
	void GPIO_SetPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
	void GPIO_ResetPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
	bool GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
	void GPIO_TogglePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);


};
#endif


#endif /* _GPIOClass_H */
