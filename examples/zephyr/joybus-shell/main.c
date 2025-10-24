#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/console/console.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>

#include <joybus/joybus.h>
#include <joybus/backend/gecko.h>

#define JOYBUS_DATA_PORT  gpioPortD
#define JOYBUS_DATA_PIN   3
#define JOYBUS_TIMER      TIMER0
#define JOYBUS_USART      USART0

struct joybus_gecko gecko_bus;
struct joybus *bus = JOYBUS(&gecko_bus);

static uint8_t response[JOYBUS_BLOCK_SIZE];

// Poll thread state
static volatile bool polling_active = false;
static volatile int poll_rate       = 60;
static uint8_t poll_command[JOYBUS_BLOCK_SIZE];
static size_t poll_command_len = 0;
static uint8_t poll_response[JOYBUS_BLOCK_SIZE];
static size_t poll_response_len = 0;

// Print a response buffer as hex
static void print_response(uint8_t *response, uint8_t response_len)
{
  printk("< ");
  for (int i = 0; i < response_len; i++) {
    printk("%02X ", response[i]);
  }
  printk("\n");
}

// Callback for command completion
static void send_cb(struct joybus *bus, int status, void *user_data)
{
  print_response(response, status);
}

// Parse a hex byte from a string, without without 0x prefix
static uint8_t parse_hex_byte(const char *s)
{
  return (uint8_t)strtoul(s, NULL, 16);
}

// Spin up a thread to handle polling
void polling_thread(void *p1, void *p2, void *p3)
{
  while (1) {
    if (polling_active && poll_command_len > 0) {
      joybus_transfer(bus, poll_command, poll_command_len, poll_response, poll_response_len, NULL, NULL);
      k_sleep(K_USEC(1000000 / poll_rate));
    } else {
      k_sleep(K_MSEC(100));
    }
  }
}
K_THREAD_DEFINE(polling_thread_id, 512, polling_thread, NULL, NULL, NULL, 5, 0, 0);

// Handle 'send identify' commands
static int send_identify_handler(const struct shell *sh, size_t argc, char **argv)
{
  joybus_identify(bus, response, send_cb, (void *)sh);

  return 0;
}

// Handle 'send reset' commands
static int send_reset_handler(const struct shell *sh, size_t argc, char **argv)
{
  joybus_reset(bus, response, send_cb, (void *)sh);

  return 0;
}

// Handle 'send n64_read' commands
static int send_n64_read_handler(const struct shell *sh, size_t argc, char **argv)
{
  joybus_n64_read(bus, response, send_cb, (void *)sh);

  return 0;
}

// Handle 'send n64_accessory_read' commands
static int send_n64_accessory_read_handler(const struct shell *sh, size_t argc, char **argv)
{
  uint16_t addr = (uint16_t)strtoul(argv[1], NULL, 16);
  joybus_n64_accessory_read(bus, addr, response, send_cb, (void *)sh);

  return 0;
}

// Handle 'send n64_accessory_write' commands
static int send_n64_accessory_write_handler(const struct shell *sh, size_t argc, char **argv)
{
  uint16_t addr = (uint16_t)strtoul(argv[1], NULL, 16);

  // Parse 32 bytes of hex data from argv[2] onwards
  uint8_t data[32] = {0};
  for (int i = 0; i < 32 && i + 2 < argc; i++) {
    data[i] = parse_hex_byte(argv[i + 2]);
  }

  joybus_n64_accessory_write(bus, addr, data, response, send_cb, (void *)sh);

  return 0;
}

// Handle 'send gcn_read' commands
static int send_gcn_read_handler(const struct shell *sh, size_t argc, char **argv)
{
  uint8_t analog_mode = JOYBUS_GCN_ANALOG_MODE_3;
  uint8_t motor_state = JOYBUS_GCN_MOTOR_STOP;

  if (argc > 1)
    analog_mode = parse_hex_byte(argv[1]);

  if (argc > 2)
    motor_state = parse_hex_byte(argv[2]);

  joybus_gcn_read(bus, analog_mode, motor_state, response, send_cb, (void *)sh);

  return 0;
}

// Handle 'send gcn_read_origin' commands
static int send_gcn_read_origin_handler(const struct shell *sh, size_t argc, char **argv)
{
  joybus_gcn_read_origin(bus, response, send_cb, (void *)sh);

  return 0;
}

// Handle 'send gcn_calibrate' commands
static int send_gcn_calibrate_handler(const struct shell *sh, size_t argc, char **argv)
{
  joybus_gcn_calibrate(bus, response, send_cb, (void *)sh);

  return 0;
}

// Handle 'send gcn_read_long' commands
static int send_gcn_read_long_handler(const struct shell *sh, size_t argc, char **argv)
{
  uint8_t motor_state = JOYBUS_GCN_MOTOR_STOP;

  if (argc > 1)
    motor_state = parse_hex_byte(argv[1]);

  joybus_gcn_read_long(bus, motor_state, response, send_cb, (void *)sh);

  return 0;
}

// Handle 'send gcn_probe_device' commands
static int send_gcn_probe_device_handler(const struct shell *sh, size_t argc, char **argv)
{
  joybus_gcn_probe_device(bus, response, send_cb, (void *)sh);

  return 0;
}

// Handle 'send gcn_fix_device' commands
static int send_gcn_fix_device_handler(const struct shell *sh, size_t argc, char **argv)
{
  uint16_t wireless_id = (uint16_t)strtoul(argv[1], NULL, 16);
  joybus_gcn_fix_device(bus, wireless_id, response, send_cb, (void *)sh);

  return 0;
}

// Handle 'clear' commands
static int clear_handler(const struct shell *sh, size_t argc, char **argv)
{
  // Send VT100 escape sequences to clear the screen
  printf("\033[2J\033[H");

  return 0;
}

// Handle 'poll command' commands
static int poll_command_handler(const struct shell *sh, size_t argc, char **argv)
{
  // Save the command and response lengths
  poll_response_len = atoi(argv[1]);
  poll_command_len  = argc - 2;

  // Save the command bytes
  for (int i = 0; i < poll_command_len; i++)
    poll_command[i] = parse_hex_byte(argv[i + 2]);

  shell_print(sh, "Set poll command (%d bytes, expecting %d byte response)", poll_command_len, poll_response_len);

  return 0;
}

// Handle 'poll rate' commands
static int poll_rate_handler(const struct shell *sh, size_t argc, char **argv)
{
  if (argc == 1) {
    shell_print(sh, "Current poll rate is %dhz", poll_rate);
  } else if (argc == 2) {
    poll_rate = atoi(argv[1]);
    shell_print(sh, "Set poll rate to %dhz", poll_rate);
  }

  return 0;
}

// Handle 'poll start' commands
static int poll_start_handler(const struct shell *sh, size_t argc, char **argv)
{
  if (!poll_command_len) {
    shell_error(sh, "start: Command must be set before starting polling");
    return -1;
  }

  polling_active = true;
  shell_print(sh, "Started polling");

  return 0;
}

// Handle 'poll peek' commands
static int poll_peek_handler(const struct shell *sh, size_t argc, char **argv)
{
  print_response(poll_response, poll_response_len);
  return 0;
}

// Handle 'poll stop' commands
static int poll_stop_handler(const struct shell *sh, size_t argc, char **argv)
{
  polling_active = false;
  shell_print(sh, "Stopped polling");
  return 0;
}

// clang-format off
// Create subcommand for "send"
SHELL_STATIC_SUBCMD_SET_CREATE(sub_send,
  SHELL_CMD(identify, NULL, "Identify the target device attached to the Joybus", send_identify_handler),
  SHELL_CMD(reset, NULL, "Reset the target device attached to the Joybus", send_reset_handler),
  SHELL_CMD(n64_read, NULL, "Read the current input state of a N64 controller", send_n64_read_handler),
  SHELL_CMD_ARG(n64_accessory_read, NULL, SHELL_HELP("Read data from a N64 controller's accessory port", "<addr>"), send_n64_accessory_read_handler, 2, 0),
  SHELL_CMD_ARG(n64_accessory_write, NULL, SHELL_HELP("Write data to a N64 controller's accessory port", "<addr> <data0> <data1> ... <data31>"), send_n64_accessory_write_handler, 2, 32),
  SHELL_CMD_ARG(gcn_read, NULL, SHELL_HELP("Read the current input state of a GameCube controller", "[analog_mode] [motor_state]"), send_gcn_read_handler, 1, 2),
  SHELL_CMD(gcn_read_origin, NULL, "Read the origin state of a GameCube controller", send_gcn_read_origin_handler),
  SHELL_CMD(gcn_calibrate, NULL, "Calibrate a GameCube controller, setting its current input state as the origin", send_gcn_calibrate_handler),
  SHELL_CMD_ARG(gcn_read_long, NULL, SHELL_HELP("Read the current input state of a GameCube controller, with full precision", "[motor_state]"), send_gcn_read_long_handler, 1, 1),
  SHELL_CMD(gcn_probe_device, NULL, "Send a 'probe device' command to a WaveBird controller", send_gcn_probe_device_handler),
  SHELL_CMD_ARG(gcn_fix_device, NULL, SHELL_HELP("Send a 'fix device' command to a WaveBird controller", "<wireless_id>"), send_gcn_fix_device_handler, 2, 0),
  SHELL_SUBCMD_SET_END
);

// Create subcommand for "poll"
SHELL_STATIC_SUBCMD_SET_CREATE(sub_poll,
  SHELL_CMD_ARG(command, NULL, SHELL_HELP("Set command to poll", "<response_len> <byte0> [byte1] ..."), poll_command_handler, 3, 10),
  SHELL_CMD_ARG(rate, NULL, SHELL_HELP("Set poll rate", "<hz>"), poll_rate_handler, 1, 1),
  SHELL_CMD(start, NULL, "Start polling", poll_start_handler),
  SHELL_CMD(peek, NULL, "Print the last polled response", poll_peek_handler),
  SHELL_CMD(stop, NULL, "Stop polling", poll_stop_handler),
  SHELL_SUBCMD_SET_END
);

// Register the top-level commands
SHELL_CMD_REGISTER(send, &sub_send, "Send a single Joybus command", NULL);
SHELL_CMD_REGISTER(poll, &sub_poll, "Configure command polling", NULL);
SHELL_CMD_REGISTER(clear, NULL, "Clear the console", clear_handler);
// clang-format on

int main(void)
{
  // Initialize and enable the Joybus
  joybus_gecko_init(&gecko_bus, JOYBUS_DATA_PORT, JOYBUS_DATA_PIN, JOYBUS_TIMER, JOYBUS_USART);
  joybus_enable(bus);

  // Print welcome message
  printk("Joybus Interactive Shell\n");
  printk("Type 'help' for a list of commands.\n");

  return 0;
}
