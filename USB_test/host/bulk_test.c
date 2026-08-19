#include <libusb-1.0/libusb.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define USB_VID          0x0483
#define USB_PID          0x5741
#define BULK_OUT_EP      0x01
#define BULK_IN_EP       0x81
#define INTERFACE_NUMBER 0
#define BLOCK_SIZE       4096
#define COMMAND_SIZE     32
#define PAYLOAD_SIZE     16
#define TIMEOUT_MS       2000

#define PROTOCOL_VERSION 1
#define COMMAND_PING     0x01
#define COMMAND_INFO     0x02
#define COMMAND_STATS    0x03
#define COMMAND_STREAM   0x04

static double monotonic_seconds(void)
{
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
  {
    return 0.0;
  }

  return (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);
}

static uint16_t read_le16(const uint8_t *data)
{
  return (uint16_t)(((uint16_t)data[0]) |
                    ((uint16_t)data[1] << 8));
}

static uint32_t read_le32(const uint8_t *data)
{
  return ((uint32_t)data[0]) |
         ((uint32_t)data[1] << 8) |
         ((uint32_t)data[2] << 16) |
         ((uint32_t)data[3] << 24);
}

static void write_le16(uint8_t *data, uint16_t value)
{
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8);
}

static void write_le32(uint8_t *data, uint32_t value)
{
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8);
  data[2] = (uint8_t)(value >> 16);
  data[3] = (uint8_t)(value >> 24);
}

static int verify_in_block(const uint8_t *data,
                           uint32_t *last_sequence,
                           int *have_sequence)
{
  uint32_t sequence;
  size_t index;

  if ((data[0] != 'B') || (data[1] != 'U') ||
      (data[2] != 'L') || (data[3] != 'K'))
  {
    fprintf(stderr, "Invalid block magic\n");
    return -1;
  }

  sequence = read_le32(&data[4]);
  if ((*have_sequence != 0) &&
      (sequence != (uint32_t)(*last_sequence + 1U)))
  {
    fprintf(stderr, "Sequence error: expected %u, received %u\n",
            (unsigned int)(*last_sequence + 1U),
            (unsigned int)sequence);
    return -1;
  }

  for (index = 8U; index < BLOCK_SIZE; index++)
  {
    uint8_t expected = (uint8_t)(index + sequence);
    if (data[index] != expected)
    {
      fprintf(stderr,
              "Payload error: sequence=%u offset=%zu expected=%02x received=%02x\n",
              (unsigned int)sequence, index,
              (unsigned int)expected, (unsigned int)data[index]);
      return -1;
    }
  }

  *last_sequence = sequence;
  *have_sequence = 1;
  return 0;
}

static libusb_device_handle *open_bulk_device(libusb_context **context)
{
  libusb_device_handle *device;
  int result;

  result = libusb_init(context);
  if (result != LIBUSB_SUCCESS)
  {
    fprintf(stderr, "libusb_init: %s\n", libusb_error_name(result));
    return NULL;
  }

  device = libusb_open_device_with_vid_pid(*context, USB_VID, USB_PID);
  if (device == NULL)
  {
    fprintf(stderr,
            "USB_test bulk device %04x:%04x not found or permission denied\n",
            USB_VID, USB_PID);
    libusb_exit(*context);
    *context = NULL;
    return NULL;
  }

  (void)libusb_set_auto_detach_kernel_driver(device, 1);

  result = libusb_claim_interface(device, INTERFACE_NUMBER);
  if (result != LIBUSB_SUCCESS)
  {
    fprintf(stderr, "libusb_claim_interface: %s\n",
            libusb_error_name(result));
    libusb_close(device);
    libusb_exit(*context);
    *context = NULL;
    return NULL;
  }

  return device;
}

static void close_bulk_device(libusb_context *context,
                              libusb_device_handle *device)
{
  (void)libusb_release_interface(device, INTERFACE_NUMBER);
  libusb_close(device);
  libusb_exit(context);
}

static int send_command(libusb_device_handle *device,
                        uint8_t command,
                        const uint8_t *payload,
                        uint16_t payload_length,
                        uint8_t *response_payload,
                        uint16_t *response_length)
{
  static uint32_t next_sequence = 1U;
  uint8_t command_frame[COMMAND_SIZE] = {0};
  uint8_t input[BLOCK_SIZE];
  uint32_t sequence = next_sequence++;
  uint32_t response_sequence;
  uint16_t length;
  int transferred;
  int result;
  int attempt;

  if (payload_length > PAYLOAD_SIZE)
  {
    fprintf(stderr, "Command payload is too large\n");
    return -1;
  }

  command_frame[0] = 'B';
  command_frame[1] = 'C';
  command_frame[2] = 'M';
  command_frame[3] = 'D';
  command_frame[4] = PROTOCOL_VERSION;
  command_frame[5] = command;
  write_le32(&command_frame[8], sequence);
  write_le16(&command_frame[12], payload_length);
  if ((payload != NULL) && (payload_length > 0U))
  {
    memcpy(&command_frame[16], payload, payload_length);
  }

  transferred = 0;
  result = libusb_bulk_transfer(device, BULK_OUT_EP,
                                command_frame, COMMAND_SIZE,
                                &transferred, TIMEOUT_MS);
  if ((result != LIBUSB_SUCCESS) || (transferred != COMMAND_SIZE))
  {
    fprintf(stderr, "Command OUT failed: %s, transferred=%d\n",
            libusb_error_name(result), transferred);
    return -1;
  }

  for (attempt = 0; attempt < 8; attempt++)
  {
    transferred = 0;
    result = libusb_bulk_transfer(device, BULK_IN_EP,
                                  input, sizeof(input),
                                  &transferred, TIMEOUT_MS);
    if (result != LIBUSB_SUCCESS)
    {
      fprintf(stderr, "Command response IN failed: %s\n",
              libusb_error_name(result));
      return -1;
    }

    if ((transferred == COMMAND_SIZE) &&
        (input[0] == 'B') && (input[1] == 'R') &&
        (input[2] == 'S') && (input[3] == 'P'))
    {
      response_sequence = read_le32(&input[8]);
      if (response_sequence != sequence)
      {
        continue;
      }

      if (input[4] != PROTOCOL_VERSION)
      {
        fprintf(stderr, "Unsupported response protocol version %u\n",
                (unsigned int)input[4]);
        return -1;
      }

      length = read_le16(&input[12]);
      if (length > PAYLOAD_SIZE)
      {
        fprintf(stderr, "Invalid response payload length %u\n",
                (unsigned int)length);
        return -1;
      }

      if (input[6] != 0U)
      {
        fprintf(stderr, "Command 0x%02x failed with status %u\n",
                (unsigned int)command, (unsigned int)input[6]);
        return -1;
      }

      if ((response_payload != NULL) && (length > 0U))
      {
        memcpy(response_payload, &input[16], length);
      }
      if (response_length != NULL)
      {
        *response_length = length;
      }
      return 0;
    }
  }

  fprintf(stderr, "No matching command response received\n");
  return -1;
}

static int run_named_command(libusb_device_handle *device,
                             int argc,
                             char **argv)
{
  uint8_t response[PAYLOAD_SIZE] = {0};
  uint16_t response_length = 0U;
  uint8_t stream_value;

  if (strcmp(argv[1], "ping") == 0)
  {
    static const uint8_t ping_data[] = "bulk ping";

    if (send_command(device, COMMAND_PING, ping_data,
                     (uint16_t)(sizeof(ping_data) - 1U),
                     response, &response_length) != 0)
    {
      return -1;
    }

    printf("PING response: %.*s\n",
           (int)response_length, (const char *)response);
    return 0;
  }

  if (strcmp(argv[1], "info") == 0)
  {
    if (send_command(device, COMMAND_INFO, NULL, 0U,
                     response, &response_length) != 0)
    {
      return -1;
    }
    if (response_length < 11U)
    {
      fprintf(stderr, "INFO response is too short\n");
      return -1;
    }

    printf("Protocol version : %u\n", (unsigned int)response[0]);
    printf("USB speed        : %s\n", (response[1] == 2U) ? "high" : "full");
    printf("Bulk IN endpoint : 0x%02x\n", (unsigned int)response[2]);
    printf("Bulk OUT endpoint: 0x%02x\n", (unsigned int)response[3]);
    printf("Stream block     : %u bytes\n",
           (unsigned int)read_le32(&response[4]));
    printf("Stream enabled   : %s\n", response[8] ? "yes" : "no");
    printf("Firmware version : %u.%u\n",
           (unsigned int)response[9], (unsigned int)response[10]);
    return 0;
  }

  if (strcmp(argv[1], "stats") == 0)
  {
    if (send_command(device, COMMAND_STATS, NULL, 0U,
                     response, &response_length) != 0)
    {
      return -1;
    }
    if (response_length != PAYLOAD_SIZE)
    {
      fprintf(stderr, "STATS response has invalid length\n");
      return -1;
    }

    printf("IN transfers : %u\n", (unsigned int)read_le32(&response[0]));
    printf("IN bytes     : %u\n", (unsigned int)read_le32(&response[4]));
    printf("OUT transfers: %u\n", (unsigned int)read_le32(&response[8]));
    printf("OUT bytes    : %u\n", (unsigned int)read_le32(&response[12]));
    return 0;
  }

  if ((strcmp(argv[1], "stream") == 0) && (argc == 3))
  {
    if (strcmp(argv[2], "start") == 0)
    {
      stream_value = 1U;
    }
    else if (strcmp(argv[2], "stop") == 0)
    {
      stream_value = 0U;
    }
    else
    {
      fprintf(stderr, "Stream argument must be start or stop\n");
      return -1;
    }

    if (send_command(device, COMMAND_STREAM, &stream_value, 1U,
                     response, &response_length) != 0)
    {
      return -1;
    }

    printf("Stream is %s\n",
           ((response_length == 1U) && (response[0] != 0U)) ?
           "enabled" : "disabled");
    return 0;
  }

  fprintf(stderr,
          "Commands: ping | info | stats | stream start | stream stop\n");
  return -1;
}

static int run_benchmark(libusb_device_handle *device, int duration_seconds)
{
  uint8_t tx_buffer[BLOCK_SIZE];
  uint8_t rx_buffer[BLOCK_SIZE];
  uint8_t stream_value = 1U;
  uint8_t response[PAYLOAD_SIZE];
  uint16_t response_length;
  uint64_t tx_bytes = 0U;
  uint64_t rx_bytes = 0U;
  uint32_t last_sequence = 0U;
  uint32_t out_sequence = 0U;
  int have_sequence = 0;
  int transferred;
  int result = LIBUSB_SUCCESS;
  double start;
  double last_report;
  double now;

  if (send_command(device, COMMAND_STREAM, &stream_value, 1U,
                   response, &response_length) != 0)
  {
    return -1;
  }

  start = monotonic_seconds();
  last_report = start;

  printf("Testing USB bulk IN/OUT for %d seconds...\n", duration_seconds);

  while ((monotonic_seconds() - start) < (double)duration_seconds)
  {
    size_t index;

    for (index = 0U; index < BLOCK_SIZE; index++)
    {
      tx_buffer[index] = (uint8_t)(index + out_sequence);
    }
    out_sequence++;

    transferred = 0;
    result = libusb_bulk_transfer(device, BULK_OUT_EP,
                                  tx_buffer, BLOCK_SIZE,
                                  &transferred, TIMEOUT_MS);
    if ((result != LIBUSB_SUCCESS) || (transferred != BLOCK_SIZE))
    {
      fprintf(stderr, "Bulk OUT failed: %s, transferred=%d\n",
              libusb_error_name(result), transferred);
      break;
    }
    tx_bytes += (uint64_t)transferred;

    transferred = 0;
    result = libusb_bulk_transfer(device, BULK_IN_EP,
                                  rx_buffer, BLOCK_SIZE,
                                  &transferred, TIMEOUT_MS);
    if ((result != LIBUSB_SUCCESS) || (transferred != BLOCK_SIZE))
    {
      fprintf(stderr, "Bulk IN failed: %s, transferred=%d\n",
              libusb_error_name(result), transferred);
      break;
    }
    rx_bytes += (uint64_t)transferred;

    if (verify_in_block(rx_buffer, &last_sequence, &have_sequence) != 0)
    {
      result = LIBUSB_ERROR_IO;
      break;
    }

    now = monotonic_seconds();
    if ((now - last_report) >= 1.0)
    {
      double elapsed = now - start;
      printf("IN %.3f MiB/s, OUT %.3f MiB/s, sequence %u\n",
             ((double)rx_bytes / (1024.0 * 1024.0)) / elapsed,
             ((double)tx_bytes / (1024.0 * 1024.0)) / elapsed,
             (unsigned int)last_sequence);
      last_report = now;
    }
  }

  now = monotonic_seconds();
  if (now <= start)
  {
    now = start + 0.000001;
  }

  printf("Final: IN %.3f MiB/s, OUT %.3f MiB/s, verified %llu IN bytes\n",
         ((double)rx_bytes / (1024.0 * 1024.0)) / (now - start),
         ((double)tx_bytes / (1024.0 * 1024.0)) / (now - start),
         (unsigned long long)rx_bytes);

  return (result == LIBUSB_SUCCESS) ? 0 : -1;
}

int main(int argc, char **argv)
{
  libusb_context *context = NULL;
  libusb_device_handle *device;
  int duration_seconds = 10;
  int result;

  if ((argc > 1) &&
      (strcmp(argv[1], "ping") != 0) &&
      (strcmp(argv[1], "info") != 0) &&
      (strcmp(argv[1], "stats") != 0) &&
      (strcmp(argv[1], "stream") != 0))
  {
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(argv[1], &end, 10);
    if ((errno != 0) || (end == argv[1]) || (*end != '\0') ||
        (parsed < 1) || (parsed > 3600))
    {
      fprintf(stderr,
              "Usage: %s [seconds | ping | info | stats | stream start|stop]\n",
              argv[0]);
      return EXIT_FAILURE;
    }
    duration_seconds = (int)parsed;
  }

  device = open_bulk_device(&context);
  if (device == NULL)
  {
    return EXIT_FAILURE;
  }

  if ((argc > 1) &&
      ((strcmp(argv[1], "ping") == 0) ||
       (strcmp(argv[1], "info") == 0) ||
       (strcmp(argv[1], "stats") == 0) ||
       (strcmp(argv[1], "stream") == 0)))
  {
    result = run_named_command(device, argc, argv);
  }
  else
  {
    result = run_benchmark(device, duration_seconds);
  }

  close_bulk_device(context, device);
  return (result == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
