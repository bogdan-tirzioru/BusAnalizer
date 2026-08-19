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
#define TIMEOUT_MS       2000

static double monotonic_seconds(void)
{
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
  {
    return 0.0;
  }

  return (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);
}

static uint32_t read_le32(const uint8_t *data)
{
  return ((uint32_t)data[0]) |
         ((uint32_t)data[1] << 8) |
         ((uint32_t)data[2] << 16) |
         ((uint32_t)data[3] << 24);
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

int main(int argc, char **argv)
{
  libusb_context *context = NULL;
  libusb_device_handle *device = NULL;
  uint8_t tx_buffer[BLOCK_SIZE];
  uint8_t rx_buffer[BLOCK_SIZE];
  uint64_t tx_bytes = 0U;
  uint64_t rx_bytes = 0U;
  uint32_t last_sequence = 0U;
  uint32_t out_sequence = 0U;
  int have_sequence = 0;
  int duration_seconds = 10;
  int transferred;
  int result;
  double start;
  double last_report;
  double now;

  if (argc > 1)
  {
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(argv[1], &end, 10);
    if ((errno != 0) || (end == argv[1]) || (*end != '\0') ||
        (parsed < 1) || (parsed > 3600))
    {
      fprintf(stderr, "Usage: %s [duration_seconds: 1..3600]\n", argv[0]);
      return EXIT_FAILURE;
    }
    duration_seconds = (int)parsed;
  }

  result = libusb_init(&context);
  if (result != LIBUSB_SUCCESS)
  {
    fprintf(stderr, "libusb_init: %s\n", libusb_error_name(result));
    return EXIT_FAILURE;
  }

  device = libusb_open_device_with_vid_pid(context, USB_VID, USB_PID);
  if (device == NULL)
  {
    fprintf(stderr,
            "USB_test bulk device %04x:%04x not found or permission denied\n",
            USB_VID, USB_PID);
    libusb_exit(context);
    return EXIT_FAILURE;
  }

  (void)libusb_set_auto_detach_kernel_driver(device, 1);

  result = libusb_claim_interface(device, INTERFACE_NUMBER);
  if (result != LIBUSB_SUCCESS)
  {
    fprintf(stderr, "libusb_claim_interface: %s\n",
            libusb_error_name(result));
    libusb_close(device);
    libusb_exit(context);
    return EXIT_FAILURE;
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

  (void)libusb_release_interface(device, INTERFACE_NUMBER);
  libusb_close(device);
  libusb_exit(context);

  return (result == LIBUSB_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
}
