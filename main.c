#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <modbus.h>

char ip[16];
int port;
int slave = 0;

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

  // read the csv file , if the name is default or the given profile name then read the ip, port and slave
  while (fgets(line, sizeof(line), f))
  {
    if (sscanf(line, "%63[^,],%15[^,],%d,%d", name, ip, &port, &slave) == 4)
    {
      if (strcmp(name, "default") == 0)
      {
        break;
      }
    }
  }

  fclose(f);

  // if the name is empty then return error
  if (name[0] == '\0')
  {
    printf("profile not found\n");
    return -1;
  }

  printf("using profile %s\n", name);

  return 0;
}

void usage(char *argv[])
{
  printf("Usage: %s [-p=<profile>] [-a=<addr:port>] [-p=<slave_id>] <register>[=<value>]\n", argv[0]);
}

int main(int argc, char *argv[])
{
  int status;
  modbus_t *mb;
  uint16_t tab_reg[8];
  char *profile = "default";
  int reg = 0;
  int rvalue = -1;

  // read the register number from argument, if it contains a = then we need to write the register with the rvalue
  char *query;

  int c;
  char *address = NULL;

  while ((c = getopt(argc, argv, "p:a:s:h")) != -1)
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
    process_profile(profile);
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

  strchr(query, '=') ? sscanf(query, "%d=%d", &reg, &rvalue) : sscanf(query, "%d", &reg);

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

  /* Read 1 register from the address  */
  status = modbus_read_registers(mb, reg, 1, tab_reg);
  if (status < 0)
  {
    printf("modbus_read_registers failed: %s\n", modbus_strerror(errno));
    modbus_close(mb);
    modbus_free(mb);
    return -1;
  }

  printf("addr[%d]=%d (0x%X)\n", reg, tab_reg[0], tab_reg[0]);

  if (rvalue != -1)
  {
    tab_reg[0] = rvalue;
    status = modbus_write_register(mb, reg, tab_reg[0]);
    if (status < 0)
    {
      printf("modbus_write_register failed: %s\n", modbus_strerror(errno));
      modbus_close(mb);
      modbus_free(mb);
      return -1;
    }
    printf("addr[%d]=%d (0x%X)\n", reg, tab_reg[0], tab_reg[0]);
  }

  modbus_close(mb);
  modbus_free(mb);
}
