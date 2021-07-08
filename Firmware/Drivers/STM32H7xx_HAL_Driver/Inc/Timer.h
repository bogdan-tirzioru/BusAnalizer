
#include "stm32h7xx_hal.h"

struct timerinit
{
	TIM_TypeDef *Instance;
	uint32_t Prescaler;
	uint32_t CounterMode;
	uint32_t Period;
	uint32_t ClockDivision;
	uint32_t AutoReloadPreload;
	uint32_t ClockSource;
	uint32_t MasterOutputTrigger;
	uint32_t MasterSlaveMode;
	uint32_t OCMode;
	uint32_t Pulse;
	uint32_t OCPolarity;
	uint32_t OCFastMode;
	uint32_t TIM_CHANNEL;
};


class Timer
{
private:
	  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	  TIM_MasterConfigTypeDef sMasterConfig = {0};
	  TIM_OC_InitTypeDef sConfigOC = {0};

public:

	Timer () {};

	void TimerInit(TIM_HandleTypeDef *Handle, timerinit structinit);
	HAL_StatusTypeDef TIM_Base_Init(TIM_HandleTypeDef *htim);
	HAL_StatusTypeDef TIM_ConfigClockSource(TIM_HandleTypeDef *htim, TIM_ClockConfigTypeDef *sClockSourceConfig);
	HAL_StatusTypeDef TIM_OC_Init(TIM_HandleTypeDef *htim);
	HAL_StatusTypeDef TIMEx_MasterConfigSynchronization(TIM_HandleTypeDef *htim,
	                                                        TIM_MasterConfigTypeDef *sMasterConfig);
	void TIM_TI1_ConfigInputStage(TIM_TypeDef *TIMx, uint32_t TIM_ICPolarity, uint32_t TIM_ICFilter);
	void TIM_ITRx_SetConfig(TIM_TypeDef *TIMx, uint32_t InputTriggerSource);
	void TIM_TI2_ConfigInputStage(TIM_TypeDef *TIMx, uint32_t TIM_ICPolarity, uint32_t TIM_ICFilter);
	HAL_StatusTypeDef TIM_OC_ConfigChannel(TIM_HandleTypeDef *htim,
	                                           TIM_OC_InitTypeDef *sConfig,
	                                           uint32_t Channel);
	void TIM_OC1_SetConfig(TIM_TypeDef *TIMx, TIM_OC_InitTypeDef *OC_Config);
	void TIM_OC2_SetConfig(TIM_TypeDef *TIMx, TIM_OC_InitTypeDef *OC_Config);
	void TIM_OC3_SetConfig(TIM_TypeDef *TIMx, TIM_OC_InitTypeDef *OC_Config);
	void TIM_OC4_SetConfig(TIM_TypeDef *TIMx, TIM_OC_InitTypeDef *OC_Config);
	void TIM_OC5_SetConfig(TIM_TypeDef *TIMx, TIM_OC_InitTypeDef *OC_Config);
	void TIM_OC6_SetConfig(TIM_TypeDef *TIMx, TIM_OC_InitTypeDef *OC_Config);
};
