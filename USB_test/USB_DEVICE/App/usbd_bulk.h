#ifndef USBD_GS_USB_H
#define USBD_GS_USB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_def.h"

#include <stdint.h>

#define GS_USB_IN_EP                         0x81U
#define GS_USB_OUT_EP                        0x01U
#define GS_USB_FS_MAX_PACKET_SIZE            64U
#define GS_USB_HS_MAX_PACKET_SIZE            512U
#define GS_USB_HOST_FRAME_HEADER_SIZE        12U
#define GS_USB_CLASSIC_HOST_FRAME_SIZE       20U
#define GS_USB_FD_HOST_FRAME_SIZE            76U
#define GS_USB_HOST_FRAME_SIZE               GS_USB_FD_HOST_FRAME_SIZE

enum
{
  GS_USB_BREQ_HOST_FORMAT = 0U,
  GS_USB_BREQ_BITTIMING = 1U,
  GS_USB_BREQ_MODE = 2U,
  GS_USB_BREQ_BERR = 3U,
  GS_USB_BREQ_BT_CONST = 4U,
  GS_USB_BREQ_DEVICE_CONFIG = 5U,
  GS_USB_BREQ_TIMESTAMP = 6U,
  GS_USB_BREQ_IDENTIFY = 7U,
  GS_USB_BREQ_GET_USER_ID = 8U,
  GS_USB_BREQ_SET_USER_ID = 9U,
  GS_USB_BREQ_DATA_BITTIMING = 10U,
  GS_USB_BREQ_BT_CONST_EXT = 11U
};

enum
{
  GS_CAN_MODE_RESET = 0U,
  GS_CAN_MODE_START = 1U
};

#define GS_CAN_MODE_LISTEN_ONLY      (1UL << 0)
#define GS_CAN_MODE_FD               (1UL << 8)

#define GS_CAN_FEATURE_LISTEN_ONLY   (1UL << 0)
#define GS_CAN_FEATURE_FD            (1UL << 8)
#define GS_CAN_FEATURE_BT_CONST_EXT  (1UL << 10)

#define GS_CAN_FLAG_OVERFLOW         (1U << 0)
#define GS_CAN_FLAG_FD               (1U << 1)
#define GS_CAN_FLAG_BRS              (1U << 2)
#define GS_CAN_FLAG_ESI              (1U << 3)

#define GS_HOST_FRAME_ECHO_ID_RX     0xFFFFFFFFUL

#define GS_CAN_EFF_FLAG              0x80000000UL
#define GS_CAN_RTR_FLAG              0x40000000UL
#define GS_CAN_EFF_MASK              0x1FFFFFFFUL
#define GS_CAN_SFF_MASK              0x000007FFUL

#if defined(__GNUC__)
#define GS_USB_PACKED __attribute__((packed))
#else
#define GS_USB_PACKED
#endif

typedef struct GS_USB_PACKED
{
  uint32_t byte_order;
} GS_USB_HostConfig;

typedef struct GS_USB_PACKED
{
  uint8_t reserved[3];
  uint8_t icount;
  uint32_t sw_version;
  uint32_t hw_version;
} GS_USB_DeviceConfig;

typedef struct GS_USB_PACKED
{
  uint32_t prop_seg;
  uint32_t phase_seg1;
  uint32_t phase_seg2;
  uint32_t sjw;
  uint32_t brp;
} GS_USB_BitTiming;

typedef struct GS_USB_PACKED
{
  uint32_t mode;
  uint32_t flags;
} GS_USB_Mode;

typedef struct GS_USB_PACKED
{
  uint32_t feature;
  uint32_t fclk_can;
  uint32_t tseg1_min;
  uint32_t tseg1_max;
  uint32_t tseg2_min;
  uint32_t tseg2_max;
  uint32_t sjw_max;
  uint32_t brp_min;
  uint32_t brp_max;
  uint32_t brp_inc;
} GS_USB_BitTimingConst;

typedef struct GS_USB_PACKED
{
  uint32_t feature;
  uint32_t fclk_can;
  uint32_t tseg1_min;
  uint32_t tseg1_max;
  uint32_t tseg2_min;
  uint32_t tseg2_max;
  uint32_t sjw_max;
  uint32_t brp_min;
  uint32_t brp_max;
  uint32_t brp_inc;
  uint32_t dtseg1_min;
  uint32_t dtseg1_max;
  uint32_t dtseg2_min;
  uint32_t dtseg2_max;
  uint32_t dsjw_max;
  uint32_t dbrp_min;
  uint32_t dbrp_max;
  uint32_t dbrp_inc;
} GS_USB_BitTimingConstExtended;

typedef struct GS_USB_PACKED
{
  uint32_t echo_id;
  uint32_t can_id;
  uint8_t can_dlc;
  uint8_t channel;
  uint8_t flags;
  uint8_t reserved;
  uint8_t data[64];
} GS_USB_HostFrame;

extern USBD_ClassTypeDef USBD_GS_USB;

uint8_t USBD_GS_USB_Transmit(USBD_HandleTypeDef *pdev,
                             uint8_t *buffer,
                             uint32_t length);
uint8_t USBD_GS_USB_TxSlotsAvailable(USBD_HandleTypeDef *pdev);

#ifdef __cplusplus
}
#endif

#endif
