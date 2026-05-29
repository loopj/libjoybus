#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"

#include <joybus/joybus.h>
#include <joybus/backend/rp2xxx.h>

// GPIO pin for Joybus data line
#define JOYBUS_GPIO 0

#define MAX_LINE    256
#define MAX_ARGS    JOYBUS_BLOCK_SIZE

// Joybus instance
struct joybus_rp2xxx rp2xxx_bus;
struct joybus *bus = JOYBUS(&rp2xxx_bus);

// Buffer for Joybus responses
uint8_t joybus_response[JOYBUS_BLOCK_SIZE] = {0};

// State for synchronous transfers
volatile bool transfer_done  = false;
volatile int transfer_result = 0;

// State for background polling of a raw command
struct repeating_timer poll_timer;
bool poll_active                         = false;
uint32_t poll_interval_us                = 1000000 / 60; // default 60 Hz
uint8_t poll_command[JOYBUS_BLOCK_SIZE]  = {0};
int poll_command_len                     = 0; // 0 means no command configured
uint8_t poll_read_len                    = 0;
uint8_t poll_response[JOYBUS_BLOCK_SIZE] = {0};
int poll_last_result                     = 0;
bool poll_has_result                     = false;

// Command forward declarations
static void cmd_help(int argc, char **argv);
static void cmd_clear(int argc, char **argv);
static void cmd_transfer(int argc, char **argv);
static void cmd_poll_command(int argc, char **argv);
static void cmd_poll_rate(int argc, char **argv);
static void cmd_poll_start(int argc, char **argv);
static void cmd_poll_stop(int argc, char **argv);
static void cmd_poll_peek(int argc, char **argv);
static void cmd_identify(int argc, char **argv);
static void cmd_reset(int argc, char **argv);
static void cmd_n64_read(int argc, char **argv);
static void cmd_n64_accessory_read(int argc, char **argv);
static void cmd_n64_accessory_write(int argc, char **argv);
static void cmd_gcn_read(int argc, char **argv);
static void cmd_gcn_read_origin(int argc, char **argv);
static void cmd_gcn_calibrate(int argc, char **argv);
static void cmd_gcn_read_long(int argc, char **argv);

// Command dispatch table.
static const struct {
  const char *name;
  void (*fn)(int, char **);
  int min_args;
  const char *usage;
  const char *help;
} commands[] = {
  {"help", cmd_help, 0, "[command]", "Show this help message, or detailed help for a command"},
  {"clear", cmd_clear, 0, NULL, "Clear the console"},
  {"transfer", cmd_transfer, 2, "<read_len> <command> [arg0] [arg1] ...",
   "Perform a raw Joybus transfer of the given data bytes"},
  {"poll_command", cmd_poll_command, 0, "<read_len> <command> [arg0] [arg1] ...",
   "Set the raw Joybus command to poll in the background"},
  {"poll_rate", cmd_poll_rate, 0, "<hz>", "Set the background polling rate in Hz (default 60)"},
  {"poll_start", cmd_poll_start, 0, NULL, "Start polling the configured command in the background"},
  {"poll_stop", cmd_poll_stop, 0, NULL, "Stop background polling"},
  {"poll_peek", cmd_poll_peek, 0, NULL, "Print the result of the most recent poll"},
  {"identify", cmd_identify, 0, NULL, "Identify the target device attached to the Joybus"},
  {"reset", cmd_reset, 0, NULL, "Reset the target device attached to the Joybus"},
  {"n64_read", cmd_n64_read, 0, NULL, "Read the current input state of a N64 controller"},
  {"n64_accessory_read", cmd_n64_accessory_read, 1, "<addr>", "Read data from a N64 controller pak"},
  {"n64_accessory_write", cmd_n64_accessory_write, 33, "<addr> <data0> <data1> ... <data31>",
   "Write data to a N64 controller pak"},
  {"gcn_read", cmd_gcn_read, 2, "<analog_mode> <motor_state>", "Read the current input state of a GameCube controller"},
  {"gcn_read_origin", cmd_gcn_read_origin, 0, NULL, "Read the origin state of a GameCube controller"},
  {"gcn_calibrate", cmd_gcn_calibrate, 0, NULL,
   "Calibrate a GameCube controller, setting its current input state as the origin"},
  {"gcn_read_long", cmd_gcn_read_long, 1, "<motor_state>",
   "Read the current input state of a GameCube controller, with full precision"},
};
#define NUM_CMDS (sizeof(commands) / sizeof(commands[0]))

// Human-readable message for a Joybus error code
static const char *joybus_error_str(int result)
{
  switch (-result) {
    case JOYBUS_ERR_DISABLED:
      return "bus not enabled";
    case JOYBUS_ERR_BUSY:
      return "bus is busy";
    case JOYBUS_ERR_TIMEOUT:
      return "command timeout";
    default:
      return "unknown error";
  }
}

// Parse a hex or decimal number from a string
static int parse_uint(const char *s, unsigned int *out)
{
  // strtoul silently accepts a leading '-' and wraps it, so reject signs ourselves
  if (*s == '-' || *s == '+')
    return -1;

  char *end;
  unsigned long v = strtoul(s, &end, 0);
  if (end == s || *end != '\0')
    return -1;
  *out = (unsigned int)v;
  return 0;
}

// Parse a 16-bit Joybus accessory address from a string, printing an error on failure
static int parse_addr(const char *s, uint16_t *out)
{
  unsigned int addr;
  if (parse_uint(s, &addr) < 0 || addr > 0xFFFF) {
    printf("invalid address: %s\r\n", s);
    return -1;
  }
  *out = (uint16_t)addr;
  return 0;
}

// Parse a single data byte from a string, printing an error on failure
static int parse_byte(const char *s, uint8_t *out)
{
  unsigned int byte;
  if (parse_uint(s, &byte) < 0 || byte > 0xFF) {
    printf("invalid data byte: %s\r\n", s);
    return -1;
  }
  *out = (uint8_t)byte;
  return 0;
}

// Generic transfer callback
static void transfer_cb(struct joybus *bus, int result, void *user_data)
{
  transfer_result = result;
  transfer_done   = true;
}

// Block until the in-flight transfer completes and return its result
static int sync_wait(int start_result)
{
  if (start_result < 0)
    return start_result;

  while (!transfer_done)
    tight_loop_contents();

  return transfer_result;
}

// Wrapper to send a joybus command and wait for the response synchronously
#define sync_command(fn, ...) (transfer_done = false, sync_wait(fn(__VA_ARGS__, transfer_cb, NULL)))

// Print a response buffer or error message
static void print_data(const uint8_t *buf, int result)
{
  if (result < 0) {
    printf("! %s\r\n", joybus_error_str(result));
    return;
  }

  printf("<");
  for (int i = 0; i < result; i++)
    printf(" %02x", buf[i]);
  printf("\r\n");
}

// Print the shared response buffer or an error message
static void print_response(int result)
{
  print_data(joybus_response, result);
}

// Background poll completion callback: stash the result for poll_peek
static void poll_cb(struct joybus *bus, int result, void *user_data)
{
  poll_last_result = result;
  poll_has_result  = true;
}

// Repeating timer callback: fire the configured command asynchronously
static bool poll_task(struct repeating_timer *timer)
{
  if (poll_active && poll_command_len > 0)
    joybus_transfer(bus, poll_command, (uint8_t)poll_command_len, poll_response, poll_read_len, poll_cb, NULL);

  return true;
}

// (Re)arm the repeating timer at the current interval, using a negative delay for a fixed firing rate
static void poll_rearm(void)
{
  cancel_repeating_timer(&poll_timer);
  add_repeating_timer_us(-(int64_t)poll_interval_us, poll_task, NULL, &poll_timer);
}

// Tokenize a command line into arguments
static int tokenize(char *line, char **argv, int max)
{
  int argc  = 0;
  char *tok = strtok(line, " \t");
  while (tok && argc < max) {
    argv[argc++] = tok;
    tok          = strtok(NULL, " \t");
  }
  return argc;
}

// Print the usage line for a named command
static void print_usage(const char *name)
{
  for (size_t i = 0; i < NUM_CMDS; i++) {
    if (strcmp(name, commands[i].name) == 0) {
      printf("Usage: %s %s\r\n", name, commands[i].usage ? commands[i].usage : "");
      return;
    }
  }
}

// Dispatch a command line to the appropriate handler
static void dispatch(char *line)
{
  // Tokenize the line into arguments
  char *argv[MAX_ARGS];
  int argc = tokenize(line, argv, MAX_ARGS);
  if (argc == 0)
    return;

  // Look up the command
  for (size_t i = 0; i < NUM_CMDS; i++) {
    if (strcmp(argv[0], commands[i].name) == 0) {
      // Enforce minimum argument count
      if (argc - 1 < commands[i].min_args) {
        print_usage(commands[i].name);
        return;
      }

      // Call the handler
      commands[i].fn(argc, argv);
      return;
    }
  }

  printf("Unknown command: \"%s\"\r\n", argv[0]);
}

static void cmd_help(int argc, char **argv)
{
  // With a command name, print its description and usage
  if (argc >= 2) {
    for (size_t i = 0; i < NUM_CMDS; i++) {
      if (strcmp(argv[1], commands[i].name) == 0) {
        printf("%s\r\n", commands[i].help);
        print_usage(commands[i].name);
        return;
      }
    }

    printf("Unknown command: \"%s\"\r\n", argv[1]);
    return;
  }

  // Otherwise list every command with its description
  for (size_t i = 0; i < NUM_CMDS; i++)
    printf("%-20s %s\r\n", commands[i].name, commands[i].help);
}

static void cmd_clear(int argc, char **argv)
{
  printf("\033[2J\033[H");
}

static void cmd_transfer(int argc, char **argv)
{
  // Parse the expected response length
  uint8_t read_len;
  if (parse_byte(argv[1], &read_len) < 0)
    return;

  // Parse each remaining argument as a data byte to write
  uint8_t data[JOYBUS_BLOCK_SIZE];
  int len = argc - 2;
  for (int i = 0; i < len; i++) {
    if (parse_byte(argv[i + 2], &data[i]) < 0)
      return;
  }

  // Write the bytes and read back the requested number of bytes in response
  print_response(sync_command(joybus_transfer, bus, data, (uint8_t)len, joybus_response, read_len));
}

static void cmd_poll_command(int argc, char **argv)
{
  // With no arguments, print the currently configured command
  if (argc < 2) {
    if (poll_command_len == 0) {
      printf("no poll command set\r\n");
      print_usage("poll_command");
      return;
    }

    printf("read_len=%u, command=0x%02x, args=[", poll_read_len, poll_command[0]);
    for (int i = 1; i < poll_command_len; i++)
      printf("%s0x%02x", i > 1 ? ", " : "", poll_command[i]);
    printf("]\r\n");
    return;
  }

  // Require an expected response length plus at least the command byte
  if (argc < 3) {
    print_usage("poll_command");
    return;
  }

  // Parse the expected response length
  uint8_t read_len;
  if (parse_byte(argv[1], &read_len) < 0)
    return;

  // Parse each remaining argument as a data byte to write
  uint8_t data[JOYBUS_BLOCK_SIZE];
  int len = argc - 2;
  for (int i = 0; i < len; i++) {
    if (parse_byte(argv[i + 2], &data[i]) < 0)
      return;
  }

  // Store the parsed command for the background poller
  memcpy(poll_command, data, len);
  poll_command_len = len;
  poll_read_len    = read_len;
  poll_has_result  = false;
}

static void cmd_poll_rate(int argc, char **argv)
{
  // With no arguments, print the current polling rate
  if (argc < 2) {
    printf("poll rate: %u Hz\r\n", 1000000 / poll_interval_us);
    return;
  }

  // Parse the rate and convert to a microsecond interval
  unsigned int hz;
  if (parse_uint(argv[1], &hz) < 0 || hz == 0) {
    printf("invalid rate: %s\r\n", argv[1]);
    return;
  }
  poll_interval_us = 1000000 / hz;

  // Apply the new rate immediately if we're already polling
  if (poll_active)
    poll_rearm();
}

static void cmd_poll_start(int argc, char **argv)
{
  // Refuse to start without a configured command
  if (poll_command_len == 0) {
    printf("no poll command set; use poll_command first\r\n");
    return;
  }

  poll_active = true;
  poll_rearm();
}

static void cmd_poll_stop(int argc, char **argv)
{
  poll_active = false;
  cancel_repeating_timer(&poll_timer);
}

static void cmd_poll_peek(int argc, char **argv)
{
  if (!poll_has_result) {
    printf("no poll result yet\r\n");
    return;
  }

  print_data(poll_response, poll_last_result);
}

static void cmd_reset(int argc, char **argv)
{
  print_response(sync_command(joybus_reset, bus, joybus_response));
}

static void cmd_identify(int argc, char **argv)
{
  print_response(sync_command(joybus_identify, bus, joybus_response));
}

static void cmd_n64_read(int argc, char **argv)
{
  print_response(sync_command(joybus_n64_read, bus, joybus_response));
}

static void cmd_n64_accessory_read(int argc, char **argv)
{
  // Parse the address argument
  uint16_t addr;
  if (parse_addr(argv[1], &addr) < 0)
    return;

  // Send the command and print the response
  print_response(sync_command(joybus_n64_accessory_read, bus, addr, joybus_response));
}

static void cmd_n64_accessory_write(int argc, char **argv)
{
  // Parse the address argument
  uint16_t addr;
  if (parse_addr(argv[1], &addr) < 0)
    return;

  // Parse the block of data bytes as hex or decimal
  uint8_t data[JOYBUS_ACCESSORY_BLOCK_SIZE];
  for (int i = 0; i < JOYBUS_ACCESSORY_BLOCK_SIZE; i++) {
    if (parse_byte(argv[i + 2], &data[i]) < 0)
      return;
  }

  print_response(sync_command(joybus_n64_accessory_write, bus, addr, data, joybus_response));
}

static void cmd_gcn_read(int argc, char **argv)
{
  // Parse the analog mode and motor state arguments
  uint8_t analog_mode, motor_state;
  if (parse_byte(argv[1], &analog_mode) < 0 || parse_byte(argv[2], &motor_state) < 0)
    return;

  print_response(sync_command(joybus_gcn_read, bus, analog_mode, motor_state, joybus_response));
}

static void cmd_gcn_read_origin(int argc, char **argv)
{
  print_response(sync_command(joybus_gcn_read_origin, bus, joybus_response));
}

static void cmd_gcn_calibrate(int argc, char **argv)
{
  print_response(sync_command(joybus_gcn_calibrate, bus, joybus_response));
}

static void cmd_gcn_read_long(int argc, char **argv)
{
  // Parse the motor state argument
  uint8_t motor_state;
  if (parse_byte(argv[1], &motor_state) < 0)
    return;

  print_response(sync_command(joybus_gcn_read_long, bus, motor_state, joybus_response));
}

int main()
{
  // Initialize stdio
  stdio_init_all();

  // Wait for USB serial connection so we don't start printing before the user can see it
  while (!stdio_usb_connected()) {
    sleep_ms(100);
  }

  // Initialize Joybus
  joybus_rp2xxx_init(&rp2xxx_bus, JOYBUS_GPIO, pio0);
  joybus_enable(bus);

  // Print welcome message and help prompt
  printf("\033[2J\033[H");
  printf("Joybus Interactive Shell\r\n");
  printf("Type 'help' for a list of commands.\r\n");

  char line[MAX_LINE];
  int len = 0;
  printf("> ");

  while (true) {
    int c = getchar();
    if (c == PICO_ERROR_TIMEOUT)
      continue;

    if (c == '\r' || c == '\n') {
      putchar('\r');
      putchar('\n');
      line[len] = 0;
      dispatch(line);
      len = 0;
      printf("> ");
    } else if (c == '\b' || c == 127) { // backspace
      if (len > 0) {
        len--;
        printf("\b \b");
      }
    } else if (len < MAX_LINE - 1 && c >= 32) {
      line[len++] = c;
      putchar(c); // echo
    }
  }
}