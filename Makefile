modbus: main.c
	# pkg-config --cflags --libs libmodbus
	gcc -o modbus main.c -lmodbus
	