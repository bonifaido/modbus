# modbus
A modbus CLI written in C based on [libmodbus](https://github.com/stephane/libmodbus).

## Usage

```
modbus [-p=<profile>] [-a=<addr:port>] [-p=<slave_id>] <register>[=<value>]
```

## Profiles
The `modbus` CLI makes it easy to handle multiple modbus devices in your home with the help of profiles. It supports a simple CSV based profile file, located in the user's home directory.

If there is only one profile listed it will be used if not specified, otherwise the one with name default or specified on the CLI.

**Line format:** Name,IP,PORT,SlaveID\n

Example content of the file under `~/.modbus`:

```csv
default,192.168.1.200,4196,1
boiler,192.168.2.101,0
```

## Examples

```bash
# Read the register 41 from the given address
modbus -a 192.168.1.200:4196 41

# Specify slave id for the above operation
modbus -a 192.168.1.200:4196 -s 1 41
```

In this case you have profile file the usage is just simply:

```bash
# Read the register 41
modbus 41

# Write the value 34 to the register number 41
modbus 41=34

# Write the value 34 to the register number 41 for the boiler
modbus -p boiler 41=34
```


# Build and use

You need to have libmodbus installed. On macOS it is:

```bash
# Install libmodbus
brew install libmodbus

# Expose brew based headers and shared libraries
export CPATH="$HOMEBREW_PREFIX/include:$CPATH"
export LIBRARY_PATH="$HOMEBREW_PREFIX/lib:$LIBRARY_PATH"
```

Build and run:

```bash
# Compile
make
# Run
./modbus -h
```