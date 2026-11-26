#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <semphr.h>
#include <tusb.h>
//#include "/home/student/jtkj-projects/JTKJ-PicoRTOS-Project/libs/usb-serial-debug/include/usbSerialDebug/helper.h"
#include "usbSerialDebug/helper.h"
#include "tkjhat/sdk.h"


#define DEFAULT_STACK_SIZE 2048
#define CDC_ITF_DATA      1
#define BUFFER_SIZE 100
#define DEFAULT_I2C_SDA_PIN   12
#define DEFAULT_I2C_SCL_PIN   13


// Tehtävä 3: Tilakoneen esittely Add missing states.

enum state { WAITING=1, DATA_READY=2, DATA_RECEIVED=3};
enum state programState = WAITING;

char messageTx[BUFFER_SIZE];
char messageRx[BUFFER_SIZE];
char symbol;

// Ledin käyttöön tarkoittettu viestijono
static QueueHandle_t ledQueue = NULL;

static void btn_fxn(uint gpio, uint32_t eventMask) {


    if (gpio == SW1_PIN) {
        char selectedSymbol = symbol;
        char temp[2] = {symbol, '\0'};
        strcat(messageTx, temp);
        //blink_led(1);
        TickType_t ledDuration = 0;

        if (selectedSymbol == '.') {
            ledDuration = pdMS_TO_TICKS(50);
        }
        else if (selectedSymbol == '-') {
            ledDuration = pdMS_TO_TICKS(150);
        }
        //Led pyynnön lähetys
        if (ledDuration > 0 && ledQueue != NULL) {
            BaseType_t higherPriorityTaskWoken = pdFALSE;

            xQueueSendFromISR(
            ledQueue,
            &ledDuration,
            &higherPriorityTaskWoken
        );

        portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }

    }

    if (gpio == SW2_PIN && strlen(messageTx) > 0) {
        char temp2[4] = {' ', ' ' , '\n','\0'};
        strcat(messageTx, temp2);
        programState = DATA_READY;
    }
}

// I2C Mutex, jotta I2C väylää käyttäisi vain yksi laite kerrallaan
static SemaphoreHandle_t i2cMutex = NULL;

//Called by the USB task when a CDC interface receives data.
void tud_cdc_rx_cb(uint8_t itf) {
    // Reserve two extra bytes for the newline and string terminator.
    char buf[CFG_TUD_CDC_RX_BUFSIZE + 2];
    uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf) - 2);
    buf[count] = '\n';
    buf[count + 1] = '\0';

    // Also read CDC0 so that its receive buffer does not fill up.
    if (itf == 1 && count > 0) {
        //snprintf((char*)messageRx, sizeof(messageRx), "Viesti: %s", buf);
        strcat(messageRx, buf);

        programState = DATA_RECEIVED;
        if (tud_cdc_n_connected(0)) {
            // Nonblocking writes: do not use the waiting logger here.
            // Meaning, do not use usb_serial_print() or usb_serial_flush() here.
            const char *prefix = "\nReceived on CDC 1:";
            tud_cdc_n_write(0, prefix, strlen(prefix));
            // Include the appended newline, but not the null terminator.
            tud_cdc_n_write(0, buf, count + 1);
            tud_cdc_n_write_flush(0);
            
        }
        // Tarkista 
        if (tud_cdc_n_connected(1)) {
        //    tud_cdc_n_write(1, "OK\n", 3);
            tud_cdc_n_write_flush(1);
        }
    }
}

// soittoääni onnistuneesta lähetyksestä
static void play_tx_melody(void) {
    
    // nuottien taajuudet hertseinä
    const uint32_t notes[] = {
        587, 659, 784, 659, 988, 988, 880,
        587, 659, 784, 659, 880, 880, 784
    };
    // nuottien kesto millisekunteina
    const uint32_t durations[] = {
        80,  80,  80,  80,  460, 200, 700,
        80,  80,  80,  80,  460, 200, 600
    };
    const int count = sizeof(notes) / sizeof(notes[0]);

    for (int i = 0; i < count; i++) {
        buzzer_play_tone(notes[i], durations[i]);
        vTaskDelay(pdMS_TO_TICKS(50)); // tauko nuottien välissä
    }
}

// saapuvan morsetuksen soitto summerilla
static void play_morse_audio(const char *msg) {
    const uint32_t tone_hz = 600;
    const uint32_t dot_ms  = 100;
    const uint32_t dash_ms = 300;

    for (int i = 0; msg[i] != '\0'; i++) {
        if (msg[i] == '.') {
            buzzer_play_tone(tone_hz, dot_ms);
            vTaskDelay(pdMS_TO_TICKS(dot_ms));
        } else if (msg[i] == '-') {
            buzzer_play_tone(tone_hz, dash_ms);
            vTaskDelay(pdMS_TO_TICKS(dot_ms));
        } else if (msg[i] == ' ') {
            vTaskDelay(pdMS_TO_TICKS(300));  // tauko merkkien välissä
        }
    }
}


static void display_task(void *pvParameters) {
    (void)pvParameters;
    
    //init_display();
    //clear_display();

    //while (1) 
    //{
    //    if (programState == DATA_RECEIVED) {
    //        clear_display();
    //        write_text(messageRx);
    //        messageRx[0] = '\0';
            //write_text("Toimii");

    //        programState = WAITING;
    //    }


    //    vTaskDelay(pdMS_TO_TICKS(200));

    while (1) {

        // Tarkistetaan, onko USB:n kautta vastaanotettu viesti
        if (programState == DATA_RECEIVED) {

            // Pyydetään käyttöoikeutta I2C-väylään
            if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {

                // Tyhjennetään näyttö
                clear_display();

                // Kirjoitetaan vastaanotettu viesti näytölle
                write_text_xy(0, 0, messageRx);

                // Vapautetaan I2C-väylä
                xSemaphoreGive(i2cMutex);

                // Soitetaan summerilla vastaanotettu morsetus
                play_morse_audio(messageRx);

                // Tyhjennetään vastaanottopuskuri
                messageRx[0] = '\0';

                // Palautetaan ohjelma odotustilaan
                programState = WAITING;
            }
        }

        // Odotetaan 200 ms ennen seuraavaa tarkistusta
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

//Tehtävä, joka väläyttää lediä aina tietetyn ajan
static void led_task(void *pvParameters) {
    (void)pvParameters;
    TickType_t duration;

    while (1) {
        if (xQueueReceive(ledQueue, &duration, portMAX_DELAY) == pdPASS) {
            set_led_status(true);
            vTaskDelay(duration);
            set_led_status(false);
        }
    }
}


static void imu_task(void *pvParameters) {
    (void)pvParameters;

    
    float ax, ay, az, gx, gy, gz, t;
    // Setting up the sensor. 
    //if (init_ICM42670() == 0) {
    //    usb_serial_print("ICM-42670P initialized successfully!\n");
    //    if (ICM42670_start_with_default_values() != 0){
    //        usb_serial_print("ICM-42670P could not initialize accelerometer or gyroscope");
    //    }

    //} else {
    //    usb_serial_print("Failed to initialize ICM-42670P.\n");
    //}
    // Start collection data here. Infinite loop. 
    // Setting up the sensor with I2C Mutex protection
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {

        int initResult = init_ICM42670();
        int startResult = -1;

        if (initResult == 0) {
            startResult = ICM42670_start_with_default_values();
        }

        xSemaphoreGive(i2cMutex);

        if (initResult == 0) {
            usb_serial_print("ICM-42670P initialized successfully!\n");

        if (startResult != 0) {
            usb_serial_print("ICM-42670P could not initialize accelerometer or gyroscope\n");
        }
    } else {
        usb_serial_print("Failed to initialize ICM-42670P.\n");
    }
}
    char buf[BUFFER_SIZE];
    while (1)
    {
        //if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0 ){
        int readResult = -1;

        // Pyydetään käyttöoikeutta I2C-väylään
        if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {

        // Luetaan IMU-anturi
        readResult = ICM42670_read_sensor_data(
            &ax, &ay, &az,
            &gx, &gy, &gz, &t
        );

        // Vapautetaan I2C-väylä
        xSemaphoreGive(i2cMutex);
    }

    if (readResult == 0) {
            sprintf(buf,"X=%.2f, Y=%.2f, Z=%.2f \n", ax, ay, az);
            usb_serial_print(buf);
            if (az >= 0.8){
                symbol = '-';
                //usb_serial_print("Merkki on -\n");
            } else if (ax >= 0.8 || ax <= -0.8) {
                symbol = '.';
                //usb_serial_print("Merkki on .\n");
            } else if (az <= -0.8) {
                symbol = ' ';
                //usb_serial_print("Merkki on väli\n");
            } else {
                sprintf(buf,"Accel: X=%.2f, Y=%.2f, Z=%.2f \n", ax, ay, az);
                usb_serial_print(buf);
            }
            
        } else {
            usb_serial_print("Failed to read imu data\n");
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

}

static void print_task(void *arg){
    (void)arg;
    
    char buf[BUFFER_SIZE]; 

    // Wait for USB enumeration, not for either serial monitor to open.
    while (!tud_mounted()){
            vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (usb_serial_connected()) {
        usb_serial_print("=== Dual CDC Example Started ===\n");
        usb_serial_print("CDC0: Debug output (this interface)\n");
        usb_serial_print("CDC1: Sending serial data\n");
    }
    usb_serial_flush();

    while (1) {
        if (programState == DATA_READY) {
            if(tud_cdc_n_connected(CDC_ITF_DATA)){
                //Send them using tucdc_n_write to the ACM1
                snprintf(buf, BUFFER_SIZE,"%s\n", messageTx);
                tud_cdc_n_write(CDC_ITF_DATA,buf,strlen(buf));
                tud_cdc_n_write_flush(CDC_ITF_DATA);
                
            }
            //Send also the debug log to the ACM0
            if (usb_serial_connected()) {
                snprintf(buf, BUFFER_SIZE,"testi: %s\n", messageTx);
                usb_serial_print(buf);
                usb_serial_flush();
            }
            play_tx_melody();
            messageTx[0] = '\0';
            programState = WAITING;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
        
        // Exercise 4. Use usb_serial_print() instead of printf() for debug output
        //             produced after the USB serial library has been initialized.
        //             Do not replace error messages that can execute before usb_serial_init().
        //             Use the TinyUSB library to send data through the other serial port (CDC 1).
        //             You can use the functions at https://github.com/hathach/tinyusb/blob/master/src/class/cdc/cdc_device.h
        //             You can find an example at hello_dual_cdc
        //             The data written using this should be provided using csv
        //             timestamp, illuminance
        // Tehtävä 4. Käytä usb_serial_print()-funktiota printf()-funktion sijaan
        //            debug-tulostukseen sen jälkeen, kun USB-sarjaliikennekirjasto on alustettu.
        //            Älä korvaa virheilmoituksia, jotka voivat suorittua ennen usb_serial_init()-kutsua.
        //            Käytä TinyUSB-kirjastoa datan lähettämiseen toisen sarjaportin (CDC 1) kautta.
        //            Voit käyttää funktioita: https://github.com/hathach/tinyusb/blob/master/src/class/cdc/cdc_device.h
        //            Esimerkki löytyy hello_dual_cdc-projektista.
        //            Tällä menetelmällä kirjoitettu data tulee antaa CSV-muodossa:
        //            timestamp, illuminance

        // Do not remove this
        //vTaskDelay(pdMS_TO_TICKS(200));
}

static void usbTask(void *arg) {
    (void)arg;
    while (1) {
        tud_task();              // With FreeRTOS wait for events
                                 // Do not add vTaskDelay. 
    }
}

int main() {

    // Exercise 4: Comment the statement stdio_init_all(); 
    //             Instead, add AT THE END OF MAIN (before vTaskStartScheduler();) adequate statements to enable the TinyUSB library and the usb-serial-debug.
    //             You can see hello_dual_cdc for help
    //             In CMakeLists.txt add usb_serial_debug to target_link_libraries.
    //             In CMakeLists.txt deactivate pico_enable_stdio_usb

    //stdio_init_all();

    // Uncomment this lines if you want to wait till the serial monitor is connected
    /*while (!stdio_usb_connected()){
        sleep_ms(10);
    }*/ 
    
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    //  Alustetaan led, painikkeet, summeri ja näyttö
    init_led();
    init_sw1(); 
    init_sw2();
    init_buzzer();
    init_display();
    
    // Tyhjennetään näyttö käynnistyksessä
    clear_display();
    
    i2cMutex = xSemaphoreCreateMutex();

    if (i2cMutex == NULL) {
        printf("I2C Mutexin teko ei onnistunut \n");
        return 0;
    }

    ledQueue = xQueueCreate(8, sizeof(TickType_t));

    if (ledQueue == NULL) {
        printf("LED queue creation failed\n");
        return 0;
    }

    gpio_set_irq_enabled_with_callback(SW1_PIN, GPIO_IRQ_EDGE_FALL, true, btn_fxn);
    gpio_set_irq_enabled_with_callback(SW2_PIN, GPIO_IRQ_EDGE_FALL, true, btn_fxn);

    

    
    
    TaskHandle_t hImuTask, hPrintTask, hUSB, hDisplayTask = NULL;

    // Exercise 4: Uncomment this xTaskCreate to create the task that enables dual USB communication.
    // Tehtävä 4: Poista tämän xTaskCreate-rivin kommentointi luodaksesi tehtävän,
    // joka mahdollistaa kaksikanavaisen USB-viestinnän.

    
    xTaskCreate(usbTask, "usb", 2048, NULL, 3, &hUSB);
    #if (configNUMBER_OF_CORES > 1)
        vTaskCoreAffinitySet(hUSB, 1u << 0);
    #endif
    


    // Create the tasks with xTaskCreate
    BaseType_t result = xTaskCreate(imu_task, // (en) Task function
                "imu",                        // (en) Name of the task 
                DEFAULT_STACK_SIZE,              // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                            // (en) Arguments of the task 
                2,                               // (en) Priority of this task
                &hImuTask);                   // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Sensor task creation failed\n");
        return 0;
    }
    result = xTaskCreate(print_task,  // (en) Task function
                "print",              // (en) Name of the task 
                DEFAULT_STACK_SIZE,   // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                 // (en) Arguments of the task 
                2,                    // (en) Priority of this task
                &hPrintTask);         // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Print Task creation failed\n");
        return 0;
    }

    result = xTaskCreate(display_task,  // (en) Task function
                "display",              // (en) Name of the task 
                DEFAULT_STACK_SIZE,   // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                 // (en) Arguments of the task 
                2,                    // (en) Priority of this task
                &hDisplayTask);       // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Display task creation failed\n");
        return 0;
    }

    // Create LED task
    result = xTaskCreate(
        led_task,
        "led",
        DEFAULT_STACK_SIZE,
        NULL,
        2,
        NULL
    );

    if (result != pdPASS) {
        printf("LED task creation failed\n");
        return 0;
    }




    // VERY IMPORTANT, THIS SHOULD GO JUST BEFORE vTaskStartSheduler
    // WITHOUT ANY DELAYS. OTHERWISE, THE TinyUSB stack wont recognize
    // the device.
    // Initialize TinyUSB 
    tusb_init();
    //Initialize helper library to write in CDC0)
    usb_serial_init();

    // Start the scheduler (never returns)
    vTaskStartScheduler();
    
    // Never reach this line.
    return 0;
}
