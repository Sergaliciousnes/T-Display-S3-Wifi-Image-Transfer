#define PLATFORM_LCDA

#include <Arduino.h>

#include <SPI.h>
#include <TFT_eSPI.h>

#include "OneButton.h"

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>

#include <LittleFS.h>
#define FileSys LittleFS

#if defined(PLATFORM_LCD)
    #include "pin_config.h"
    const uint8_t HEIGHT = 170;
    const uint16_t WIDTH = 320;
    uint8_t backlight_pin = PIN_LCD_BL;
#elif defined(PLATFORM_LCDA)
    #include "pins_config.h"
    #include "rm67162.h"
    const uint8_t HEIGHT = 240;
    const uint16_t WIDTH = 536;
    uint8_t backlight_pin = PIN_LED;
#endif

void listDir(fs::FS &fs, const char * dirname, uint8_t levels=0);
void writeBinaryData(const char *path, const uint8_t *data, size_t size);
void readFile(const char *path);
uint8_t read_status_register();
void load_image_over_spi();
void write_image_to_spi();


TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
TFT_eSprite text_sprite = TFT_eSprite(&tft);

WiFiMulti WM;
WebSocketsClient webSocket;

const char* wifi_ssid     = "YourSSID";
const char* wifi_password = "YourPassword";

const char* ws_local_ip   = "192.168.1.115";

char text[((int)HEIGHT/9)+1][256];
uint8_t textIndex = 0;
uint8_t max_console_size = ((int)HEIGHT/9)+1;
int time_since_last_activity = 0;

uint16_t* ps_image;
int memoffset = 0;
int x_offset = 0;
int y_offset = 0;

OneButton button1(PIN_BUTTON_1, true);
OneButton button2(PIN_BUTTON_2, true);

SPIClass *spif;
SPISettings spi_settings = SPISettings(40000000, MSBFIRST, SPI_MODE0);
#define CS_PIN 10

bool ws_failed = false;
bool connected = false;

uint8_t menu_index = 0;
uint8_t menu_option_index = 0;
uint8_t menu_option_count = 5;

uint8_t menu_filenum = 0;
char menu_filename[32];



uint16_t image_width = 0;
uint8_t image_height = 0;

bool image_loaded = false;

//menu title and option strings
char menu_title[32] = {"Main Menu"};
char menu_options[5][32] = {"Power", "Files", "Placeholder", "Send", "Exit"};

char file_menu_title[32] = {"Files Menu"};
char file_menu_options[6][32] = {"Load Flash", "Write Flash", "SPI", "Format", "Back"};

char spi_menu_title[32] = {"SPI Menu"};
char spi_menu_options[4][32] = {"Load SPI", "Write SPI", "SPI Info", "Back"};

char select_menu_title[32];
char select_menu_options[3][32] = {"+", "-", "Confirm"};

char confirm_menu_title[32] = {"Confirm Menu"};
char confirm_menu_options[3][32] = {"Yes", "No"};

void moveText() {
    if (textIndex < max_console_size-1) return;
    for (int i = 1; i < (max_console_size+1); i++) {
        strcpy(text[i - 1], text[i]);
    }
}

void drawText() {
    for (int i = 0; i < (max_console_size+1); i++) {
        text_sprite.drawString(text[i], 0, i * 9);
    }
}

void apendText(const char *textdata) {
    sprintf(text[textIndex], "%s", textdata);
    moveText();
    textIndex++;
    if (textIndex >= max_console_size) textIndex = max_console_size-1;
    time_since_last_activity = 0;
}

#if defined(PLATFORM_LCD)
void draw_frame() {
    sprite.pushSprite(0, 0);
}
#elif defined(PLATFORM_LCDA)
void draw_frame() {
    lcd_PushColors(0, 0, 536, 240, (uint16_t*)sprite.getPointer());
}
#endif

//function to draw a menu, takes menu_title and menu_options 2d array
void draw_menu(char* menu_title, char menu_options[][32], uint8_t option_count) {

    int menu_height = 10 * option_count + 20;
    int menu_width = 100;

    int menu_y_offset = (HEIGHT - menu_height) / 2;
    int menu_x_offset = (WIDTH - menu_width) / 2;

    sprite.fillRect(menu_x_offset, menu_y_offset, menu_width, menu_height, TFT_BLUE);

    sprite.setTextColor(TFT_WHITE, TFT_BLUE);

    sprite.drawString(menu_title, menu_x_offset + 10, menu_y_offset + 5);

    for (int i = 0; i < option_count; i++) {
        if (i == menu_option_index) {
            sprite.setTextColor(TFT_RED, TFT_BLUE);
        } else {
            sprite.setTextColor(TFT_WHITE, TFT_BLUE);
        }
        
        sprite.drawString(menu_options[i], menu_x_offset + 10, menu_y_offset + 15 + (i * 10));
    }
    
}

void make_decision() {
    switch (menu_index) {
        case 1:
            //main menu
            switch (menu_option_index) {
                case 0:
                    //sleep mode
                    digitalWrite(backlight_pin, LOW);
                    webSocket.setReconnectInterval(50000);
                    webSocket.disconnect();
                    WiFi.mode(WIFI_OFF);

                    #if defined(PLATFORM_LCD)
                    digitalWrite(PIN_LCD_RES, LOW);
                    #elif defined(PLATFORM_LCDA)
                    digitalWrite(TFT_RES, LOW);
                    #endif


                    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BUTTON_2, 0);
                    esp_deep_sleep_start();
                    break;
                
                case 1:
                    //enter file menu
                    menu_index = 2;
                    menu_option_index = 0;
                    menu_option_count = 5;
                    break;
                
                case 2:
                    if (ws_failed) {
                        ws_failed = 0;
                        strcpy(menu_options[2], "Connecting");
                    } else {
                        ws_failed = 1;
                        webSocket.disconnect();
                        strcpy(menu_options[2], "Connect");
                    }
                    break;
                
                case 3:
                    webSocket.sendTXT("Test");
                    menu_index = 0;
                    break;

                case 4:
                    //close menu
                    menu_index = 0;
                    break;
            }
            break;

        case 2:
            //file menu
            switch (menu_option_index) {
                case 0:
                    //open load image menu
                    menu_index = 4;
                    menu_option_index = 0;
                    menu_option_count = 3;
                    break;
                case 1:
                    //open write image menu
                    menu_index = 5;
                    menu_option_index = 0;
                    menu_option_count = 3;
                    break;
                
                case 2:
                    //Open SPI Menu
                    menu_index = 3;
                    menu_option_index = 0;
                    menu_option_count = 4;
                    break;
                    
                    //load_image_over_spi();
                
                case 3:
                    //open confirm format menu
                    menu_index = 6;
                    menu_option_index = 1;
                    menu_option_count = 2;

                    //write_image_to_spi();
                    break;
                case 4:
                    //return to submenu
                    menu_index -= 1;
                    menu_option_index = 0;
                    menu_option_count = 5;
                    break;

                    //read status register
                    //read_status_register();
                    break;
            }
            break;
        
        case 3:
            //spi menu
            switch (menu_option_index) {
                case 0:
                    //load from spi
                    load_image_over_spi();
                    break;

                case 1:
                    //write to spi
                    write_image_to_spi();
                    break;
                
                case 2:
                    //read spi status
                    read_status_register();
                    break;
                
                case 3:
                    //return to submenu
                    menu_index -= 1;
                    menu_option_index = 2;
                    menu_option_count = 5;
                    break;
            
            }
            break;
        
        case 4:
            //load image menu
        case 5:
            //write image menu
            switch (menu_option_index) {
                case 0:
                    //increment file number
                    menu_filenum += 1;
                    break;
                
                case 1:
                    //decrement file number
                    menu_filenum -= 1;
                    if (menu_filenum < 0) {
                        menu_filenum = 0;
                    }
                    break;
                
                case 2:
                    //comfirm and return to menu
                    if (menu_index == 4) {
                        //load image with number
                        sprintf(menu_filename, "/image%d", menu_filenum);
                        readFile(menu_filename);
                    } else {
                        //write image with number
                        sprintf(menu_filename, "/image%d", menu_filenum);
                        writeBinaryData(menu_filename, (uint8_t*)ps_image, HEIGHT*WIDTH*sizeof(uint16_t));
                    }

                    menu_index = 2;
                    menu_option_index = 0;
                    menu_option_count = 5;
                    break;
            }
            break;
        
        case 6:
            //confirm format menu
            switch (menu_option_index) {
                case 0:
                    //format
                    LittleFS.format();
                    menu_index = 2;
                    menu_option_index = 3;
                    menu_option_count = 5;
                    break;
                
                case 1:
                    //return to menu
                    menu_index = 2;
                    menu_option_index = 3;
                    menu_option_count = 5;
                    break;
            }
            break;
    }
}

void set_offset() {
    if (image_height < HEIGHT) {
        y_offset = (int)((HEIGHT - image_height) / 2);
    } else {
        y_offset = 0;
    }

    if (image_width < WIDTH) {
        x_offset = (int)((WIDTH - image_width) / 2);
    } else {
        x_offset = 0;
    }
}

void hexdump(const void *mem, uint32_t len, uint8_t cols = 16) {
    const uint8_t* src = (const uint8_t*) mem;

    Serial.printf("\n[HX] Adr: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);

    char textdata[255];
    sprintf(textdata, "\n[HX] Adr: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
    apendText(textdata);

    if (len == 0x06) {
        if (src[0] == 0x1F && src[1] == 0x8B) {
            image_height = src[2];
            image_width = src[3] + src[4] + src[5];
            image_loaded = true;

            set_offset();

            memoffset = 0;

            return;
        }
    }

    memcpy(ps_image + memoffset, mem, len);
    memoffset += (int)len/2;
    
    return;

    //unused
    char linedata[255];

    uint32_t i = 0;
    
    for(i = 0; i < len; i++) {
        if(i % cols == 0) {
            Serial.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
            sprintf(textdata, "\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
            apendText(textdata);
        }
        
        Serial.printf("%02X ", *src);
        sprintf(textdata, "%02X ", *src);

        if (strlen(linedata) >= 48) {
            apendText(linedata);
            sprintf(linedata, "\n");
        }

        strcat(linedata, textdata);
        src++;
    }
    
    if (strlen(linedata) > 0) {

        sprintf(textdata, "\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
        apendText(textdata);
        apendText(linedata);
    }

    Serial.printf("\n");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

    char textdata[255];

    switch(type) {
        case WStype_DISCONNECTED:
            connected = 0;
            Serial.printf("[WSc] Disconnected!\n");
            apendText("Server Disconnected");
            apendText("Open Menu to Reconnect");

            //update the menu text on disconnect
            strcpy(menu_options[2], "Connect");

            ws_failed = 1;
            break;

        case WStype_CONNECTED:
            connected = 1;
            Serial.printf("[WSc] Connected to url: %s\n", payload);

            #if defined(PLATFORM_LCD)
            webSocket.sendTXT("ESP");
            #elif defined(PLATFORM_LCDA)
            webSocket.sendTXT("ESPA");
            #endif

            //update the menu text on connect
            strcpy(menu_options[2], "Disconnect");
            break;

        case WStype_TEXT:
            Serial.printf("[WSc] Text recv: %s\n", payload);
            sprintf(textdata, "%s", payload);
            apendText(textdata);
            break;

        case WStype_BIN:
            Serial.printf("[WSc] bytes recv, length: %u\n", length);
            hexdump(payload, length);
            break;

        case WStype_ERROR:
            Serial.printf("[WSc] Disconnected!\n");
            apendText("WS Error");
            break;

        case WStype_FRAGMENT_TEXT_START:
        
        case WStype_FRAGMENT_BIN_START:
        
        case WStype_FRAGMENT:
        
        case WStype_FRAGMENT_FIN:
            break;
    }

}

void update_screen() {
    sprite.fillSprite(TFT_BLACK);

    if (image_loaded) {
        sprite.pushImage(x_offset, y_offset, image_width, image_height, ps_image);
    }

    if (time_since_last_activity <= 40) {

        if (!connected) {
            sprite.fillRect(WIDTH-10, 0, 10, 10, TFT_RED);
        } else {
            sprite.fillRect(WIDTH-10, 0, 10, 10, TFT_GREEN);
        }

        text_sprite.fillSprite(TFT_BLACK);
        drawText();
        text_sprite.pushToSprite(&sprite, 0, 0, TFT_BLACK);
    }

    switch (menu_index) {
        
        case 1:
            draw_menu(menu_title, menu_options, 5);
            break;
        
        case 2:
            draw_menu(file_menu_title, file_menu_options, 5);
            break;
        
        case 3:
            draw_menu(spi_menu_title, spi_menu_options, 4);
            break;
        
        case 4:
            sprintf(menu_filename, "/image%d", menu_filenum);

            if (LittleFS.exists(menu_filename)) {
                sprintf(select_menu_title, "Load Image%d*", menu_filenum);
            } else {
                sprintf(select_menu_title, "Load Image%d", menu_filenum);
            }

            draw_menu(select_menu_title, select_menu_options, 3);
            break;
        
        case 5:
            sprintf(menu_filename, "/image%d", menu_filenum);

            if (LittleFS.exists(menu_filename)) {
                sprintf(select_menu_title, "Write Image%d*", menu_filenum);
            } else {
                sprintf(select_menu_title, "Write Image%d", menu_filenum);
            }
            
            draw_menu(select_menu_title, select_menu_options, 3);
            break;
        
        case 6:
            sprintf(confirm_menu_title, "Format Flash?");
            draw_menu(confirm_menu_title, confirm_menu_options, 2);
            break;
    
    }

    draw_frame();
}

void readFile(const char *path) {
    Serial.println("Reading File: " + String(path));
    apendText("Loading image file");
    update_screen();

    char path_details[32];
    sprintf(path_details, "%s.info", path);

    apendText(path_details);	

    fs::File file = LittleFS.open(path, FILE_READ);
    fs::File file_info = LittleFS.open(path_details, FILE_READ);


    if (file) {
        Serial.println("File opened: " + String(path));
        update_screen();
        
        if (file.available()) {
            file.read((uint8_t*)ps_image, file.size());
        }

        file.close();

        apendText("Loaded image from filesystem");

    } else {
        Serial.println("Failed to open file for reading: " + String(path));
        apendText("Failed to read image file");
    }

    uint8_t info[4];

    if (file_info) {
        
        file_info.read((uint8_t*)info, 4);
        file_info.close();
        
        image_height = info[0];
        //image_width = info[1] + info[2] + info[3];
        image_width = ((uint16_t)info[2] << 8) | info[1];
        image_loaded = true;

        set_offset();
        
        apendText("Loaded image info");

    } else {
        Serial.println("Failed to open file for reading: " + String(path_details));
        apendText("Failed to read image file info");
    }
}

void writeBinaryData(const char *path, const uint8_t *data, size_t size) {
    Serial.print("Creating file with binary data: ");
    Serial.println(String(path));

    char textdata[255];
    sprintf(textdata, "Creating file with binary data: %s", path);
    apendText(textdata);

    update_screen();

    char path_details[32];
    sprintf(path_details, "%s.info", path);

    fs::File file = LittleFS.open(path, FILE_WRITE);
    fs::File file_info = LittleFS.open(path_details, FILE_WRITE);
  
    if (file) {
        file.write(data, size);
        file.close();
        Serial.println("Binary file created and written successfully");
        apendText("File written successfully");
    
    } else {
        Serial.println("Failed to create binary file");
        apendText("Failed to create binary file");
    }

    if (file_info) {

        uint8_t widths[2];
        widths[0] = image_width & 0xFF;
        widths[1] = (image_width >> 8) & 0xFF;

        uint8_t info[3] = {image_height, widths[0], widths[1]};

        file_info.write(info, 3);

        file_info.close();
        Serial.println("File info created and written successfully");
        apendText("File info written successfully");
    } else {
        Serial.println("Failed to create binary file info");
        apendText("Failed to create binary file info");
    }
}

const char* pcheck(uint8_t r, uint8_t c) {
  if (r & (1 << c)) {
    return "True";
  } else {
    return "False";
  }
}

unsigned char * to_24bit_char_arr(int input) {

  unsigned char *buffer = (unsigned char *) malloc(3);

  if (buffer == NULL) {
    return NULL;
  }

  buffer[2] = input >> 16;
  buffer[1] = input >> 8;
  buffer[0] = input;

  return buffer;
}

void spif_write_enable() {
  apendText("Writing enable...");
  update_screen();

  spif->beginTransaction(spi_settings);
  digitalWrite(CS_PIN, LOW);
  spif->transfer(0x06);
  digitalWrite(CS_PIN, HIGH);
  spif->endTransaction();
}

void spif_write_disable() {
  spif->beginTransaction(spi_settings);
  digitalWrite(CS_PIN, LOW);
  spif->transfer(0x04);
  digitalWrite(CS_PIN, HIGH);
  spif->endTransaction();
}

uint8_t read_status_register() {

    apendText("Reading status register...");
    update_screen();
    spif->beginTransaction(spi_settings);
    digitalWrite(CS_PIN, LOW);

    spif->transfer(0x05);

    uint8_t r = spif->transfer(0x00);

    digitalWrite(CS_PIN, HIGH);
    spif->endTransaction();

    apendText("Read Complete");
    update_screen();

    const char status_keys[8][5] = { "WPEN", "x   ", "x   ", "x   ", "BP1 ", "BP0 ", "WEL ", "x   " };

    char textdata[255];
    for (int i = 7; i >= 0; i--) {
        sprintf(textdata, "%s: %s\r\n", status_keys[i], pcheck(r, i));
        apendText(textdata);
    }

    return r;
}

char * spif_read(int address, int length) {
    unsigned char *buffer = to_24bit_char_arr(address);
    if (buffer == NULL) {
        apendText("Failed to allocate memory");
        update_screen();
        return NULL;
    }

    char textdata[255];
    sprintf(textdata, "Reading at address: 0x%X", address);
    apendText(textdata);
    update_screen();

    char *data = (char *) malloc(length);

    apendText("Reading...");
    update_screen();

    spif->beginTransaction(spi_settings);
    digitalWrite(CS_PIN, LOW);
    spif->transfer(0x03);

    spif->transfer(buffer, 3);

    spif->transfer(data, length);

    digitalWrite(CS_PIN, HIGH);
    spif->endTransaction();

    apendText("Read Complete");
    update_screen();

    free(buffer);

    return data;

}

void spif_write(int address, char *data, int length) {
    unsigned char *buffer = to_24bit_char_arr(address);
    if (buffer == NULL) {
        apendText("Failed to allocate memory");
        update_screen();
        return;
    }
    
    char textdata[255];

    sprintf(textdata, "Writing at address: 0x%X", address);
    apendText(textdata);
    update_screen();

    spif_write_enable();
    spif->beginTransaction(spi_settings);
    digitalWrite(CS_PIN, LOW);
    spif->transfer(0x02);
    spif->transfer(buffer, 3);
    spif->transfer(data, length);
    digitalWrite(CS_PIN, HIGH);
    spif->endTransaction();

    apendText("Write Complete");
    update_screen();
}

void load_image_over_spi() {

    apendText("Loading image...");
    update_screen();
    
    char *data = spif_read(0, WIDTH*HEIGHT*2);

    if (data == NULL) {
        apendText("Failed to load image");
        update_screen();
        return;
    }

    apendText("Copying image to buffer...");
    update_screen();

    memcpy(ps_image, data, WIDTH*HEIGHT*2);

    apendText("Image loaded");
    update_screen();
    
    free(data);
}

void write_image_to_spi() {
    apendText("Writing image to SPI...");
    spif_write(0, (char*)ps_image, WIDTH*HEIGHT*2);
    apendText("Image written");
    update_screen();
}

#if defined(PLATFORM_LCD)
void screen_init() {
    tft.init();
    tft.setRotation(3);
    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);

    ledcWrite(0, 30);
}
#elif defined(PLATFORM_LCDA)
void screen_init() {
    rm67162_init();
    lcd_setRotation(3);
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);
}
#endif

void spif_init() {
    spif = new SPIClass(HSPI);
    spif->begin(12, 13, 11, 10);

    pinMode(CS_PIN, OUTPUT);
    digitalWrite(CS_PIN, HIGH);

    pinMode(14, OUTPUT);
    digitalWrite(14, HIGH);
}

void setup(void)
{
    Serial.begin(115200);
    Serial.println("INIT");


    screen_init();

    //init spi
    #if defined(PLATFORM_LCDA)
    spif_init();
    #endif
    
    
    sprite.createSprite(WIDTH, HEIGHT);
    sprite.setSwapBytes(1);

    text_sprite.createSprite(WIDTH, HEIGHT);


    ps_image = (uint16_t*)ps_calloc(WIDTH*HEIGHT, sizeof(uint16_t));

    sprite.fillSprite(TFT_RED); delay(250); draw_frame();
    sprite.fillSprite(TFT_GREEN); delay(250); draw_frame();
    sprite.fillSprite(TFT_BLUE); delay(250); draw_frame();

    WM.addAP(wifi_ssid, wifi_password);

    while(WM.run() != WL_CONNECTED) {
        delay(100);
        Serial.println("Connecting to WiFi..");
    }

    // server address, port and URL
    webSocket.begin(ws_local_ip, 8765, "/");

    // event handler
    webSocket.onEvent(webSocketEvent);

    // try every 5000 again if connection has failed
    webSocket.setReconnectInterval(500);

    text_sprite.setTextColor(0xFFFF, TFT_BLACK);

    button1.attachClick([]() {
        if (menu_index == 0) {
            menu_index = 1;
        } else {
            make_decision();
        }
    });

    button2.attachClick([]() {
        if (menu_index == 0) {
            time_since_last_activity = 0;
        } else {
            menu_option_index += 1;
            if (menu_option_index >= menu_option_count) {
                menu_option_index = 0;
            }
        }
    });

    // Initialise FS
    if (!FileSys.begin(true)) {
        Serial.println("LittleFS initialisation failed!");
        apendText("Filesystem mount failed!");
    } else {
        Serial.println("LittleFS initialisation done");
        apendText("Filesystem mounted successfully");
    }

    fs::File root = LittleFS.open("/", "r");

    if (!root) {
        Serial.println("root open failed");
        apendText("root open failed");
    } else {
        Serial.println("root opened");
        apendText("root opened");
    }
    
}

void loop()
{
    time_since_last_activity += 1;
    
    if (!ws_failed) {
        webSocket.loop();
    }

    button1.tick();
    button2.tick();

    update_screen();

    delay(1);

}
