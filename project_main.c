/* C Standard library */
#include <stdio.h>
#include <string.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

/* Board Header files */
#include "Board.h"
#include "sensors/mpu9250.h"

// Task variables
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];

// Definition of the state machine
enum state {INTERFACE = 1, SENDING_DATA, OFF, READING_DATA, RECEIVING_DATA};
enum state programState = INTERFACE;

// Global variable for ambient light
#define NUM_SAMPLES 50 // Number of samples to read for motion analysis

float motionData[6][NUM_SAMPLES];
char debugString[1000];
uint8_t dataIndex = 0;
uint32_t time = 0;

// Add pins RTOS-variables and configuration here
static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;
static PIN_Handle mpuHandle;

PIN_Config mpuPinConfig[] = {
    Board_MPU_POWER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

PIN_Config buttonConfig[] = {
   Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};

PIN_Config ledConfig[] = {
   Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};

// MPU uses its own I2C interface
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
    if (pinId == Board_BUTTON0) {
        if (programState == INTERFACE) {
            PIN_setOutputValue(ledHandle, Board_LED0, !PIN_getOutputValue(Board_LED0));
            programState = READING_DATA;
        } else if (programState == READING_DATA || programState == SENDING_DATA) {
            PIN_setOutputValue(ledHandle, Board_LED0, !PIN_getOutputValue(Board_LED0));
            programState = INTERFACE;
        }

    } else if (pinId == Board_BUTTON1) {
        PIN_setOutputValue(ledHandle, Board_LED1, !PIN_getOutputValue(Board_LED1));
    }
}

Void uartTaskFxn(UArg arg0, UArg arg1) {

    UART_Handle uart;
    UART_Params uartParams;

    // Initialize UART parameters
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.readMode = UART_MODE_BLOCKING;
    uartParams.baudRate = 9600;
    uartParams.dataLength = UART_LEN_8;
    uartParams.parityType = UART_PAR_NONE;
    uartParams.stopBits = UART_STOP_ONE;

    // Open connection to the device with default port Board_UART0
    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
        System_abort("Error in opening UART");
    }

    while (1) {
        if(programState == SENDING_DATA) {
            // Send light sensor data string with UART
            UART_write(uart, debugString, strlen(debugString));

            if (PIN_getOutputValue(Board_LED0)) {
                programState = READING_DATA;
            }
        }

        // Once per second, you can modify this
        delay(100);
    }
}

Void sensorTaskFxn(UArg arg0, UArg arg1) {

    // RTOS i2c-variables initialization
    I2C_Handle i2cMPU;
    I2C_Params i2cMPUParams;

    // Variable for i2c-message structure
    // I2C_Transaction i2cMessage;

    // Open the i2c bus
    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;


    // MPU power on
    PIN_setOutputValue(mpuHandle, Board_MPU_POWER, Board_MPU_POWER_ON);
    delay(100);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // Open I2C connection
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL) {
        System_abort("Error on initializing I2CMPU");
    }

    // Setup the OPT3001 sensor for use
    // Before calling the setup function, insert 100ms delay with Task_sleep
    delay(100);
    mpu9250_setup(&i2cMPU);
    System_printf("MPU9250: Setup and calibration OK\n");
    System_flush();

    while (1) {

        // Read sensor data and print it to the Debug window as string
        // Save the sensor value into the global variable and modify state
        if(programState == READING_DATA) {
            mpu9250_get_data(&i2cMPU, &motionData[0][dataIndex],
                             &motionData[1][dataIndex],
                             &motionData[2][dataIndex],
                             &motionData[3][dataIndex],
                             &motionData[4][dataIndex],
                             &motionData[5][dataIndex]);
            sprintf(debugString, "Time:%d, index:%d, Data: [%.2f %.2f %.2f %.2f %.2f %.2f]\n\r", time, dataIndex, motionData[0][dataIndex],
                        motionData[1][dataIndex],
                        motionData[2][dataIndex],
                        motionData[3][dataIndex],
                        motionData[4][dataIndex],
                        motionData[5][dataIndex]);
            if (PIN_getOutputValue(Board_LED0)) {
                programState = SENDING_DATA;
            }
            time += 10;
            dataIndex = (dataIndex + 1) % NUM_SAMPLES;
        }

        // Once per second, you can modify this
        delay(100);
    }
}

Int main(void) {

    // Task variables
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    // Initialize board
    Board_initGeneral();

    // Initialize i2c bus
    Board_initI2C();

    // Initialize UART
    Board_initUART();

    // Open LED handle
    ledHandle = PIN_open(&ledState, ledConfig);
    if(!ledHandle) {
       System_abort("Error initializing LED pin\n");
    }

    // Open button handle
    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if(!buttonHandle) {
       System_abort("Error initializing button pin\n");
    }

    // Open interrupt button handle
    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
       System_abort("Error registering button callback function");
    }

    // Initialize sensor task parameters
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority = 2;
    sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);
    if (sensorTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    // Initialize UART task parameters
    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority = 2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);
    if (uartTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    // Sanity check
    System_printf("Hello world!\n");
    System_flush();

    // Start BIOS
    BIOS_start();

    return (0);
}
