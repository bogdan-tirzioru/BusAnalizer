#include "usbd_bulk.h"

#include "usbd_ctlreq.h"
#include "usbd_core.h"

typedef struct
{
  uint8_t *tx_buffer;
  uint32_t tx_length;
  volatile uint8_t tx_busy;
} USBD_BULK_HandleTypeDef;

static uint8_t USBD_BULK_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_BULK_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_BULK_Setup(USBD_HandleTypeDef *pdev,
                               USBD_SetupReqTypedef *req);
static uint8_t USBD_BULK_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_BULK_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t *USBD_BULK_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_BULK_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_BULK_GetOtherSpeedCfgDesc(uint16_t *length);
static uint8_t *USBD_BULK_GetDeviceQualifierDesc(uint16_t *length);

static USBD_BULK_HandleTypeDef bulk_handle;
__ALIGN_BEGIN static uint8_t bulk_rx_buffer[BULK_TEST_BLOCK_SIZE] __ALIGN_END;
static volatile USBD_BULK_StatsTypeDef bulk_stats;

USBD_ClassTypeDef USBD_BULK =
{
  USBD_BULK_Init,
  USBD_BULK_DeInit,
  USBD_BULK_Setup,
  NULL,
  NULL,
  USBD_BULK_DataIn,
  USBD_BULK_DataOut,
  NULL,
  NULL,
  NULL,
  USBD_BULK_GetHSCfgDesc,
  USBD_BULK_GetFSCfgDesc,
  USBD_BULK_GetOtherSpeedCfgDesc,
  USBD_BULK_GetDeviceQualifierDesc,
#if (USBD_SUPPORT_USER_STRING_DESC == 1U)
  NULL,
#endif
};

__ALIGN_BEGIN static uint8_t USBD_BULK_FSCfgDesc[32] __ALIGN_END =
{
  0x09U,
  USB_DESC_TYPE_CONFIGURATION,
  0x20U, 0x00U,
  0x01U,
  0x01U,
  0x00U,
  0xC0U,
  0x32U,

  0x09U,
  USB_DESC_TYPE_INTERFACE,
  0x00U,
  0x00U,
  0x02U,
  0xFFU,
  0x00U,
  0x00U,
  0x00U,

  0x07U,
  USB_DESC_TYPE_ENDPOINT,
  BULK_OUT_EP,
  0x02U,
  LOBYTE(BULK_FS_MAX_PACKET_SIZE),
  HIBYTE(BULK_FS_MAX_PACKET_SIZE),
  0x00U,

  0x07U,
  USB_DESC_TYPE_ENDPOINT,
  BULK_IN_EP,
  0x02U,
  LOBYTE(BULK_FS_MAX_PACKET_SIZE),
  HIBYTE(BULK_FS_MAX_PACKET_SIZE),
  0x00U
};

__ALIGN_BEGIN static uint8_t USBD_BULK_HSCfgDesc[32] __ALIGN_END =
{
  0x09U,
  USB_DESC_TYPE_CONFIGURATION,
  0x20U, 0x00U,
  0x01U,
  0x01U,
  0x00U,
  0xC0U,
  0x32U,

  0x09U,
  USB_DESC_TYPE_INTERFACE,
  0x00U,
  0x00U,
  0x02U,
  0xFFU,
  0x00U,
  0x00U,
  0x00U,

  0x07U,
  USB_DESC_TYPE_ENDPOINT,
  BULK_OUT_EP,
  0x02U,
  LOBYTE(BULK_HS_MAX_PACKET_SIZE),
  HIBYTE(BULK_HS_MAX_PACKET_SIZE),
  0x00U,

  0x07U,
  USB_DESC_TYPE_ENDPOINT,
  BULK_IN_EP,
  0x02U,
  LOBYTE(BULK_HS_MAX_PACKET_SIZE),
  HIBYTE(BULK_HS_MAX_PACKET_SIZE),
  0x00U
};

__ALIGN_BEGIN static uint8_t USBD_BULK_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00U, 0x02U,
  0x00U,
  0x00U,
  0x00U,
  USB_MAX_EP0_SIZE,
  0x01U,
  0x00U
};

static uint16_t USBD_BULK_MaxPacket(const USBD_HandleTypeDef *pdev)
{
  return (pdev->dev_speed == USBD_SPEED_HIGH) ?
         BULK_HS_MAX_PACKET_SIZE : BULK_FS_MAX_PACKET_SIZE;
}

static uint8_t USBD_BULK_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  uint16_t max_packet = USBD_BULK_MaxPacket(pdev);

  UNUSED(cfgidx);

  (void)USBD_memset(&bulk_handle, 0, sizeof(bulk_handle));
  (void)USBD_memset((void *)&bulk_stats, 0, sizeof(bulk_stats));

  pdev->pClassDataCmsit[pdev->classId] = &bulk_handle;
  pdev->pClassData = &bulk_handle;

  (void)USBD_LL_OpenEP(pdev, BULK_IN_EP, USBD_EP_TYPE_BULK, max_packet);
  pdev->ep_in[BULK_IN_EP & 0x0FU].is_used = 1U;
  pdev->ep_in[BULK_IN_EP & 0x0FU].maxpacket = max_packet;

  (void)USBD_LL_OpenEP(pdev, BULK_OUT_EP, USBD_EP_TYPE_BULK, max_packet);
  pdev->ep_out[BULK_OUT_EP & 0x0FU].is_used = 1U;
  pdev->ep_out[BULK_OUT_EP & 0x0FU].maxpacket = max_packet;

  (void)USBD_LL_PrepareReceive(pdev, BULK_OUT_EP,
                               bulk_rx_buffer, BULK_TEST_BLOCK_SIZE);

  return (uint8_t)USBD_OK;
}

static uint8_t USBD_BULK_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);

  (void)USBD_LL_CloseEP(pdev, BULK_IN_EP);
  pdev->ep_in[BULK_IN_EP & 0x0FU].is_used = 0U;

  (void)USBD_LL_CloseEP(pdev, BULK_OUT_EP);
  pdev->ep_out[BULK_OUT_EP & 0x0FU].is_used = 0U;

  bulk_handle.tx_busy = 0U;
  pdev->pClassDataCmsit[pdev->classId] = NULL;
  pdev->pClassData = NULL;

  return (uint8_t)USBD_OK;
}

static uint8_t USBD_BULK_Setup(USBD_HandleTypeDef *pdev,
                               USBD_SetupReqTypedef *req)
{
  uint8_t alternate_setting = 0U;
  uint16_t status = 0U;

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
    case USB_REQ_TYPE_STANDARD:
      switch (req->bRequest)
      {
        case USB_REQ_GET_STATUS:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&status, 2U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            return (uint8_t)USBD_FAIL;
          }
          break;

        case USB_REQ_GET_INTERFACE:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, &alternate_setting, 1U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            return (uint8_t)USBD_FAIL;
          }
          break;

        case USB_REQ_SET_INTERFACE:
          if ((pdev->dev_state != USBD_STATE_CONFIGURED) ||
              (req->wValue != 0U))
          {
            USBD_CtlError(pdev, req);
            return (uint8_t)USBD_FAIL;
          }
          break;

        case USB_REQ_CLEAR_FEATURE:
          break;

        default:
          USBD_CtlError(pdev, req);
          return (uint8_t)USBD_FAIL;
      }
      break;

    default:
      USBD_CtlError(pdev, req);
      return (uint8_t)USBD_FAIL;
  }

  return (uint8_t)USBD_OK;
}

static uint8_t USBD_BULK_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  UNUSED(pdev);

  if ((epnum & 0x0FU) != (BULK_IN_EP & 0x0FU))
  {
    return (uint8_t)USBD_FAIL;
  }

  bulk_stats.tx_transfers++;
  bulk_stats.tx_bytes += bulk_handle.tx_length;
  bulk_handle.tx_busy = 0U;

  return (uint8_t)USBD_OK;
}

static uint8_t USBD_BULK_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  uint32_t received;

  if ((epnum & 0x0FU) != (BULK_OUT_EP & 0x0FU))
  {
    return (uint8_t)USBD_FAIL;
  }

  received = USBD_LL_GetRxDataSize(pdev, epnum);
  bulk_stats.rx_packets++;
  bulk_stats.rx_bytes += received;

  (void)USBD_LL_PrepareReceive(pdev, BULK_OUT_EP,
                               bulk_rx_buffer, BULK_TEST_BLOCK_SIZE);

  return (uint8_t)USBD_OK;
}

static uint8_t *USBD_BULK_GetHSCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_BULK_HSCfgDesc);
  return USBD_BULK_HSCfgDesc;
}

static uint8_t *USBD_BULK_GetFSCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_BULK_FSCfgDesc);
  return USBD_BULK_FSCfgDesc;
}

static uint8_t *USBD_BULK_GetOtherSpeedCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_BULK_FSCfgDesc);
  return USBD_BULK_FSCfgDesc;
}

static uint8_t *USBD_BULK_GetDeviceQualifierDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_BULK_DeviceQualifierDesc);
  return USBD_BULK_DeviceQualifierDesc;
}

uint8_t USBD_BULK_Transmit(USBD_HandleTypeDef *pdev,
                           uint8_t *buffer,
                           uint32_t length)
{
  USBD_StatusTypeDef status;

  if ((pdev == NULL) || (buffer == NULL) || (length == 0U) ||
      (pdev->dev_state != USBD_STATE_CONFIGURED) ||
      (pdev->pClassData != &bulk_handle))
  {
    return (uint8_t)USBD_FAIL;
  }

  if (bulk_handle.tx_busy != 0U)
  {
    return (uint8_t)USBD_BUSY;
  }

  bulk_handle.tx_buffer = buffer;
  bulk_handle.tx_length = length;
  bulk_handle.tx_busy = 1U;

  status = USBD_LL_Transmit(pdev, BULK_IN_EP, buffer, length);
  if (status != USBD_OK)
  {
    bulk_handle.tx_busy = 0U;
  }

  return (uint8_t)status;
}

uint8_t USBD_BULK_TxReady(USBD_HandleTypeDef *pdev)
{
  if ((pdev == NULL) ||
      (pdev->dev_state != USBD_STATE_CONFIGURED) ||
      (pdev->pClassData != &bulk_handle))
  {
    return 0U;
  }

  return (bulk_handle.tx_busy == 0U) ? 1U : 0U;
}

void USBD_BULK_GetStats(USBD_BULK_StatsTypeDef *stats)
{
  if (stats == NULL)
  {
    return;
  }

  stats->tx_transfers = bulk_stats.tx_transfers;
  stats->tx_bytes = bulk_stats.tx_bytes;
  stats->rx_packets = bulk_stats.rx_packets;
  stats->rx_bytes = bulk_stats.rx_bytes;
}
