#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <modbus.h>

char ip[16];
int port;
int slave = 0;

typedef enum
{
  REG_U16,
  REG_U32,
} register_type_t;

typedef enum
{
  WORD_HL,
  WORD_LH,
} word_order_t;

int parse_u32(const char *s, uint32_t *value)
{
  char *endptr;
  unsigned long parsed;

  if (s == NULL || *s == '\0')
  {
    return -1;
  }

  errno = 0;
  parsed = strtoul(s, &endptr, 0);
  if (errno != 0 || *endptr != '\0' || parsed > UINT32_MAX)
  {
    return -1;
  }

  *value = (uint32_t)parsed;
  return 0;
}

uint32_t read_u32_from_words(const uint16_t *words, word_order_t word_order)
{
  if (word_order == WORD_LH)
  {
    return ((uint32_t)words[1] << 16) | words[0];
  }
  return ((uint32_t)words[0] << 16) | words[1];
}

void write_u32_to_words(uint32_t value, uint16_t *words, word_order_t word_order)
{
  uint16_t high = (uint16_t)((value >> 16) & 0xFFFF);
  uint16_t low = (uint16_t)(value & 0xFFFF);

  if (word_order == WORD_LH)
  {
    words[0] = low;
    words[1] = high;
  }
  else
  {
    words[0] = high;
    words[1] = low;
  }
}

int process_profile(char *profile)
{
  // read profile file from home
  char *home = getenv("HOME");
  if (home == NULL)
  {
    printf("HOME not set\n");
    return -1;
  }

  char *profile_path = malloc(strlen(home) + strlen("/.modbus") + 1);
  if (profile_path == NULL)
  {
    printf("malloc failed\n");
    return -1;
  }

  strcpy(profile_path, home);
  strcat(profile_path, "/.modbus");

  FILE *f = fopen(profile_path, "r");
  if (f == NULL)
  {
    printf("failed to open modbus profile '%s': %s\n", profile_path, strerror(errno));
    return -1;
  }

  free(profile_path);

  char line[256];
  char name[64];
  name[0] = '\0';
  int found = 0;

  // read the csv file and use the requested profile name
  while (fgets(line, sizeof(line), f))
  {
    if (sscanf(line, "%63[^,],%15[^,],%d,%d", name, ip, &port, &slave) == 4)
    {
      if (strcmp(name, profile) == 0)
      {
        found = 1;
        break;
      }
    }
  }

  fclose(f);

  // if the name is empty then return error
  if (!found)
  {
    printf("profile '%s' not found\n", profile);
    return -1;
  }

  printf("using profile %s\n", name);

  return 0;
}

void usage(char *argv[])
{
  printf("Usage: %s [-p=<profile>] [-a=<addr:port>] [-s=<slave_id>] [-t=<u16|u32>] [-w=<hl|lh>] <register>[=<value>]\n", argv[0]);
  printf("  -t type: u16 (default) or u32\n");
  printf("  -w word order for u32: hl (default, high word first) or lh (low word first)\n");
}

int main(int argc, char *argv[])
{
  int status;
  modbus_t *mb;
  uint16_t tab_reg[2];
  char *profile = "default";
  int reg = 0;
  int has_write = 0;
  uint32_t write_value = 0;
  register_type_t reg_type = REG_U16;
  word_order_t word_order = WORD_HL;

  // read the register number from argument, if it contains a = then we need to write the register with the rvalue
  char *query;

  int c;
  char *address = NULL;

  while ((c = getopt(argc, argv, "p:a:s:t:w:h")) != -1)
  {
    switch (c)
    {
    case 'p':
      profile = optarg;
      break;
    case 'a':
      address = optarg;
      break;
    case 's':
      slave = atoi(optarg);
      break;
    case 't':
      if (strcmp(optarg, "u16") == 0)
      {
        reg_type = REG_U16;
      }
      else if (strcmp(optarg, "u32") == 0)
      {
        reg_type = REG_U32;
      }
      else
      {
        fprintf(stderr, "Error: Invalid type '%s'. Use u16 or u32.\n", optarg);
        usage(argv);
        return 1;
      }
      break;
    case 'w':
      if (strcmp(optarg, "hl") == 0)
      {
        word_order = WORD_HL;
      }
      else if (strcmp(optarg, "lh") == 0)
      {
        word_order = WORD_LH;
      }
      else
      {
        fprintf(stderr, "Error: Invalid word order '%s'. Use hl or lh.\n", optarg);
        usage(argv);
        return 1;
      }
      break;
    case 'h':
      usage(argv);
      return 0;
      break;
    case '?':
      fprintf(stderr, "Unknown option: -%c\n", optopt);
      return 1;
    default:
      usage(argv);
      abort();
    }
  }

  // Check for mandatory non-flag argument
  if (optind >= argc)
  {
    fprintf(stderr, "Error: Missing mandatory query argument.\n");
    usage(argv);
    return 1;
  }

  query = argv[optind];

  {
    char *eq = strchr(query, '=');
    if (eq != NULL)
    {
      uint32_t parsed_reg;
      size_t reg_len = (size_t)(eq - query);
      char reg_part[32];

      if (reg_len == 0 || reg_len >= sizeof(reg_part))
      {
        fprintf(stderr, "Error: Invalid register query '%s'.\n", query);
        return 1;
      }

      memcpy(reg_part, query, reg_len);
      reg_part[reg_len] = '\0';

      if (parse_u32(reg_part, &parsed_reg) != 0 || parsed_reg > INT_MAX)
      {
        fprintf(stderr, "Error: Invalid register index '%s'.\n", reg_part);
        return 1;
      }

      if (parse_u32(eq + 1, &write_value) != 0)
      {
        fprintf(stderr, "Error: Invalid register value '%s'.\n", eq + 1);
        return 1;
      }

      reg = (int)parsed_reg;
      has_write = 1;
    }
    else
    {
      uint32_t parsed_reg;
      if (parse_u32(query, &parsed_reg) != 0 || parsed_reg > INT_MAX)
      {
        fprintf(stderr, "Error: Invalid register index '%s'.\n", query);
        return 1;
      }
      reg = (int)parsed_reg;
    }
  }

  if (reg_type == REG_U16 && has_write && write_value > UINT16_MAX)
  {
    fprintf(stderr, "Error: Value %u exceeds u16 range. Use -t u32 for 32-bit values.\n", write_value);
    return 1;
  }

  // Check if both profile and address are provided
  if (profile == NULL && address == NULL)
  {
    fprintf(stderr, "Error: Either -p or -a option must be provided.\n");
    usage(argv);
    return 1;
  }

  // Process the profile file if no address is provided
  if (address == NULL)
  {
    if (process_profile(profile) != 0)
    {
      return 1;
    }
  }
  else if (address != NULL)
  {
    status = sscanf(address, "%[^:]:%d", ip, &port);

    if (status != 2)
    {
      fprintf(stderr, "Error: Invalid address format. Expected format: <ip>:<port>\n");
      usage(argv);
      return 1;
    }

    printf("Address: %s:%d slave: %d\n", ip, port, slave);
  }

  mb = modbus_new_tcp(ip, port);
  if (mb == NULL)
  {
    printf("modbus_new_tcp failed: %s\n", modbus_strerror(errno));
    return -1;
  }

  status = modbus_connect(mb);
  if (status < 0)
  {
    printf("modbus_connect failed: %s\n", modbus_strerror(errno));
    modbus_free(mb);
    return -1;
  }

  status = modbus_set_slave(mb, slave);
  if (status < 0)
  {
    printf("modbus_set_slave failed: %s\n", modbus_strerror(errno));
    modbus_close(mb);
    modbus_free(mb);
    return -1;
  }

  /* Read 1 register for u16 or 2 registers for u32 */
  status = modbus_read_registers(mb, reg, reg_type == REG_U32 ? 2 : 1, tab_reg);
  if (status < 0)
  {
    printf("modbus_read_registers failed: %s\n", modbus_strerror(errno));
    modbus_close(mb);
    modbus_free(mb);
    return -1;
  }

  if (reg_type == REG_U32)
  {
    uint32_t value = read_u32_from_words(tab_reg, word_order);
    printf("addr[%d:%d]=%u (0x%08X) words=[0x%04X,0x%04X]\n", reg, reg + 1, value, value, tab_reg[0], tab_reg[1]);
  }
  else
  {
    printf("addr[%d]=%u (0x%04X)\n", reg, tab_reg[0], tab_reg[0]);
  }

  if (has_write)
  {
    if (reg_type == REG_U32)
    {
      write_u32_to_words(write_value, tab_reg, word_order);
      status = modbus_write_registers(mb, reg, 2, tab_reg);
      if (status < 0)
      {
        printf("modbus_write_registers failed: %s\n", modbus_strerror(errno));
        modbus_close(mb);
        modbus_free(mb);
        return -1;
      }
      printf("wrote addr[%d:%d]=%u (0x%08X) words=[0x%04X,0x%04X]\n", reg, reg + 1, write_value, write_value, tab_reg[0], tab_reg[1]);
    }
    else
    {
      tab_reg[0] = (uint16_t)write_value;
      status = modbus_write_register(mb, reg, tab_reg[0]);
      if (status < 0)
      {
        printf("modbus_write_register failed: %s\n", modbus_strerror(errno));
        modbus_close(mb);
        modbus_free(mb);
        return -1;
      }
      printf("wrote addr[%d]=%u (0x%04X)\n", reg, tab_reg[0], tab_reg[0]);
    }
  }

  modbus_close(mb);
  modbus_free(mb);
}
