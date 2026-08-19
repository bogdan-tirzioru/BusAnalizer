#ifndef USBD_BULK_H
#define USBD_BULK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_def.h"

#define BULK_IN_EP                  0x81U
#define BULK_OUT_EP                 0x01U
#define BULK_FS_MAX_PACKET_SIZE     64U
#define BULK_HS_MAX_PACKET_SIZE     512U
#define BULK_TEST_BLOCK_SIZE        4096U
#define BULK_COMMAND_MAX_SIZE       256U

typedef struct
{
  uint32_t tx_transfers;
  uint32_t tx_bytes;
  uint32_t rx_packets;
  uint32_t rx_bytes;
} USBD_BULK_StatsTypeDef;

extern USBD_ClassTypeDef USBD_BULK;

uint8_t USBD_BULK_Transmit(USBD_HandleTypeDef *pdev,
                           uint8_t *buffer,
                           uint32_t length);
uint8_t USBD_BULK_TransmitControl(USBD_HandleTypeDef *pdev,
                                  uint8_t *buffer,
                                  uint32_t length);
uint8_t USBD_BULK_TxReady(USBD_HandleTypeDef *pdev);
uint8_t USBD_BULK_GetCommand(uint8_t *buffer,
                             uint32_t capacity,
                             uint32_t *length);
void USBD_BULK_GetStats(USBD_BULK_StatsTypeDef *stats);

#ifdef __cplusplus
}
#endif

#endif
