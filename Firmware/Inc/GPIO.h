
#ifdef __cplusplus


class gpio
{
public:
	gpio(){};

	void gpio_init(void);
	void gpio_register_init(GPIO_TypeDef  *GPIOx, GPIO_InitTypeDef *GPIO_Init);
	void GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);
};


#endif
