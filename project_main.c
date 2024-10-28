/* Includes */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/UART.h>
#include "Board.h"

/* Constants */
#define STACKSIZE 2048
#define ACC_THRESHOLD 0.1 // Define a threshold for motion detection
#define NUM_SAMPLES 50 // Number of samples to read for motion analysis

Char uartSendTaskStack[STACKSIZE];
Char uartReceiveTaskStack[STACKSIZE];

/* Global Variables */
float accelData[NUM_SAMPLES][7];
float readInterval = 0.1;
char morseMessage[100] = "";   
char receivedMessage[100] = ""; 

/* States */
typedef enum { INTERFACE, GET_CODE, SEND_CODE, RECEIVE_CODE, PRINT_RECEIVED, OFF, READING_DATA, RECEIVING_DATA } State;
State currentState = INTERFACE;

/* Button and LED Configuration */
static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;

PIN_Config buttonConfig[] = {
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE, // Button 0: Start/Stop reading data
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE, // Button 1: On/Off switch
    PIN_TERMINATE
};

PIN_Config ledConfig[] = {
    Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, // Green LED
    Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, // Red LED
    PIN_TERMINATE
};

/* Function Prototypes */
Void uartSendTask(UArg arg0, UArg arg1);
Void uartReceiveTask(UArg arg0, UArg arg1);
void buttonCallback(PIN_Handle handle, PIN_Id pinId);
void analyzeMotion(float accelData[][7], int sampleCount);
void collectSensorData();

/* Task Functions */
Void uartSendTask(UArg arg0, UArg arg1) {
    UART_Handle uart;
    UART_Params uartParams;

    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.baudRate = 9600;
    uartParams.readEcho = UART_ECHO_OFF;

    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
        System_abort("Error opening UART");
    }

    while (1) {
        System_printf("Sanity Check: uartSendTask loop entered\n");
        System_flush();

        if (currentState == READING_DATA) {
            // Example Morse code message; replace with actual analysis
            strcpy(morseMessage, ".- .- ... ..");
            UART_write(uart, morseMessage, strlen(morseMessage));
            UART_write(uart, "   ", 3); // End message with three spaces
        }
        Task_sleep(5000000 / Clock_tickPeriod); // 5 seconds
    }
}

Void uartReceiveTask(UArg arg0, UArg arg1) {
    UART_Handle uart;
    UART_Params uartParams;

    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.baudRate = 9600;
    uartParams.readEcho = UART_ECHO_OFF;

    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
        System_abort("Error opening UART");
    }

    while (1) {
        System_printf("Sanity Check: uartReceiveTask loop entered\n");
        System_flush();

        if (currentState == RECEIVING_DATA) {
            int bytesRead = UART_read(uart, receivedMessage, sizeof(receivedMessage) - 1);
            if (bytesRead > 0) {
                receivedMessage[bytesRead] = '\0';
                System_printf("Received Morse Code: %s\n", receivedMessage);
                System_flush();
            }
        }
        Task_sleep(1000000 / Clock_tickPeriod); // 1 second
    }
}

void analyzeMotion(float accelData[][7], int sampleCount) {
    // Example implementation of motion analysis based on accelerometer data
    for (int i = 0; i < sampleCount; i++) {
        float ax = accelData[i][1]; // x-axis acceleration
        float ay = accelData[i][2]; // y-axis acceleration
        float az = accelData[i][3]; // z-axis acceleration

        // Check for motion detection
        if (fabs(ax) > ACC_THRESHOLD || fabs(ay) > ACC_THRESHOLD) {
            // Implement your command recognition logic here
            System_printf("Motion detected at sample %d: ax=%f, ay=%f\n", i, ax, ay);
            System_flush();
            // Example: update morseMessage based on detected motion
        }
    }
}

void collectSensorData() {
    // Simulated function for reading data from the MPU9250
    // Fill the accelData array with new sensor readings
    // Replace this with actual sensor reading logic
    for (int i = 0; i < NUM_SAMPLES; i++) {
        accelData[i][0] = i * readInterval; // Time stamp
        accelData[i][1] = (rand() % 200 - 100) / 1000.0; // Simulated random acceleration x
        accelData[i][2] = (rand() % 200 - 100) / 1000.0; // Simulated random acceleration y
        accelData[i][3] = -1.0; // Gravity, z-axis (static)
    }

    analyzeMotion(accelData, NUM_SAMPLES);
}

void buttonCallback(PIN_Handle handle, PIN_Id pinId) {
    if (pinId == Board_BUTTON0) {
        System_printf("Button 0 pressed\n");
        System_flush();

        if (currentState == OFF) {
            currentState = READING_DATA;
            PIN_setOutputValue(ledHandle, Board_LED0, 1); // Green LED on
            PIN_setOutputValue(ledHandle, Board_LED1, 0); // Red LED off
            collectSensorData(); // Start collecting data
        } else if (currentState == READING_DATA) {
            currentState = RECEIVING_DATA;
            PIN_setOutputValue(ledHandle, Board_LED0, 0); // Green LED off
            PIN_setOutputValue(ledHandle, Board_LED1, 1); // Red LED on
        }
    } else if (pinId == Board_BUTTON1) {
        System_printf("Button 1 pressed\n");
        System_flush();

        currentState = OFF;
        PIN_setOutputValue(ledHandle, Board_LED0, 0);
        PIN_setOutputValue(ledHandle, Board_LED1, 0);
    }
}

Int main(void) {
    System_printf("Sanity Check: Entering main function\n");
    System_flush();

    Board_initGeneral();
    Board_initUART();

    ledHandle = PIN_open(&ledState, ledConfig);
    if (!ledHandle) {
        System_abort("Error initializing LED pins");
    }

    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if (!buttonHandle) {
        System_abort("Error initializing button pins");
    }

    if (PIN_registerIntCb(buttonHandle, buttonCallback) != 0) {
        System_abort("Error registering button callback");
    }

    Task_Params uartSendTaskParams;
    Task_Params_init(&uartSendTaskParams);
    uartSendTaskParams.stackSize = STACKSIZE;
    uartSendTaskParams.stack = &uartSendTaskStack;
    uartSendTaskParams.priority = 1;
    Task_create(uartSendTask, &uartSendTaskParams, NULL);

    Task_Params uartReceiveTaskParams;
    Task_Params_init(&uartReceiveTaskParams);
    uartReceiveTaskParams.stackSize = STACKSIZE;
    uartReceiveTaskParams.stack = &uartReceiveTaskStack;
    uartReceiveTaskParams.priority = 2;
    Task_create(uartReceiveTask, &uartReceiveTaskParams, NULL);

    System_printf("Sanity Check: Starting BIOS\n");
    System_flush();

    BIOS_start();

    return (0);
}
