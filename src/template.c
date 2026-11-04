
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE 2048
#define BUFFER_SIZE 100

//Add here necessary states
enum state { IDLE=1 };
enum state programState = IDLE;

static void sensor_task(void *arg){
    (void)arg;

    float ax, ay, az, gx, gy, gz, t;
    // Setting up the sensor. 
    if (init_ICM42670() == 0) {
        usb_serial_print("ICM-42670P initialized successfully!\n");
        if (ICM42670_start_with_default_values() != 0){
            usb_serial_print("ICM-42670P could not initialize accelerometer or gyroscope");
        }
        /*int _enablegyro = ICM42670_enable_accel_gyro_ln_mode();
        usb_serial_print ("Enable gyro: %d\n",_enablegyro);
        int _gyro = ICM42670_startGyro(ICM42670_GYRO_ODR_DEFAULT, ICM42670_GYRO_FSR_DEFAULT);
        usb_serial_print ("Gyro return:  %d\n", _gyro);
        int _accel = ICM42670_startAccel(ICM42670_ACCEL_ODR_DEFAULT, ICM42670_ACCEL_FSR_DEFAULT);
        usb_serial_print ("Accel return:  %d\n", _accel);*/
    } else {
        usb_serial_print("Failed to initialize ICM-42670P.\n");
    }

    uint8_t buf[BUFFER_SIZE];

    for(;;){
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {
            sprintf(buf,"Accel: X=%.2f, Y=%.2f, Z=%.2f | Gyro: X=%.2f, Y=%.2f, Z=%.2f| Temp: %2.2f°C\n", ax, ay, az, gx, gy, gz, t);
            usb_serial_print(buf);

        } else {
            usb_serial_print("Failed to read imu data\n");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main() {
    stdio_init_all();
    // Uncomment this lines if you want to wait till the serial monitor is connected
    /*while (!stdio_usb_connected()){
        sleep_ms(10);
    }*/ 
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    TaskHandle_t myExampleTask = NULL;
    // Create the tasks with xTaskCreate
    BaseType_t result = xTaskCreate(sensor_task,       // (en) Task function
                "example",              // (en) Name of the task 
                DEFAULT_STACK_SIZE, // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,               // (en) Arguments of the task 
                2,                  // (en) Priority of this task
                &myExampleTask);    // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Example Task creation failed\n");
        return 0;
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}

