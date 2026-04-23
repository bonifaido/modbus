modbus: main.c
	gcc $(shell pkg-config --cflags libmodbus) -o modbus main.c $(shell pkg-config --libs libmodbus)

.PHONY: clean install uninstall

clean:
	rm -f modbus

.PHONY: clean install uninstall

clean:
	rm -f modbus

install: modbus
	sudo install -m 755 modbus /usr/local/bin/modbus

uninstall:
	sudo rm -f /usr/local/bin/modbus
