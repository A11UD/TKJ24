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
#include "buzzer.h"

/* Extra header files */
#include "message.h"
#include "coders.h"

// Task variables
#define STACKSIZE 2048
Char mpuTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];
Char buzzerStack[STACKSIZE];

// Definition of the state machine
enum state {INTERFACE=0, SENDING_DATA, WAITING, READING_DATA, RECEIVING_DATA, DATA_READY};
enum state programState = INTERFACE;

// Constants
#define NUM_SAMPLES 20 // Max number of samples in motion data
#define AVG_WIN_SIZE 10 // Window size for calculation averages from raw data
#define CLOCK_PERIOD 10 // Clock task interrupt period in milliseconds
#define READ_WAIT 8000  // Wait time (ms) after last read character before repeating message to user

// Buffers
char txBuffer[4];
char rxBuffer[10];
msg TX_MESSAGE;
msg RX_MESSAGE;
char test[100];
uint8_t testI = 0;

// Arrays
float rawData[6][AVG_WIN_SIZE];
float motionData[6][NUM_SAMPLES];
/* Data = [[ax_1, ax_2, ax_3, ...],
 *         [ay_1, ay_2, ay_3, ...],
 *         [az_1, az_2, az_3, ...],
 *         [gx_1, gx_2, gx_3, ...],
 *         [gy_1, gy_2, gy_3, ...],
 *         [gz_1, gz_2, gz_3, ...]]
 */
uint32_t times[NUM_SAMPLES];
uint32_t maxTimes[6];
uint32_t minTimes[6];
float maxValues[6];
float minValues[6];
const char mario[] = {"--.-.-...---"};

typedef struct Note {
    uint16_t frequency;
    uint16_t duration; // In milliseconds
} Note;

// ChatGPT provided notes
Note song[] = {{659, 150}, // E5
               {659, 150}, // E5
               {0,   150}, // REST
               {659, 150}, // E5
               {0,   150}, // REST
               {523, 150}, // C5
               {659, 150}, // E5
               {0,   150}, // REST
               {784, 300}, // G5
               {0,   300}, // REST
               {392, 300}};// G4

// Variables
uint8_t dataIndex = 0;
uint8_t rawDataIndex = 0;
uint32_t time = 0;
uint32_t dataReadyTime = 0;
uint8_t dataReadyNum = 0;

// pins RTOS-variables and configuration here
static PIN_Handle button0Handle;
static PIN_State button0State;
static PIN_Handle button1Handle;
static PIN_State button1State;
static PIN_Handle ledHandle;
static PIN_State ledState;
static PIN_Handle mpuHandle;
static PIN_Handle hBuzzer;
static PIN_State sBuzzer;
static UART_Handle uart;
static UART_Params uartParams;

PIN_Config cBuzzer[] = {
  Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
  PIN_TERMINATE
};

PIN_Config mpuPinConfig[] = {
    Board_MPU_POWER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

PIN_Config button0Config[] = {
   Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};

PIN_Config button1Config[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};

PIN_Config powerButtonWakeConfig[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
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

uint8_t isSong() {
    if (RX_MESSAGE.count == 13) {
        uint8_t i = 0;
        for (; i < RX_MESSAGE.count; i++) {
            if (RX_MESSAGE.data[i] != mario[i]) {
                return 0;
            }
        }
        return 1;
    }
    return 0;
}

void movavg(float *fromArray, float *destArray) {
    uint8_t i = 0;
    float avg = 0;
    while (i < AVG_WIN_SIZE) {
        avg += fromArray[i];
        i++;
    }
    destArray[dataIndex] = avg / AVG_WIN_SIZE;
}

void getMaxMin() {
    uint8_t i = 0;
    uint8_t j = 0;
    for (; i < 6; i++) {
        j = 0;
        for (; j < NUM_SAMPLES; j++) {
            if (j == 0) {
                maxValues[i] = motionData[i][j];
                minValues[i] = motionData[i][j];
                continue;
            }
            if (motionData[i][j] > maxValues[i]) {
                maxValues[i] = motionData[i][j];
                maxTimes[i] = times[j];
            }
            if (motionData[i][j] < minValues[i]) {
                minValues[i] = motionData[i][j];
                minTimes[i] = times[j];
            }
        }
    }
}

uint8_t checkMoves() {
    getMaxMin();
    // Turn to left
    if (maxValues[1] > 0.6 && minValues[2] < 0.4) {
        if (maxValues[3] > 90.0 && minValues[3] < -90.0) {
            if (maxTimes[3] < minTimes[3]) {
                sprintf(txBuffer, ".\r\n\0");
                programState = SENDING_DATA;
                return 1;
            }
        }
    }
    // Turn to right
    if (minValues[1] < -0.6 && minValues[2] < 0.4) {
        if (maxValues[3] > 90.0 && minValues[3] < -90.0) {
            if (maxTimes[3] > minTimes[3]) {
                sprintf(txBuffer, "-\r\n\0");
                programState = SENDING_DATA;
                return 1;
            }
        }
    }
    return 0;
}

Void buzzerFxn(UArg arg0, UArg arg1) {
    while (1) {
        if (programState == DATA_READY) {
            uint16_t i = 0;
            System_printf(RX_MESSAGE.data);
            System_printf("\n");
            //test[testI] = '\0';
            //System_printf(test);
            //System_printf("\n");
            System_flush();
            if (isSong()) {
                uint16_t noteCount = sizeof(song) / sizeof(song[0]);
                for (; i < noteCount; i++) {
                    if (song[i].frequency == 0) {
                        delay(song[i].duration);
                    } else {
                        buzzerOpen(hBuzzer);
                        buzzerSetFrequency(song[i].frequency);
                        delay(song[i].duration);
                        buzzerClose();
                    }
                    delay(50);
                }
            } else {
                for(; i < RX_MESSAGE.count; i++) {
                    if (RX_MESSAGE.data[i] == '.') {
                        buzzerOpen(hBuzzer);
                        buzzerSetFrequency(8000);
                        delay(200);
                        buzzerClose();
                    }
                    else if (RX_MESSAGE.data[i] == '-') {
                        buzzerOpen(hBuzzer);
                        buzzerSetFrequency(2500);
                        delay(600);
                        buzzerClose();
                    }
                    else if (RX_MESSAGE.data[i] == ' ') {
                        buzzerOpen(hBuzzer);
                        buzzerSetFrequency(1000);
                        delay(1400);
                        buzzerClose();
                    }
                    delay(400);
                }
            }
            msgClear(&RX_MESSAGE);
            PIN_setOutputValue(ledHandle, Board_LED1, 0);
            programState = INTERFACE;
        }
        delay(100);
    }
}

Void clkFxn(UArg arg0) {
   // Clock tick = 10us
   time = Clock_getTicks() / 100; // Time in milliseconds
   if (programState == RECEIVING_DATA || (dataReadyTime == 0  && programState == WAITING)) {
       dataReadyTime = time;
   }
   if (dataReadyTime != 0) {
       if (time < dataReadyTime || (time - dataReadyTime) > READ_WAIT) {
           programState = DATA_READY;
           dataReadyTime = 0;
       }
   }
}

Void button1Fxn(PIN_Handle handle, PIN_Id pinId) {
    if (programState == INTERFACE) {
        /*
        PIN_setOutputValue(ledHandle, Board_LED1, 1);
        msgDestroy(&TX_MESSAGE);
        msgDestroy(&RX_MESSAGE);

        delay(500);

        PIN_close(button1Handle);
        PIN_setOutputValue(ledHandle, Board_LED1, 0);
        PINCC26XX_setWakeup(powerButtonWakeConfig);
        Power_shutdown(NULL,0);
        */
    } else if (programState == READING_DATA) {
        sprintf(txBuffer, " \r\n\0");
        programState = SENDING_DATA;
    }
}

void button0Fxn(PIN_Handle handle, PIN_Id pinId) {
    if (pinId == Board_BUTTON0) {
        if (programState == INTERFACE) {
            PIN_setOutputValue(ledHandle, Board_LED0, 1);
            programState = READING_DATA;
        } else if (programState == READING_DATA) {
            PIN_setOutputValue(ledHandle, Board_LED0, 0);
            programState = INTERFACE;
        }
    }
}

void readCallback(UART_Handle uart, void *buffer, size_t len) {
    char *receivedChr = (char *)buffer;
    programState = RECEIVING_DATA;

    if (PIN_getOutputValue(Board_LED1) == 0) {
        PIN_setOutputValue(ledHandle, Board_LED1, 1);
    }
    if (receivedChr[0] == ' ' || receivedChr[0] == '-' || receivedChr[0] == '.') {
        msgAppend(&RX_MESSAGE, receivedChr[0]);
        test[testI] = receivedChr[0];
        testI++;
    }
    programState = WAITING;
    UART_read(uart, rxBuffer, 1);
}


void writeCallback(UART_Handle uart, void *buffer, size_t len) {
    programState = READING_DATA;
}


Void uartTaskFxn(UArg arg0, UArg arg1) {

    // Initialize UART parameters
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.writeMode = UART_MODE_CALLBACK;
    uartParams.writeCallback = writeCallback;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.readMode = UART_MODE_CALLBACK;
    uartParams.readCallback = readCallback;
    uartParams.baudRate = 9600;
    uartParams.dataLength = UART_LEN_8;
    uartParams.parityType = UART_PAR_NONE;
    uartParams.stopBits = UART_STOP_ONE;

    // Open connection to the device with default port Board_UART0
    uart = UART_open(Board_UART, &uartParams);
    if (uart == NULL) {
        System_abort("Error in opening UART");
    }
    UART_read(uart, rxBuffer, 1);

    while (1) {
        if(programState == SENDING_DATA) {
            PIN_setOutputValue(ledHandle, Board_LED0, 0);
            // Send data string with UART
            int8_t wBytes = UART_write(uart, txBuffer, strlen(txBuffer));
            if (wBytes < 0) {
                System_abort("Error in UART_write!");
            }
            delay(250);
            PIN_setOutputValue(ledHandle, Board_LED0, 1);
        }
        // Once per second, you can modify this
        delay(100);
    }
}

Void mpuTaskFxn(UArg arg0, UArg arg1) {

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
        System_abort("Error on initializing I2CMPU!");
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
            mpu9250_get_data(&i2cMPU, &rawData[0][rawDataIndex],
                             &rawData[1][rawDataIndex],
                             &rawData[2][rawDataIndex],
                             &rawData[3][rawDataIndex],
                             &rawData[4][rawDataIndex],
                             &rawData[5][rawDataIndex]);
            rawDataIndex = (rawDataIndex + 1) % AVG_WIN_SIZE;
            if (rawDataIndex == 0) {
                if (dataReadyNum < 20) {
                    dataReadyNum++;
                }
                uint8_t i = 0;
                for(;i < 6; i++) {
                    movavg(rawData[i], motionData[i]);
                }
                times[dataIndex] = time;
                dataIndex = (dataIndex + 1) % NUM_SAMPLES;
                if (dataIndex % 5 == 0 && dataReadyNum >= 20) {
                    if (checkMoves() == 1) {
                        dataReadyNum = 0;
                    }
                }
            }
        }
        delay(1); // Sleep 1ms
    }
}

Int main(void) {

    // Task variables
    Task_Handle mpuTaskHandle;
    Task_Params mpuTaskParams;

    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    Clock_Handle clkHandle;
    Clock_Params clkParams;

    Task_Handle buzzerTaskHandle;
    Task_Params buzzerParams;

    // Initialize board
    Board_initGeneral();

    // Initialize i2c bus
    Board_initI2C();

    // Initialize UART
    Board_initUART();

    // Initialize clock
    Clock_Params_init(&clkParams);
    clkParams.period = (CLOCK_PERIOD*1000) / Clock_tickPeriod;
    clkParams.startFlag = TRUE;

    // Initialize message structs
    msgInit(&TX_MESSAGE);
    msgInit(&RX_MESSAGE);

    // Initialize Buzzer handle
    hBuzzer = PIN_open(&sBuzzer, cBuzzer);
    if (hBuzzer == NULL) {
      System_abort("Error buzzer pin failed to open!");
    }

    // Open LED handle
    ledHandle = PIN_open(&ledState, ledConfig);
    if(!ledHandle) {
       System_abort("Error initializing LED pin!");
    }

    // Create clock handle
    clkHandle = Clock_create((Clock_FuncPtr)clkFxn, clkParams.period, &clkParams, NULL);
    if (clkHandle == NULL) {
       System_abort("Error clock creation failed!");
    }

    // Create button0 handle
    button0Handle = PIN_open(&button0State, button0Config);
    if(!button0Handle) {
       System_abort("Error initializing button0 pin!");
    }

    // Register interrupt function for button0
    if (PIN_registerIntCb(button0Handle, &button0Fxn) != 0) {
       System_abort("Error registering button0 callback function!");
    }

    // Create button1 handle
    button1Handle = PIN_open(&button1State, button1Config);
    if(!button1Handle) {
       System_abort("Error initializing button1 pin!");
    }

    // Register interrupt function for button1
    if (PIN_registerIntCb(button1Handle, &button1Fxn) != 0) {
       System_abort("Error registering button1 callback function!");
    }


    // Create Buzzer task
    Task_Params_init(&buzzerParams);
    buzzerParams.stackSize = STACKSIZE;
    buzzerParams.stack = &buzzerStack;
    buzzerTaskHandle = Task_create((Task_FuncPtr)buzzerFxn, &buzzerParams, NULL);
    if (buzzerTaskHandle == NULL) {
      System_abort("Error buzzer task creation failed!");
    }

    // Initialize MPU task parameters and create MPU task handle
    Task_Params_init(&mpuTaskParams);
    mpuTaskParams.stackSize = STACKSIZE;
    mpuTaskParams.stack = &mpuTaskStack;
    mpuTaskParams.priority = 2;
    mpuTaskHandle = Task_create(mpuTaskFxn, &mpuTaskParams, NULL);
    if (mpuTaskHandle == NULL) {
        System_abort("Error MPU task creation failed!");
    }

    // Initialize UART task parameters and create UART task handle
    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority = 2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);
    if (uartTaskHandle == NULL) {
        System_abort("Error UART task creation failed!");
    }

    // Sanity check
    System_printf("Hello world!\n");
    System_flush();

    // Start BIOS
    BIOS_start();

    return (0);
}
