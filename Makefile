modbus: main.c
	gcc $(shell pkg-config --cflags --libs libmodbus) -o modbus main.c

install: modbus
	sudo install -m 755 modbus /usr/local/bin/modbus

uninstall:
	sudo rm -f /usr/local/bin/modbus
