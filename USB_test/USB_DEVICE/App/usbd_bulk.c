#include "usbd_bulk.h"

#include "can_sniffer.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"

#include <string.h>

#define GS_USB_CTRL_BUFFER_SIZE 72U
#define GS_USB_DIR_IN           0x80U

typedef struct
{
  uint8_t *tx_buffer;
  uint32_t tx_length;
  uint8_t *tx_pending_buffer;
  uint32_t tx_pending_length;
  volatile uint8_t tx_busy;
  volatile uint8_t tx_pending;
  uint8_t pending_request;
  uint8_t pending_channel;
  uint8_t pending_valid;
  uint16_t pending_length;
} USBD_GS_USB_HandleTypeDef;

static uint8_t USBD_GS_USB_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_GS_USB_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_GS_USB_Setup(USBD_HandleTypeDef *pdev,
                                 USBD_SetupReqTypedef *req);
static uint8_t USBD_GS_USB_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t USBD_GS_USB_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_GS_USB_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t *USBD_GS_USB_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_GS_USB_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_GS_USB_GetOtherSpeedCfgDesc(uint16_t *length);
static uint8_t *USBD_GS_USB_GetDeviceQualifierDesc(uint16_t *length);

static USBD_GS_USB_HandleTypeDef gs_handle;
__ALIGN_BEGIN static uint8_t gs_rx_buffer[GS_USB_HOST_FRAME_SIZE] __ALIGN_END;
__ALIGN_BEGIN static uint8_t gs_ctrl_buffer[GS_USB_CTRL_BUFFER_SIZE] __ALIGN_END;

_Static_assert(sizeof(GS_USB_DeviceConfig) == 12U,
               "gs_usb device config ABI mismatch");
_Static_assert(sizeof(GS_USB_BitTiming) == 20U,
               "gs_usb bit timing ABI mismatch");
_Static_assert(sizeof(GS_USB_BitTimingConst) == 40U,
               "gs_usb bit timing constants ABI mismatch");
_Static_assert(sizeof(GS_USB_BitTimingConstExtended) == 72U,
               "gs_usb extended bit timing constants ABI mismatch");
_Static_assert(sizeof(GS_USB_HostFrame) == GS_USB_FD_HOST_FRAME_SIZE,
               "gs_usb CAN FD frame ABI mismatch");

USBD_ClassTypeDef USBD_GS_USB =
{
  USBD_GS_USB_Init,
  USBD_GS_USB_DeInit,
  USBD_GS_USB_Setup,
  NULL,
  USBD_GS_USB_EP0_RxReady,
  USBD_GS_USB_DataIn,
  USBD_GS_USB_DataOut,
  NULL,
  NULL,
  NULL,
  USBD_GS_USB_GetHSCfgDesc,
  USBD_GS_USB_GetFSCfgDesc,
  USBD_GS_USB_GetOtherSpeedCfgDesc,
  USBD_GS_USB_GetDeviceQualifierDesc,
#if (USBD_SUPPORT_USER_STRING_DESC == 1U)
  NULL,
#endif
};

__ALIGN_BEGIN static uint8_t USBD_GS_USB_FSCfgDesc[32] __ALIGN_END =
{
  0x09U, USB_DESC_TYPE_CONFIGURATION, 0x20U, 0x00U,
  0x01U, 0x01U, 0x00U, 0x80U, 0x32U,

  0x09U, USB_DESC_TYPE_INTERFACE, 0x00U, 0x00U,
  0x02U, 0xFFU, 0x00U, 0x00U, 0x00U,

  0x07U, USB_DESC_TYPE_ENDPOINT, GS_USB_OUT_EP, 0x02U,
  LOBYTE(GS_USB_FS_MAX_PACKET_SIZE), HIBYTE(GS_USB_FS_MAX_PACKET_SIZE), 0x00U,

  0x07U, USB_DESC_TYPE_ENDPOINT, GS_USB_IN_EP, 0x02U,
  LOBYTE(GS_USB_FS_MAX_PACKET_SIZE), HIBYTE(GS_USB_FS_MAX_PACKET_SIZE), 0x00U
};

__ALIGN_BEGIN static uint8_t USBD_GS_USB_HSCfgDesc[32] __ALIGN_END =
{
  0x09U, USB_DESC_TYPE_CONFIGURATION, 0x20U, 0x00U,
  0x01U, 0x01U, 0x00U, 0x80U, 0x32U,

  0x09U, USB_DESC_TYPE_INTERFACE, 0x00U, 0x00U,
  0x02U, 0xFFU, 0x00U, 0x00U, 0x00U,

  0x07U, USB_DESC_TYPE_ENDPOINT, GS_USB_OUT_EP, 0x02U,
  LOBYTE(GS_USB_HS_MAX_PACKET_SIZE), HIBYTE(GS_USB_HS_MAX_PACKET_SIZE), 0x00U,

  0x07U, USB_DESC_TYPE_ENDPOINT, GS_USB_IN_EP, 0x02U,
  LOBYTE(GS_USB_HS_MAX_PACKET_SIZE), HIBYTE(GS_USB_HS_MAX_PACKET_SIZE), 0x00U
};

__ALIGN_BEGIN static uint8_t USBD_GS_USB_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC, USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00U, 0x02U, 0x00U, 0x00U, 0x00U,
  USB_MAX_EP0_SIZE, 0x01U, 0x00U
};

static uint16_t USBD_GS_USB_MaxPacket(const USBD_HandleTypeDef *pdev)
{
  return (pdev->dev_speed == USBD_SPEED_HIGH) ?
         GS_USB_HS_MAX_PACKET_SIZE : GS_USB_FS_MAX_PACKET_SIZE;
}

static uint8_t USBD_GS_USB_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  uint16_t max_packet = USBD_GS_USB_MaxPacket(pdev);

  UNUSED(cfgidx);
  (void)USBD_memset(&gs_handle, 0, sizeof(gs_handle));

  pdev->pClassDataCmsit[pdev->classId] = &gs_handle;
  pdev->pClassData = &gs_handle;

  (void)USBD_LL_OpenEP(pdev, GS_USB_IN_EP, USBD_EP_TYPE_BULK, max_packet);
  pdev->ep_in[GS_USB_IN_EP & 0x0FU].is_used = 1U;
  pdev->ep_in[GS_USB_IN_EP & 0x0FU].maxpacket = max_packet;

  (void)USBD_LL_OpenEP(pdev, GS_USB_OUT_EP, USBD_EP_TYPE_BULK, max_packet);
  pdev->ep_out[GS_USB_OUT_EP & 0x0FU].is_used = 1U;
  pdev->ep_out[GS_USB_OUT_EP & 0x0FU].maxpacket = max_packet;
  (void)USBD_LL_PrepareReceive(pdev, GS_USB_OUT_EP,
                               gs_rx_buffer, sizeof(gs_rx_buffer));
  return (uint8_t)USBD_OK;
}

static uint8_t USBD_GS_USB_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);
  CAN_Sniffer_ResetAll();

  (void)USBD_LL_CloseEP(pdev, GS_USB_IN_EP);
  pdev->ep_in[GS_USB_IN_EP & 0x0FU].is_used = 0U;
  (void)USBD_LL_CloseEP(pdev, GS_USB_OUT_EP);
  pdev->ep_out[GS_USB_OUT_EP & 0x0FU].is_used = 0U;

  (void)USBD_memset(&gs_handle, 0, sizeof(gs_handle));
  pdev->pClassDataCmsit[pdev->classId] = NULL;
  pdev->pClassData = NULL;
  return (uint8_t)USBD_OK;
}

static uint8_t GS_USB_SendDeviceConfig(USBD_HandleTypeDef *pdev,
                                       uint16_t requested)
{
  GS_USB_DeviceConfig config =
      {{0U, 0U, 0U}, CAN_SNIFFER_CHANNEL_COUNT - 1U, 2U, 1U};
  uint16_t length = (requested < sizeof(config)) ? requested : sizeof(config);

  (void)memcpy(gs_ctrl_buffer, &config, sizeof(config));
  return (uint8_t)USBD_CtlSendData(pdev, gs_ctrl_buffer, length);
}

static uint8_t GS_USB_SendBitTimingConst(USBD_HandleTypeDef *pdev,
                                         uint16_t requested)
{
  GS_USB_BitTimingConst constants =
  {
    GS_CAN_FEATURE_LISTEN_ONLY |
    GS_CAN_FEATURE_FD |
    GS_CAN_FEATURE_BT_CONST_EXT,
    CAN_SNIFFER_FDCAN_CLOCK_HZ,
    1U, 256U,
    1U, 128U,
    128U,
    1U, 512U, 1U
  };
  uint16_t length = (requested < sizeof(constants)) ? requested : sizeof(constants);

  (void)memcpy(gs_ctrl_buffer, &constants, sizeof(constants));
  return (uint8_t)USBD_CtlSendData(pdev, gs_ctrl_buffer, length);
}

static uint8_t GS_USB_SendBitTimingConstExtended(USBD_HandleTypeDef *pdev,
                                                 uint16_t requested)
{
  GS_USB_BitTimingConstExtended constants =
  {
    GS_CAN_FEATURE_LISTEN_ONLY |
    GS_CAN_FEATURE_FD |
    GS_CAN_FEATURE_BT_CONST_EXT,
    CAN_SNIFFER_FDCAN_CLOCK_HZ,
    1U, 256U,
    1U, 128U,
    128U,
    1U, 512U, 1U,
    1U, 32U,
    1U, 16U,
    16U,
    1U, 32U, 1U
  };
  uint16_t length = (requested < sizeof(constants)) ? requested : sizeof(constants);

  (void)memcpy(gs_ctrl_buffer, &constants, sizeof(constants));
  return (uint8_t)USBD_CtlSendData(pdev, gs_ctrl_buffer, length);
}

static uint8_t USBD_GS_USB_Setup(USBD_HandleTypeDef *pdev,
                                 USBD_SetupReqTypedef *req)
{
  uint8_t alternate_setting = 0U;
  uint16_t status = 0U;
  uint8_t channel_valid =
      (req->wValue < CAN_SNIFFER_CHANNEL_COUNT) ? 1U : 0U;

  if ((req->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_VENDOR)
  {
    if (((req->bmRequest & USB_REQ_RECIPIENT_MASK) != USB_REQ_RECIPIENT_INTERFACE) ||
        (req->wIndex != 0U))
    {
      USBD_CtlError(pdev, req);
      return (uint8_t)USBD_FAIL;
    }

    if ((req->bmRequest & GS_USB_DIR_IN) != 0U)
    {
      if ((req->bRequest == GS_USB_BREQ_DEVICE_CONFIG) && (req->wValue == 1U))
      {
        return GS_USB_SendDeviceConfig(pdev, req->wLength);
      }
      if ((req->bRequest == GS_USB_BREQ_BT_CONST) && (channel_valid != 0U))
      {
        return GS_USB_SendBitTimingConst(pdev, req->wLength);
      }
      if ((req->bRequest == GS_USB_BREQ_BT_CONST_EXT) && (channel_valid != 0U))
      {
        return GS_USB_SendBitTimingConstExtended(pdev, req->wLength);
      }
    }
    else if (((req->bRequest == GS_USB_BREQ_HOST_FORMAT) &&
              (req->wValue == 1U) &&
              (req->wLength == sizeof(GS_USB_HostConfig))) ||
             ((req->bRequest == GS_USB_BREQ_BITTIMING) &&
              (channel_valid != 0U) &&
              (req->wLength == sizeof(GS_USB_BitTiming))) ||
             ((req->bRequest == GS_USB_BREQ_DATA_BITTIMING) &&
              (channel_valid != 0U) &&
              (req->wLength == sizeof(GS_USB_BitTiming))) ||
             ((req->bRequest == GS_USB_BREQ_MODE) &&
              (channel_valid != 0U) &&
              (req->wLength == sizeof(GS_USB_Mode))))
    {
      gs_handle.pending_request = req->bRequest;
      gs_handle.pending_channel =
          (req->bRequest == GS_USB_BREQ_HOST_FORMAT) ? 0U : (uint8_t)req->wValue;
      gs_handle.pending_length = req->wLength;
      gs_handle.pending_valid = 1U;
      return (uint8_t)USBD_CtlPrepareRx(pdev, gs_ctrl_buffer, req->wLength);
    }

    USBD_CtlError(pdev, req);
    return (uint8_t)USBD_FAIL;
  }

  if ((req->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_STANDARD)
  {
    switch (req->bRequest)
    {
      case USB_REQ_GET_STATUS:
        return (uint8_t)USBD_CtlSendData(pdev, (uint8_t *)&status, 2U);
      case USB_REQ_GET_INTERFACE:
        return (uint8_t)USBD_CtlSendData(pdev, &alternate_setting, 1U);
      case USB_REQ_SET_INTERFACE:
        if (req->wValue == 0U)
        {
          return (uint8_t)USBD_OK;
        }
        break;
      case USB_REQ_CLEAR_FEATURE:
        return (uint8_t)USBD_OK;
      default:
        break;
    }
  }

  USBD_CtlError(pdev, req);
  return (uint8_t)USBD_FAIL;
}

static uint8_t USBD_GS_USB_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
  uint8_t request;
  uint8_t channel;

  UNUSED(pdev);
  if (gs_handle.pending_valid == 0U)
  {
    return (uint8_t)USBD_FAIL;
  }

  request = gs_handle.pending_request;
  channel = gs_handle.pending_channel;
  gs_handle.pending_valid = 0U;

  if (request == GS_USB_BREQ_HOST_FORMAT)
  {
    GS_USB_HostConfig config;
    (void)memcpy(&config, gs_ctrl_buffer, sizeof(config));
    return (config.byte_order == 0x0000BEEFUL) ?
           (uint8_t)USBD_OK : (uint8_t)USBD_FAIL;
  }

  if (request == GS_USB_BREQ_BITTIMING)
  {
    GS_USB_BitTiming timing;
    (void)memcpy(&timing, gs_ctrl_buffer, sizeof(timing));
    return CAN_Sniffer_SetBitTimingChannel(channel,
                                           timing.prop_seg,
                                           timing.phase_seg1,
                                           timing.phase_seg2,
                                           timing.sjw,
                                           timing.brp) ?
           (uint8_t)USBD_OK : (uint8_t)USBD_FAIL;
  }

  if (request == GS_USB_BREQ_DATA_BITTIMING)
  {
    GS_USB_BitTiming timing;
    (void)memcpy(&timing, gs_ctrl_buffer, sizeof(timing));
    return CAN_Sniffer_SetDataBitTimingChannel(channel,
                                               timing.prop_seg,
                                               timing.phase_seg1,
                                               timing.phase_seg2,
                                               timing.sjw,
                                               timing.brp) ?
           (uint8_t)USBD_OK : (uint8_t)USBD_FAIL;
  }

  if (request == GS_USB_BREQ_MODE)
  {
    GS_USB_Mode mode;
    (void)memcpy(&mode, gs_ctrl_buffer, sizeof(mode));

    if (mode.mode == GS_CAN_MODE_RESET)
    {
      CAN_Sniffer_ResetChannel(channel);
      return (uint8_t)USBD_OK;
    }
    if ((mode.mode == GS_CAN_MODE_START) &&
        ((mode.flags & GS_CAN_MODE_LISTEN_ONLY) != 0U))
    {
      return CAN_Sniffer_StartListenOnlyModeChannel(
                 channel, (mode.flags & GS_CAN_MODE_FD) != 0U) ?
             (uint8_t)USBD_OK : (uint8_t)USBD_FAIL;
    }
  }

  return (uint8_t)USBD_FAIL;
}

static uint8_t USBD_GS_USB_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  uint8_t *next_buffer;
  uint32_t next_length;

  if ((epnum & 0x0FU) != (GS_USB_IN_EP & 0x0FU))
  {
    return (uint8_t)USBD_FAIL;
  }

  if (gs_handle.tx_pending != 0U)
  {
    next_buffer = gs_handle.tx_pending_buffer;
    next_length = gs_handle.tx_pending_length;
    gs_handle.tx_buffer = next_buffer;
    gs_handle.tx_length = next_length;
    gs_handle.tx_pending = 0U;

    /* Keep the endpoint busy and launch the queued frame from the completion
     * callback. This removes one main-loop round trip between USB transfers. */
    if (USBD_LL_Transmit(pdev, GS_USB_IN_EP,
                         next_buffer, next_length) != USBD_OK)
    {
      gs_handle.tx_busy = 0U;
      return (uint8_t)USBD_FAIL;
    }
  }
  else
  {
    gs_handle.tx_busy = 0U;
  }
  return (uint8_t)USBD_OK;
}

static uint8_t USBD_GS_USB_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  if ((epnum & 0x0FU) != (GS_USB_OUT_EP & 0x0FU))
  {
    return (uint8_t)USBD_FAIL;
  }

  /* RX-only analyzer: SocketCAN does not TX in listen-only mode. */
  (void)USBD_LL_PrepareReceive(pdev, GS_USB_OUT_EP,
                               gs_rx_buffer, sizeof(gs_rx_buffer));
  return (uint8_t)USBD_OK;
}

static uint8_t *USBD_GS_USB_GetHSCfgDesc(uint16_t *length)
{
  *length = sizeof(USBD_GS_USB_HSCfgDesc);
  return USBD_GS_USB_HSCfgDesc;
}

static uint8_t *USBD_GS_USB_GetFSCfgDesc(uint16_t *length)
{
  *length = sizeof(USBD_GS_USB_FSCfgDesc);
  return USBD_GS_USB_FSCfgDesc;
}

static uint8_t *USBD_GS_USB_GetOtherSpeedCfgDesc(uint16_t *length)
{
  *length = sizeof(USBD_GS_USB_FSCfgDesc);
  return USBD_GS_USB_FSCfgDesc;
}

static uint8_t *USBD_GS_USB_GetDeviceQualifierDesc(uint16_t *length)
{
  *length = sizeof(USBD_GS_USB_DeviceQualifierDesc);
  return USBD_GS_USB_DeviceQualifierDesc;
}

uint8_t USBD_GS_USB_Transmit(USBD_HandleTypeDef *pdev,
                             uint8_t *buffer,
                             uint32_t length)
{
  USBD_StatusTypeDef status;
  uint32_t primask;
  uint8_t start_now = 0U;

  if ((pdev == NULL) || (buffer == NULL) ||
      ((length != GS_USB_CLASSIC_HOST_FRAME_SIZE) &&
       (length != GS_USB_FD_HOST_FRAME_SIZE)) ||
      (pdev->dev_state != USBD_STATE_CONFIGURED) ||
      (pdev->pClassData != &gs_handle))
  {
    return (uint8_t)USBD_FAIL;
  }
  primask = __get_PRIMASK();
  __disable_irq();

  if (gs_handle.tx_busy == 0U)
  {
    gs_handle.tx_buffer = buffer;
    gs_handle.tx_length = length;
    gs_handle.tx_busy = 1U;
    start_now = 1U;
  }
  else if (gs_handle.tx_pending == 0U)
  {
    gs_handle.tx_pending_buffer = buffer;
    gs_handle.tx_pending_length = length;
    gs_handle.tx_pending = 1U;
  }
  else
  {
    if (primask == 0U)
    {
      __enable_irq();
    }
    return (uint8_t)USBD_BUSY;
  }

  if (primask == 0U)
  {
    __enable_irq();
  }

  if (start_now == 0U)
  {
    return (uint8_t)USBD_OK;
  }

  status = USBD_LL_Transmit(pdev, GS_USB_IN_EP, buffer, length);
  if (status != USBD_OK)
  {
    primask = __get_PRIMASK();
    __disable_irq();
    gs_handle.tx_busy = 0U;
    if (primask == 0U)
    {
      __enable_irq();
    }
  }
  return (uint8_t)status;
}

uint8_t USBD_GS_USB_TxSlotsAvailable(USBD_HandleTypeDef *pdev)
{
  uint8_t used;

  if ((pdev == NULL) || (pdev->dev_state != USBD_STATE_CONFIGURED) ||
      (pdev->pClassData != &gs_handle))
  {
    return 0U;
  }

  used = (gs_handle.tx_busy != 0U) ? 1U : 0U;
  used += (gs_handle.tx_pending != 0U) ? 1U : 0U;
  return (uint8_t)(2U - used);
}
