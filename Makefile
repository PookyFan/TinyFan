CC=avr-gcc
OBJDUMP=avr-objdump
SIZE=avr-size
CFLAGS=-g -Os -Wall -Wextra -fpermissive -fno-exceptions -ffunction-sections -fdata-sections -pipe
MCU=attiny13a
CPU_FREQ=4800000
CALIBRATION_VALUE=0x55
BINARY_DIR=build
BINARY_NAME=tinyfan.elf
BINARY_PATH=${BINARY_DIR}/${BINARY_NAME}

all: listing elfsize

listing: app
	${OBJDUMP} --disassemble --source --line-numbers --demangle ${BINARY_PATH} > $(BINARY_PATH:.elf=.asm)

elfsize: app
	${SIZE} -A ${BINARY_PATH}

app:
	mkdir -p build
	${CC} -mmcu=${MCU} -DF_CPU=${CPU_FREQ} -DCALIBRATION_VALUE=${CALIBRATION_VALUE} ${CFLAGS} src/*.c -o ${BINARY_PATH}

clean:
	rm -rf build