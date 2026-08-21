#ifndef GS_USB_FRAME_H
#define GS_USB_FRAME_H

#include <stdint.h>

#define GS_USB_HOST_FRAME_HEADER_SIZE        12U
#define GS_USB_CLASSIC_HOST_FRAME_SIZE       20U
#define GS_USB_FD_HOST_FRAME_SIZE            76U
#define GS_USB_HOST_FRAME_SIZE               GS_USB_FD_HOST_FRAME_SIZE

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
  uint32_t echo_id;
  uint32_t can_id;
  uint8_t can_dlc;
  uint8_t channel;
  uint8_t flags;
  uint8_t reserved;
  uint8_t data[64];
} GS_USB_HostFrame;

#endif
