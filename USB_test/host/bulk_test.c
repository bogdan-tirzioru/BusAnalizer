#include <libusb-1.0/libusb.h>

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define USB_VID             0x0483
#define USB_PID             0x5741
#define BULK_OUT_EP         0x01
#define BULK_IN_EP          0x81
#define INTERFACE_NUMBER    0
#define READ_BUFFER_SIZE    8192
#define CONTROL_BUFFER_SIZE 256
#define TIMEOUT_MS          2000

#define BAII_HEADER_SIZE     20U
#define BAII_VERSION_MAJOR   0U
#define BAII_VERSION_MINOR   1U
#define BAII_MSG_COMMAND     0x01U
#define BAII_MSG_RESPONSE    0x02U
#define BAII_MSG_CAN_DATA    0x10U

#define BAII_CMD_GET_INFO           0x0001U
#define BAII_CMD_GET_STATUS         0x0002U
#define BAII_CMD_GET_CAN_CONFIG     0x0020U
#define BAII_CMD_SET_CAN_CONFIG     0x0021U
#define BAII_CMD_CAPTURE_START      0x0030U
#define BAII_CMD_CAPTURE_STOP       0x0031U
#define BAII_CMD_CAPTURE_CLEAR      0x0032U
#define BAII_CMD_GET_CAPTURE_STATUS 0x0033U

#define BAII_CAN_FLAG_EXT (1U << 0)
#define BAII_CAN_FLAG_RTR (1U << 1)
#define BAII_CAN_FLAG_FD  (1U << 2)
#define BAII_CAN_FLAG_BRS (1U << 3)
#define BAII_CAN_FLAG_ESI (1U << 4)

static volatile sig_atomic_t stop_requested;

static void on_signal(int signal_number)
{
  (void)signal_number;
  stop_requested = 1;
}

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
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

static uint32_t read_le32(const uint8_t *data)
{
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
         ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

static uint64_t read_le64(const uint8_t *data)
{
  uint64_t value = 0U;
  unsigned int i;
  for (i = 0U; i < 8U; i++)
  {
    value |= (uint64_t)data[i] << (8U * i);
  }
  return value;
}

static void write_le16(uint8_t *data, uint16_t value)
{
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8U);
}

static void write_le32(uint8_t *data, uint32_t value)
{
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8U);
  data[2] = (uint8_t)(value >> 16U);
  data[3] = (uint8_t)(value >> 24U);
}

static libusb_device_handle *open_device(libusb_context **context)
{
  libusb_device_handle *device;
  int result = libusb_init(context);

  if (result != LIBUSB_SUCCESS)
  {
    fprintf(stderr, "libusb_init: %s\n", libusb_error_name(result));
    return NULL;
  }

  device = libusb_open_device_with_vid_pid(*context, USB_VID, USB_PID);
  if (device == NULL)
  {
    fprintf(stderr, "BAII USB device %04x:%04x not found or inaccessible\n",
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

static void close_device(libusb_context *context,
                         libusb_device_handle *device)
{
  (void)libusb_release_interface(device, INTERFACE_NUMBER);
  libusb_close(device);
  libusb_exit(context);
}

static int find_response(const uint8_t *input, size_t input_length,
                         uint32_t transaction, uint16_t command,
                         uint8_t *data, uint32_t data_capacity,
                         uint32_t *data_length)
{
  size_t offset = 0U;

  while ((input_length - offset) >= BAII_HEADER_SIZE)
  {
    const uint8_t *message = &input[offset];
    uint32_t transaction_id;
    uint32_t payload_length;
    uint32_t total_length;
    const uint8_t *payload;
    uint16_t response_command;
    uint16_t status;

    if ((message[0] != 'B') || (message[1] != 'A') ||
        (message[2] != 'I') || (message[3] != 'I'))
    {
      fprintf(stderr, "Invalid BAII stream magic at offset %zu\n", offset);
      return -1;
    }

    payload_length = read_le32(&message[16]);
    total_length = BAII_HEADER_SIZE + payload_length;
    if ((total_length < BAII_HEADER_SIZE) ||
        (total_length > (input_length - offset)))
    {
      fprintf(stderr, "Truncated BAII message\n");
      return -1;
    }

    transaction_id = read_le32(&message[8]);
    if ((message[6] == BAII_MSG_RESPONSE) &&
        (transaction_id == transaction))
    {
      if (payload_length < 4U)
      {
        fprintf(stderr, "Short BAII response\n");
        return -1;
      }

      payload = &message[BAII_HEADER_SIZE];
      response_command = read_le16(&payload[0]);
      status = read_le16(&payload[2]);
      if (response_command != command)
      {
        fprintf(stderr, "Response command mismatch\n");
        return -1;
      }
      if (status != 0U)
      {
        fprintf(stderr, "BAII command 0x%04x failed, status=0x%04x\n",
                command, status);
        return -1;
      }

      payload_length -= 4U;
      if (payload_length > data_capacity)
      {
        fprintf(stderr, "BAII response is too large\n");
        return -1;
      }
      if ((data != NULL) && (payload_length > 0U))
      {
        memcpy(data, &payload[4], payload_length);
      }
      *data_length = payload_length;
      return 1;
    }

    offset += total_length;
  }

  return 0;
}

static int send_command(libusb_device_handle *device, uint16_t command,
                        const uint8_t *arguments, uint32_t argument_length,
                        uint8_t *response, uint32_t response_capacity,
                        uint32_t *response_length)
{
  static uint32_t next_transaction = 1U;
  static uint32_t next_sequence = 1U;
  uint8_t request[CONTROL_BUFFER_SIZE] = {0};
  uint8_t input[READ_BUFFER_SIZE];
  uint32_t transaction = next_transaction++;
  uint32_t payload_length = 4U + argument_length;
  uint32_t request_length = BAII_HEADER_SIZE + payload_length;
  int transferred;
  int result;
  int attempt;

  if ((request_length > sizeof(request)) || (response_length == NULL))
  {
    return -1;
  }

  request[0] = 'B';
  request[1] = 'A';
  request[2] = 'I';
  request[3] = 'I';
  request[4] = BAII_VERSION_MAJOR;
  request[5] = BAII_VERSION_MINOR;
  request[6] = BAII_MSG_COMMAND;
  write_le32(&request[8], transaction);
  write_le32(&request[12], next_sequence++);
  write_le32(&request[16], payload_length);
  write_le16(&request[20], command);
  if ((arguments != NULL) && (argument_length > 0U))
  {
    memcpy(&request[24], arguments, argument_length);
  }

  transferred = 0;
  result = libusb_bulk_transfer(device, BULK_OUT_EP, request,
                                (int)request_length, &transferred, TIMEOUT_MS);
  if ((result != LIBUSB_SUCCESS) ||
      (transferred != (int)request_length))
  {
    fprintf(stderr, "BAII command OUT: %s, transferred=%d\n",
            libusb_error_name(result), transferred);
    return -1;
  }

  *response_length = 0U;
  for (attempt = 0; attempt < 8; attempt++)
  {
    int found;
    transferred = 0;
    result = libusb_bulk_transfer(device, BULK_IN_EP, input, sizeof(input),
                                  &transferred, TIMEOUT_MS);
    if (result != LIBUSB_SUCCESS)
    {
      fprintf(stderr, "BAII response IN: %s\n", libusb_error_name(result));
      return -1;
    }

    found = find_response(input, (size_t)transferred, transaction, command,
                          response, response_capacity, response_length);
    if (found != 0)
    {
      return (found > 0) ? 0 : -1;
    }
  }

  fprintf(stderr, "No matching BAII response\n");
  return -1;
}

static int command_no_data(libusb_device_handle *device, uint16_t command)
{
  uint8_t response[64];
  uint32_t response_length;
  return send_command(device, command, NULL, 0U, response, sizeof(response),
                      &response_length);
}

static int show_info(libusb_device_handle *device)
{
  uint8_t response[64];
  uint32_t length;

  if (send_command(device, BAII_CMD_GET_INFO, NULL, 0U,
                   response, sizeof(response), &length) != 0)
  {
    return -1;
  }
  if (length < 24U)
  {
    fprintf(stderr, "GET_INFO response is too short\n");
    return -1;
  }

  printf("Protocol          : BAII %u.%u\n",
         BAII_VERSION_MAJOR, BAII_VERSION_MINOR);
  printf("Firmware          : %u.%u.%u\n", response[0], response[1],
         read_le16(&response[2]));
  printf("Capabilities      : 0x%08x\n", read_le32(&response[4]));
  printf("FDCAN kernel clock: %u Hz\n", read_le32(&response[8]));
  printf("Device ID         : %08x\n", read_le32(&response[16]));
  printf("CAN channels      : %u\n", response[20]);
  return 0;
}

static int show_status(libusb_device_handle *device)
{
  uint8_t response[64];
  uint32_t length;

  if (send_command(device, BAII_CMD_GET_STATUS, NULL, 0U,
                   response, sizeof(response), &length) != 0)
  {
    return -1;
  }
  if (length < 36U)
  {
    fprintf(stderr, "GET_STATUS response is too short\n");
    return -1;
  }

  printf("Uptime            : %u ms\n", read_le32(&response[0]));
  printf("CAN RX frames     : %u\n", read_le32(&response[4]));
  printf("SRAM buffered     : %u\n", read_le32(&response[8]));
  printf("SRAM dropped      : %u\n", read_le32(&response[12]));
  printf("FDCAN FIFO lost   : %u\n", read_le32(&response[16]));
  return 0;
}

static void print_config(const uint8_t *response, uint32_t length)
{
  if (length < 28U)
  {
    fprintf(stderr, "CAN_CONFIG response is too short\n");
    return;
  }

  printf("Channel           : CAN%u\n", response[0]);
  printf("Mode              : %s\n",
         response[1] == 1U ? "listen-only" : "normal");
  printf("Frame format      : %s\n",
         response[2] == 0U ? "Classic CAN" : "CAN FD");
  printf("FDCAN clock       : %u Hz\n", read_le32(&response[4]));
  printf("Nominal bitrate   : %u bit/s\n", read_le32(&response[8]));
  printf("Sample point      : %.1f%%\n",
         (double)read_le16(&response[16]) / 10.0);
  printf("Timing            : prescaler=%u seg1=%u seg2=%u sjw=%u\n",
         read_le16(&response[20]), read_le16(&response[22]),
         read_le16(&response[24]), read_le16(&response[26]));
}

static int show_config(libusb_device_handle *device)
{
  uint8_t arguments[4] = {1U, 0U, 0U, 0U};
  uint8_t response[64];
  uint32_t length;

  if (send_command(device, BAII_CMD_GET_CAN_CONFIG,
                   arguments, sizeof(arguments),
                   response, sizeof(response), &length) != 0)
  {
    return -1;
  }
  print_config(response, length);
  return (length >= 28U) ? 0 : -1;
}

static int set_bitrate_config(libusb_device_handle *device, uint32_t bitrate)
{
  uint8_t arguments[16] = {0};
  uint8_t response[64];
  uint32_t length;

  arguments[0] = 1U;
  arguments[1] = 1U;
  arguments[2] = 0U;
  write_le32(&arguments[4], bitrate);
  write_le32(&arguments[8], bitrate);
  write_le16(&arguments[12], 875U);
  write_le16(&arguments[14], 875U);

  if (send_command(device, BAII_CMD_SET_CAN_CONFIG,
                   arguments, sizeof(arguments),
                   response, sizeof(response), &length) != 0)
  {
    return -1;
  }
  print_config(response, length);
  return (length >= 28U) ? 0 : -1;
}

static int show_capture_status(libusb_device_handle *device)
{
  uint8_t response[64];
  uint32_t length;

  if (send_command(device, BAII_CMD_GET_CAPTURE_STATUS, NULL, 0U,
                   response, sizeof(response), &length) != 0)
  {
    return -1;
  }
  if (length < 20U)
  {
    fprintf(stderr, "CAPTURE_STATUS response is too short\n");
    return -1;
  }

  printf("Capture enabled   : %s\n", response[0] ? "yes" : "no");
  printf("SRAM buffered     : %u\n", read_le32(&response[4]));
  printf("SRAM dropped      : %u\n", read_le32(&response[8]));
  printf("FDCAN FIFO lost   : %u\n", read_le32(&response[12]));
  printf("CAN RX frames     : %u\n", read_le32(&response[16]));
  return 0;
}

static uint64_t print_can_payload(const uint8_t *payload, uint32_t length)
{
  uint32_t offset = 0U;
  uint64_t frames = 0U;

  while ((length - offset) >= 18U)
  {
    const uint8_t *record = &payload[offset];
    uint64_t timestamp = read_le64(&record[0]);
    uint32_t can_id = read_le32(&record[8]);
    uint16_t flags = read_le16(&record[12]);
    uint8_t channel = record[14];
    uint8_t dlc = record[15];
    uint8_t data_length = record[16];
    uint32_t record_length = 18U + data_length;
    unsigned int i;

    if ((record_length > (length - offset)) || (data_length > 64U))
    {
      fprintf(stderr, "Malformed CAN record\n");
      break;
    }

    printf("[%llu.%06llu] can%u ",
           (unsigned long long)(timestamp / 1000000ULL),
           (unsigned long long)(timestamp % 1000000ULL), channel);
    if ((flags & BAII_CAN_FLAG_EXT) != 0U)
    {
      printf("%08X#", can_id);
    }
    else
    {
      printf("%03X#", can_id);
    }

    if ((flags & BAII_CAN_FLAG_RTR) != 0U)
    {
      printf("R%u", dlc);
    }
    else
    {
      for (i = 0U; i < data_length; i++)
      {
        printf("%02X", record[18U + i]);
      }
    }

    if ((flags & (BAII_CAN_FLAG_FD | BAII_CAN_FLAG_BRS |
                  BAII_CAN_FLAG_ESI)) != 0U)
    {
      printf(" flags=%s%s%s",
             (flags & BAII_CAN_FLAG_FD) ? "FD " : "",
             (flags & BAII_CAN_FLAG_BRS) ? "BRS " : "",
             (flags & BAII_CAN_FLAG_ESI) ? "ESI" : "");
    }
    putchar('\n');

    frames++;
    offset += record_length;
  }

  if (offset != length)
  {
    fprintf(stderr, "Trailing %u bytes in CAN_DATA payload\n",
            (unsigned int)(length - offset));
  }
  return frames;
}

static int process_stream_chunk(const uint8_t *input, size_t input_length,
                                uint64_t *frames)
{
  size_t offset = 0U;

  while ((input_length - offset) >= BAII_HEADER_SIZE)
  {
    const uint8_t *message = &input[offset];
    uint32_t payload_length;
    uint32_t total_length;

    if ((message[0] != 'B') || (message[1] != 'A') ||
        (message[2] != 'I') || (message[3] != 'I'))
    {
      fprintf(stderr, "Lost BAII framing at byte %zu\n", offset);
      return -1;
    }

    payload_length = read_le32(&message[16]);
    total_length = BAII_HEADER_SIZE + payload_length;
    if ((total_length > (input_length - offset)) ||
        (message[4] != BAII_VERSION_MAJOR) ||
        (message[5] != BAII_VERSION_MINOR))
    {
      fprintf(stderr, "Invalid or truncated BAII stream message\n");
      return -1;
    }

    if (message[6] == BAII_MSG_CAN_DATA)
    {
      *frames += print_can_payload(&message[BAII_HEADER_SIZE], payload_length);
    }
    offset += total_length;
  }

  return (offset == input_length) ? 0 : -1;
}

static int sniff(libusb_device_handle *device, int duration_seconds)
{
  uint8_t input[READ_BUFFER_SIZE];
  uint64_t frames = 0U;
  uint64_t last_frames = 0U;
  double start;
  double last_report;
  int transferred;
  int result;

  if (command_no_data(device, BAII_CMD_CAPTURE_START) != 0)
  {
    return -1;
  }

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
  start = monotonic_seconds();
  last_report = start;
  fprintf(stderr, "Sniffing CAN1 using the active bitrate profile "
                  "(Ctrl-C to stop)...\n");

  while (!stop_requested &&
         ((duration_seconds == 0) ||
          ((monotonic_seconds() - start) < (double)duration_seconds)))
  {
    double now;
    transferred = 0;
    result = libusb_bulk_transfer(device, BULK_IN_EP, input, sizeof(input),
                                  &transferred, 1000);
    if (result == LIBUSB_ERROR_TIMEOUT)
    {
      continue;
    }
    if (result != LIBUSB_SUCCESS)
    {
      fprintf(stderr, "CAN stream IN: %s\n", libusb_error_name(result));
      return -1;
    }
    if (process_stream_chunk(input, (size_t)transferred, &frames) != 0)
    {
      return -1;
    }

    now = monotonic_seconds();
    if ((now - last_report) >= 1.0)
    {
      fprintf(stderr, "rate=%llu fps total=%llu\n",
              (unsigned long long)(frames - last_frames),
              (unsigned long long)frames);
      last_frames = frames;
      last_report = now;
    }
  }

  fprintf(stderr, "Captured %llu CAN frames\n",
          (unsigned long long)frames);
  return 0;
}

static void usage(const char *program)
{
  fprintf(stderr,
          "Usage:\n"
          "  %s info\n"
          "  %s status\n"
          "  %s config [set250k|set500k]\n"
          "  %s capture start|stop|clear|status\n"
          "  %s sniff [seconds]\n",
          program, program, program, program, program);
}

int main(int argc, char **argv)
{
  libusb_context *context = NULL;
  libusb_device_handle *device;
  int result = -1;

  if (argc < 2)
  {
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  device = open_device(&context);
  if (device == NULL)
  {
    return EXIT_FAILURE;
  }

  if ((argc == 2) && (strcmp(argv[1], "info") == 0))
  {
    result = show_info(device);
  }
  else if ((argc == 2) && (strcmp(argv[1], "status") == 0))
  {
    result = show_status(device);
  }
  else if ((argc == 2) && (strcmp(argv[1], "config") == 0))
  {
    result = show_config(device);
  }
  else if ((argc == 3) && (strcmp(argv[1], "config") == 0) &&
           ((strcmp(argv[2], "set250k") == 0) ||
            (strcmp(argv[2], "set500k") == 0)))
  {
    result = set_bitrate_config(
        device, (strcmp(argv[2], "set250k") == 0) ? 250000U : 500000U);
  }
  else if ((argc == 3) && (strcmp(argv[1], "capture") == 0))
  {
    if (strcmp(argv[2], "start") == 0)
    {
      result = command_no_data(device, BAII_CMD_CAPTURE_START);
    }
    else if (strcmp(argv[2], "stop") == 0)
    {
      result = command_no_data(device, BAII_CMD_CAPTURE_STOP);
    }
    else if (strcmp(argv[2], "clear") == 0)
    {
      result = command_no_data(device, BAII_CMD_CAPTURE_CLEAR);
    }
    else if (strcmp(argv[2], "status") == 0)
    {
      result = show_capture_status(device);
    }
  }
  else if ((strcmp(argv[1], "sniff") == 0) && (argc <= 3))
  {
    int duration = 0;
    if (argc == 3)
    {
      char *end = NULL;
      long parsed;
      errno = 0;
      parsed = strtol(argv[2], &end, 10);
      if ((errno != 0) || (end == argv[2]) || (*end != '\0') ||
          (parsed < 1) || (parsed > 86400))
      {
        usage(argv[0]);
        close_device(context, device);
        return EXIT_FAILURE;
      }
      duration = (int)parsed;
    }
    result = sniff(device, duration);
  }

  if (result != 0)
  {
    usage(argv[0]);
  }
  close_device(context, device);
  return (result == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
