
#ifndef AFDCAN_H
#define AFDCAN_H

#ifdef __cplusplus
extern "C" {
#endif

// Includes
#include "stm32h7xx_hal.h"

// Private defines
#define AFDCAN_TIMEOUT_VALUE 10U
#define AFDCAN_TIMEOUT_COUNT 50U

#define AFDCAN_MESSAGE_RAM_SIZE 0x2800U
#define AFDCAN_MESSAGE_RAM_END_ADDRESS (SRAMCAN_BASE + AFDCAN_MESSAGE_RAM_SIZE - 0x4U) /* The Message RAM has a width of 4 Bytes */

// Functions
#ifdef __cplusplus
static HAL_StatusTypeDef AFDCAN_CalcultateRamBlockAddresses(FDCAN_HandleTypeDef *hfdcan)
{
  uint32_t RAMcounter;
  uint32_t StartAddress;

  StartAddress = hfdcan->Init.MessageRAMOffset;

  /* Standard filter list start address */
  MODIFY_REG(hfdcan->Instance->SIDFC, FDCAN_SIDFC_FLSSA, (StartAddress << FDCAN_SIDFC_FLSSA_Pos));

  /* Standard filter elements number */
  MODIFY_REG(hfdcan->Instance->SIDFC, FDCAN_SIDFC_LSS, (hfdcan->Init.StdFiltersNbr << FDCAN_SIDFC_LSS_Pos));

  /* Extended filter list start address */
  StartAddress += hfdcan->Init.StdFiltersNbr;
  MODIFY_REG(hfdcan->Instance->XIDFC, FDCAN_XIDFC_FLESA, (StartAddress << FDCAN_XIDFC_FLESA_Pos));

  /* Extended filter elements number */
  MODIFY_REG(hfdcan->Instance->XIDFC, FDCAN_XIDFC_LSE, (hfdcan->Init.ExtFiltersNbr << FDCAN_XIDFC_LSE_Pos));

  /* Rx FIFO 0 start address */
  StartAddress += (hfdcan->Init.ExtFiltersNbr * 2U);
  MODIFY_REG(hfdcan->Instance->RXF0C, FDCAN_RXF0C_F0SA, (StartAddress << FDCAN_RXF0C_F0SA_Pos));

  /* Rx FIFO 0 elements number */
  MODIFY_REG(hfdcan->Instance->RXF0C, FDCAN_RXF0C_F0S, (hfdcan->Init.RxFifo0ElmtsNbr << FDCAN_RXF0C_F0S_Pos));

  /* Rx FIFO 1 start address */
  StartAddress += (hfdcan->Init.RxFifo0ElmtsNbr * hfdcan->Init.RxFifo0ElmtSize);
  MODIFY_REG(hfdcan->Instance->RXF1C, FDCAN_RXF1C_F1SA, (StartAddress << FDCAN_RXF1C_F1SA_Pos));

  /* Rx FIFO 1 elements number */
  MODIFY_REG(hfdcan->Instance->RXF1C, FDCAN_RXF1C_F1S, (hfdcan->Init.RxFifo1ElmtsNbr << FDCAN_RXF1C_F1S_Pos));

  /* Rx buffer list start address */
  StartAddress += (hfdcan->Init.RxFifo1ElmtsNbr * hfdcan->Init.RxFifo1ElmtSize);
  MODIFY_REG(hfdcan->Instance->RXBC, FDCAN_RXBC_RBSA, (StartAddress << FDCAN_RXBC_RBSA_Pos));

  /* Tx event FIFO start address */
  StartAddress += (hfdcan->Init.RxBuffersNbr * hfdcan->Init.RxBufferSize);
  MODIFY_REG(hfdcan->Instance->TXEFC, FDCAN_TXEFC_EFSA, (StartAddress << FDCAN_TXEFC_EFSA_Pos));

  /* Tx event FIFO elements number */
  MODIFY_REG(hfdcan->Instance->TXEFC, FDCAN_TXEFC_EFS, (hfdcan->Init.TxEventsNbr << FDCAN_TXEFC_EFS_Pos));

  /* Tx buffer list start address */
  StartAddress += (hfdcan->Init.TxEventsNbr * 2U);
  MODIFY_REG(hfdcan->Instance->TXBC, FDCAN_TXBC_TBSA, (StartAddress << FDCAN_TXBC_TBSA_Pos));

  /* Dedicated Tx buffers number */
  MODIFY_REG(hfdcan->Instance->TXBC, FDCAN_TXBC_NDTB, (hfdcan->Init.TxBuffersNbr << FDCAN_TXBC_NDTB_Pos));

  /* Tx FIFO/queue elements number */
  MODIFY_REG(hfdcan->Instance->TXBC, FDCAN_TXBC_TFQS, (hfdcan->Init.TxFifoQueueElmtsNbr << FDCAN_TXBC_TFQS_Pos));

  hfdcan->msgRam.StandardFilterSA = SRAMCAN_BASE + (hfdcan->Init.MessageRAMOffset * 4U);
  hfdcan->msgRam.ExtendedFilterSA = hfdcan->msgRam.StandardFilterSA + (hfdcan->Init.StdFiltersNbr * 4U);
  hfdcan->msgRam.RxFIFO0SA = hfdcan->msgRam.ExtendedFilterSA + (hfdcan->Init.ExtFiltersNbr * 2U * 4U);
  hfdcan->msgRam.RxFIFO1SA = hfdcan->msgRam.RxFIFO0SA + (hfdcan->Init.RxFifo0ElmtsNbr * hfdcan->Init.RxFifo0ElmtSize * 4U);
  hfdcan->msgRam.RxBufferSA = hfdcan->msgRam.RxFIFO1SA + (hfdcan->Init.RxFifo1ElmtsNbr * hfdcan->Init.RxFifo1ElmtSize * 4U);
  hfdcan->msgRam.TxEventFIFOSA = hfdcan->msgRam.RxBufferSA + (hfdcan->Init.RxBuffersNbr * hfdcan->Init.RxBufferSize * 4U);
  hfdcan->msgRam.TxBufferSA = hfdcan->msgRam.TxEventFIFOSA + (hfdcan->Init.TxEventsNbr * 2U * 4U);
  hfdcan->msgRam.TxFIFOQSA = hfdcan->msgRam.TxBufferSA + (hfdcan->Init.TxBuffersNbr * hfdcan->Init.TxElmtSize * 4U);

  hfdcan->msgRam.EndAddress = hfdcan->msgRam.TxFIFOQSA + (hfdcan->Init.TxFifoQueueElmtsNbr * hfdcan->Init.TxElmtSize * 4U);

  if (hfdcan->msgRam.EndAddress > AFDCAN_MESSAGE_RAM_END_ADDRESS) /* Last address of the Message RAM */
  {
    /* Update error code.
       Message RAM overflow */
    hfdcan->ErrorCode |= HAL_FDCAN_ERROR_PARAM;

    /* Change FDCAN state */
    hfdcan->State = HAL_FDCAN_STATE_ERROR;

    return HAL_ERROR;
  }
  else
  {
    /* Flush the allocated Message RAM area */
    for (RAMcounter = hfdcan->msgRam.StandardFilterSA; RAMcounter < hfdcan->msgRam.EndAddress; RAMcounter += 4U)
    {
      *(uint32_t *)(RAMcounter) = 0x00000000;
    }
  }

  /* Return function status */
  return HAL_OK;
}

// Classes
class AFdCan
{
private:
	// No private members so far
public:
	// Constructor
	AFdCan(void) {;};

	HAL_StatusTypeDef Init(FDCAN_HandleTypeDef *hfdcan);

	// Destructor
	~AFdCan(void) {;};
};

HAL_StatusTypeDef AFdCan::Init(FDCAN_HandleTypeDef *hfdcan)
{
	uint32_t tickstart;
	HAL_StatusTypeDef status;
	const uint32_t CvtEltSize[] = {0, 0, 0, 0, 0, 1, 2, 3, 4, 0, 5, 0, 0, 0, 6, 0, 0, 0, 7};

	/* Check FDCAN handle */
	if (hfdcan == NULL)
	{
	return HAL_ERROR;
	}

	/* Check FDCAN instance */
	if (hfdcan->Instance == FDCAN1)
	{
	hfdcan->ttcan = (TTCAN_TypeDef *)((uint32_t)hfdcan->Instance + 0x100U);
	}

	/* Check function parameters */
	assert_param(IS_FDCAN_ALL_INSTANCE(hfdcan->Instance));
	assert_param(IS_FDCAN_FRAME_FORMAT(hfdcan->Init.FrameFormat));
	assert_param(IS_FDCAN_MODE(hfdcan->Init.Mode));
	assert_param(IS_FUNCTIONAL_STATE(hfdcan->Init.AutoRetransmission));
	assert_param(IS_FUNCTIONAL_STATE(hfdcan->Init.TransmitPause));
	assert_param(IS_FUNCTIONAL_STATE(hfdcan->Init.ProtocolException));
	assert_param(IS_FDCAN_NOMINAL_PRESCALER(hfdcan->Init.NominalPrescaler));
	assert_param(IS_FDCAN_NOMINAL_SJW(hfdcan->Init.NominalSyncJumpWidth));
	assert_param(IS_FDCAN_NOMINAL_TSEG1(hfdcan->Init.NominalTimeSeg1));
	assert_param(IS_FDCAN_NOMINAL_TSEG2(hfdcan->Init.NominalTimeSeg2));
	if (hfdcan->Init.FrameFormat == FDCAN_FRAME_FD_BRS)
	{
	assert_param(IS_FDCAN_DATA_PRESCALER(hfdcan->Init.DataPrescaler));
	assert_param(IS_FDCAN_DATA_SJW(hfdcan->Init.DataSyncJumpWidth));
	assert_param(IS_FDCAN_DATA_TSEG1(hfdcan->Init.DataTimeSeg1));
	assert_param(IS_FDCAN_DATA_TSEG2(hfdcan->Init.DataTimeSeg2));
	}
	assert_param(IS_FDCAN_MAX_VALUE(hfdcan->Init.StdFiltersNbr, 128U));
	assert_param(IS_FDCAN_MAX_VALUE(hfdcan->Init.ExtFiltersNbr, 64U));
	assert_param(IS_FDCAN_MAX_VALUE(hfdcan->Init.RxFifo0ElmtsNbr, 64U));
	if (hfdcan->Init.RxFifo0ElmtsNbr > 0U)
	{
	assert_param(IS_FDCAN_DATA_SIZE(hfdcan->Init.RxFifo0ElmtSize));
	}
	assert_param(IS_FDCAN_MAX_VALUE(hfdcan->Init.RxFifo1ElmtsNbr, 64U));
	if (hfdcan->Init.RxFifo1ElmtsNbr > 0U)
	{
	assert_param(IS_FDCAN_DATA_SIZE(hfdcan->Init.RxFifo1ElmtSize));
	}
	assert_param(IS_FDCAN_MAX_VALUE(hfdcan->Init.RxBuffersNbr, 64U));
	if (hfdcan->Init.RxBuffersNbr > 0U)
	{
	assert_param(IS_FDCAN_DATA_SIZE(hfdcan->Init.RxBufferSize));
	}
	assert_param(IS_FDCAN_MAX_VALUE(hfdcan->Init.TxEventsNbr, 32U));
	assert_param(IS_FDCAN_MAX_VALUE((hfdcan->Init.TxBuffersNbr + hfdcan->Init.TxFifoQueueElmtsNbr), 32U));
	if (hfdcan->Init.TxFifoQueueElmtsNbr > 0U)
	{
	assert_param(IS_FDCAN_TX_FIFO_QUEUE_MODE(hfdcan->Init.TxFifoQueueMode));
	}
	if ((hfdcan->Init.TxBuffersNbr + hfdcan->Init.TxFifoQueueElmtsNbr) > 0U)
	{
	assert_param(IS_FDCAN_DATA_SIZE(hfdcan->Init.TxElmtSize));
	}

#if USE_HAL_FDCAN_REGISTER_CALLBACKS == 1
  if (hfdcan->State == HAL_FDCAN_STATE_RESET)
  {
    /* Allocate lock resource and initialize it */
    hfdcan->Lock = HAL_UNLOCKED;

    /* Reset callbacks to legacy functions */
    hfdcan->ClockCalibrationCallback    = HAL_FDCAN_ClockCalibrationCallback;    /* Legacy weak ClockCalibrationCallback    */
    hfdcan->TxEventFifoCallback         = HAL_FDCAN_TxEventFifoCallback;         /* Legacy weak TxEventFifoCallback         */
    hfdcan->RxFifo0Callback             = HAL_FDCAN_RxFifo0Callback;             /* Legacy weak RxFifo0Callback             */
    hfdcan->RxFifo1Callback             = HAL_FDCAN_RxFifo1Callback;             /* Legacy weak RxFifo1Callback             */
    hfdcan->TxFifoEmptyCallback         = HAL_FDCAN_TxFifoEmptyCallback;         /* Legacy weak TxFifoEmptyCallback         */
    hfdcan->TxBufferCompleteCallback    = HAL_FDCAN_TxBufferCompleteCallback;    /* Legacy weak TxBufferCompleteCallback    */
    hfdcan->TxBufferAbortCallback       = HAL_FDCAN_TxBufferAbortCallback;       /* Legacy weak TxBufferAbortCallback       */
    hfdcan->RxBufferNewMessageCallback  = HAL_FDCAN_RxBufferNewMessageCallback;  /* Legacy weak RxBufferNewMessageCallback  */
    hfdcan->HighPriorityMessageCallback = HAL_FDCAN_HighPriorityMessageCallback; /* Legacy weak HighPriorityMessageCallback */
    hfdcan->TimestampWraparoundCallback = HAL_FDCAN_TimestampWraparoundCallback; /* Legacy weak TimestampWraparoundCallback */
    hfdcan->TimeoutOccurredCallback     = HAL_FDCAN_TimeoutOccurredCallback;     /* Legacy weak TimeoutOccurredCallback     */
    hfdcan->ErrorCallback               = HAL_FDCAN_ErrorCallback;               /* Legacy weak ErrorCallback               */
    hfdcan->ErrorStatusCallback         = HAL_FDCAN_ErrorStatusCallback;         /* Legacy weak ErrorStatusCallback         */
    hfdcan->TT_ScheduleSyncCallback     = HAL_FDCAN_TT_ScheduleSyncCallback;     /* Legacy weak TT_ScheduleSyncCallback     */
    hfdcan->TT_TimeMarkCallback         = HAL_FDCAN_TT_TimeMarkCallback;         /* Legacy weak TT_TimeMarkCallback         */
    hfdcan->TT_StopWatchCallback        = HAL_FDCAN_TT_StopWatchCallback;        /* Legacy weak TT_StopWatchCallback        */
    hfdcan->TT_GlobalTimeCallback       = HAL_FDCAN_TT_GlobalTimeCallback;       /* Legacy weak TT_GlobalTimeCallback       */

    if (hfdcan->MspInitCallback == NULL)
    {
      hfdcan->MspInitCallback = HAL_FDCAN_MspInit;  /* Legacy weak MspInit */
    }

    /* Init the low level hardware: CLOCK, NVIC */
    hfdcan->MspInitCallback(hfdcan);
  }
#else
  if (hfdcan->State == HAL_FDCAN_STATE_RESET)
  {
    /* Allocate lock resource and initialize it */
    hfdcan->Lock = HAL_UNLOCKED;

    /* Init the low level hardware: CLOCK, NVIC */
    HAL_FDCAN_MspInit(hfdcan);
  }
#endif /* USE_HAL_FDCAN_REGISTER_CALLBACKS */

  /* Exit from Sleep mode */
  CLEAR_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_CSR);

  /* Get tick */
  tickstart = HAL_GetTick();

  /* Check Sleep mode acknowledge */
  while ((hfdcan->Instance->CCCR & FDCAN_CCCR_CSA) == FDCAN_CCCR_CSA)
  {
    if ((HAL_GetTick() - tickstart) > AFDCAN_TIMEOUT_VALUE)
    {
      /* Update error code */
      hfdcan->ErrorCode |= HAL_FDCAN_ERROR_TIMEOUT;

      /* Change FDCAN state */
      hfdcan->State = HAL_FDCAN_STATE_ERROR;

      return HAL_ERROR;
    }
  }

  /* Request initialisation */
  SET_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_INIT);

  /* Get tick */
  tickstart = HAL_GetTick();

  /* Wait until the INIT bit into CCCR register is set */
  while ((hfdcan->Instance->CCCR & FDCAN_CCCR_INIT) == 0U)
  {
    /* Check for the Timeout */
    if ((HAL_GetTick() - tickstart) > AFDCAN_TIMEOUT_VALUE)
    {
      /* Update error code */
      hfdcan->ErrorCode |= HAL_FDCAN_ERROR_TIMEOUT;

      /* Change FDCAN state */
      hfdcan->State = HAL_FDCAN_STATE_ERROR;

      return HAL_ERROR;
    }
  }

  /* Enable configuration change */
  SET_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_CCE);

  /* Set the no automatic retransmission */
  if (hfdcan->Init.AutoRetransmission == ENABLE)
  {
    CLEAR_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_DAR);
  }
  else
  {
    SET_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_DAR);
  }

  /* Set the transmit pause feature */
  if (hfdcan->Init.TransmitPause == ENABLE)
  {
    SET_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_TXP);
  }
  else
  {
    CLEAR_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_TXP);
  }

  /* Set the Protocol Exception Handling */
  if (hfdcan->Init.ProtocolException == ENABLE)
  {
    CLEAR_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_PXHD);
  }
  else
  {
    SET_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_PXHD);
  }

  /* Set FDCAN Frame Format */
  MODIFY_REG(hfdcan->Instance->CCCR, FDCAN_FRAME_FD_BRS, hfdcan->Init.FrameFormat);

  /* Reset FDCAN Operation Mode */
  CLEAR_BIT(hfdcan->Instance->CCCR, (FDCAN_CCCR_TEST | FDCAN_CCCR_MON | FDCAN_CCCR_ASM));
  CLEAR_BIT(hfdcan->Instance->TEST, FDCAN_TEST_LBCK);

  /* Set FDCAN Operating Mode:
               | Normal | Restricted |    Bus     | Internal | External
               |        | Operation  | Monitoring | LoopBack | LoopBack
     CCCR.TEST |   0    |     0      |     0      |    1     |    1
     CCCR.MON  |   0    |     0      |     1      |    1     |    0
     TEST.LBCK |   0    |     0      |     0      |    1     |    1
     CCCR.ASM  |   0    |     1      |     0      |    0     |    0
  */
  if (hfdcan->Init.Mode == FDCAN_MODE_RESTRICTED_OPERATION)
  {
    /* Enable Restricted Operation mode */
    SET_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_ASM);
  }
  else if (hfdcan->Init.Mode != FDCAN_MODE_NORMAL)
  {
    if (hfdcan->Init.Mode != FDCAN_MODE_BUS_MONITORING)
    {
      /* Enable write access to TEST register */
      SET_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_TEST);

      /* Enable LoopBack mode */
      SET_BIT(hfdcan->Instance->TEST, FDCAN_TEST_LBCK);

      if (hfdcan->Init.Mode == FDCAN_MODE_INTERNAL_LOOPBACK)
      {
        SET_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_MON);
      }
    }
    else
    {
      /* Enable bus monitoring mode */
      SET_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_MON);
    }
  }
  else
  {
    /* Nothing to do: normal mode */
  }

  /* Set the nominal bit timing register */
  hfdcan->Instance->NBTP = ((((uint32_t)hfdcan->Init.NominalSyncJumpWidth - 1U) << FDCAN_NBTP_NSJW_Pos) | \
                            (((uint32_t)hfdcan->Init.NominalTimeSeg1 - 1U) << FDCAN_NBTP_NTSEG1_Pos)    | \
                            (((uint32_t)hfdcan->Init.NominalTimeSeg2 - 1U) << FDCAN_NBTP_NTSEG2_Pos)    | \
                            (((uint32_t)hfdcan->Init.NominalPrescaler - 1U) << FDCAN_NBTP_NBRP_Pos));

  /* If FD operation with BRS is selected, set the data bit timing register */
  if (hfdcan->Init.FrameFormat == FDCAN_FRAME_FD_BRS)
  {
    hfdcan->Instance->DBTP = ((((uint32_t)hfdcan->Init.DataSyncJumpWidth - 1U) << FDCAN_DBTP_DSJW_Pos) | \
                              (((uint32_t)hfdcan->Init.DataTimeSeg1 - 1U) << FDCAN_DBTP_DTSEG1_Pos)    | \
                              (((uint32_t)hfdcan->Init.DataTimeSeg2 - 1U) << FDCAN_DBTP_DTSEG2_Pos)    | \
                              (((uint32_t)hfdcan->Init.DataPrescaler - 1U) << FDCAN_DBTP_DBRP_Pos));
  }

  if (hfdcan->Init.TxFifoQueueElmtsNbr > 0U)
  {
    /* Select between Tx FIFO and Tx Queue operation modes */
    SET_BIT(hfdcan->Instance->TXBC, hfdcan->Init.TxFifoQueueMode);
  }

  /* Configure Tx element size */
  if ((hfdcan->Init.TxBuffersNbr + hfdcan->Init.TxFifoQueueElmtsNbr) > 0U)
  {
    MODIFY_REG(hfdcan->Instance->TXESC, FDCAN_TXESC_TBDS, CvtEltSize[hfdcan->Init.TxElmtSize]);
  }

  /* Configure Rx FIFO 0 element size */
  if (hfdcan->Init.RxFifo0ElmtsNbr > 0U)
  {
    MODIFY_REG(hfdcan->Instance->RXESC, FDCAN_RXESC_F0DS, (CvtEltSize[hfdcan->Init.RxFifo0ElmtSize] << FDCAN_RXESC_F0DS_Pos));
  }

  /* Configure Rx FIFO 1 element size */
  if (hfdcan->Init.RxFifo1ElmtsNbr > 0U)
  {
    MODIFY_REG(hfdcan->Instance->RXESC, FDCAN_RXESC_F1DS, (CvtEltSize[hfdcan->Init.RxFifo1ElmtSize] << FDCAN_RXESC_F1DS_Pos));
  }

  /* Configure Rx buffer element size */
  if (hfdcan->Init.RxBuffersNbr > 0U)
  {
    MODIFY_REG(hfdcan->Instance->RXESC, FDCAN_RXESC_RBDS, (CvtEltSize[hfdcan->Init.RxBufferSize] << FDCAN_RXESC_RBDS_Pos));
  }

  /* By default operation mode is set to Event-driven communication.
     If Time-triggered communication is needed, user should call the
     HAL_FDCAN_TT_ConfigOperation function just after the HAL_FDCAN_Init */
  if (hfdcan->Instance == FDCAN1)
  {
    CLEAR_BIT(hfdcan->ttcan->TTOCF, FDCAN_TTOCF_OM);
  }

  /* Initialize the Latest Tx FIFO/Queue request buffer index */
  hfdcan->LatestTxFifoQRequest = 0U;

  /* Initialize the error code */
  hfdcan->ErrorCode = HAL_FDCAN_ERROR_NONE;

  /* Initialize the FDCAN state */
  hfdcan->State = HAL_FDCAN_STATE_READY;

  /* Calculate each RAM block address */
  status = AFDCAN_CalcultateRamBlockAddresses(hfdcan);

  /* Return function status */
  return status;
}

#endif // __cplusplus

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // AFDCAN_H
