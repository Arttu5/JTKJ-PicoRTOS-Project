
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include <tusb.h>
//#include "/home/student/jtkj-projects/JTKJ-PicoRTOS-Project/libs/usb-serial-debug/include/usbSerialDebug/helper.h"
#include "usbSerialDebug/helper.h"
#include "tkjhat/sdk.h"


// Exercise 4. Include the libraries necessaries to use the usb-serial-debug, and tinyusb
// Tehtävä 4 . Lisää usb-serial-debugin ja tinyusbin käyttämiseen tarvittavat kirjastot.



#define DEFAULT_STACK_SIZE 2048
#define CDC_ITF_DATA      1
#define BUFFER_SIZE 100


// Tehtävä 3: Tilakoneen esittely Add missing states.
// Exercise 3: Definition of the state machine. Add missing states.
enum state { WAITING=1, DATA_READY=2, DATA_RECEIVED=3};
enum state programState = WAITING;

char messageTx[BUFFER_SIZE];
char messageRx[BUFFER_SIZE];
char symbol;

static void btn_fxn(uint gpio, uint32_t eventMask) {
    //toggle_led();


    if (gpio == SW1_PIN) {
        char temp[3] = {symbol, ' ', '\0'};
        strcat(messageTx, temp);
        blink_led(1);
    }

    if (gpio == SW2_PIN && strlen(messageTx) > 0) {
        programState = DATA_READY;
    }
}

//Called by the USB task when a CDC interface receives data.
void tud_cdc_rx_cb(uint8_t itf) {
    // Reserve two extra bytes for the newline and string terminator.
    char buf[CFG_TUD_CDC_RX_BUFSIZE + 2];
    uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf) - 2);
    buf[count] = '\n';
    buf[count + 1] = '\0';

    // Also read CDC0 so that its receive buffer does not fill up.
    if (itf == 1 && count > 0) {
        snprintf((char*)messageRx, sizeof(messageRx), "Viesti: %s", buf);
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

        if (tud_cdc_n_connected(1)) {
            tud_cdc_n_write(1, "OK\n", 3);
            tud_cdc_n_write_flush(1);
        }
    }
}

static void display_task(void *pvParameters) {
    (void)pvParameters;

    init_display();

    while (1) 
    {
        if (programState == DATA_RECEIVED) {
            clear_display();
            write_text(messageRx);

            programState = WAITING;
        }


        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
}

static void imu_task(void *pvParameters) {
    (void)pvParameters;

    
    float ax, ay, az, gx, gy, gz, t;
    // Setting up the sensor. 
    if (init_ICM42670() == 0) {
        usb_serial_print("ICM-42670P initialized successfully!\n");
        if (ICM42670_start_with_default_values() != 0){
            usb_serial_print("ICM-42670P could not initialize accelerometer or gyroscope");
        }

    } else {
        usb_serial_print("Failed to initialize ICM-42670P.\n");
    }
    // Start collection data here. Infinite loop. 
    //uint8_t buf[BUFFER_SIZE];
    while (1)
    {
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {
            if (az >= 0.8){
                symbol = '-';
            } else if (ax <= -0.8) {
                symbol = '.';
            } else {
                symbol = ' ';
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


// Exercise 4: Uncomment the following line to activate the TinyUSB library.  
// Tehtävä 4:  Poista seuraavan rivin kommentointi aktivoidaksesi TinyUSB-kirjaston. 


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
    // Tehtävä 4:  Kommentoi lause stdio_init_all();
    //             Sen sijaan lisää MAIN LOPPUUN (ennen vTaskStartScheduler();) tarvittavat komennot aktivoidaksesi TinyUSB-kirjaston ja usb-serial-debugin.
    //             Voit katsoa apua esimerkistä hello_dual_cdc.
    //             Lisää CMakeLists.txt-tiedoston target_link_libraries-kohtaan usb_serial_debug.
    //             Poista CMakeLists.txt-tiedostosta käytöstä pico_enable_stdio_usb

    //stdio_init_all();

    // Uncomment this lines if you want to wait till the serial monitor is connected
    /*while (!stdio_usb_connected()){
        sleep_ms(10);
    }*/ 
    
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    // Exercise 1: Initialize the button and the led and define an register the corresponding interrupton.
    //             Interruption handler is defined up as btn_fxn
    // Tehtävä 1:  Alusta painike ja LEd ja rekisteröi vastaava keskeytys.
    //             Keskeytyskäsittelijä on määritelty yläpuolella nimellä btn_fxn

    init_led();
    init_sw1(); 
    init_sw2();

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
