#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <esp_http_server.h>
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "string.h"
#include <hx711.h>
#include <stdio.h>
#include "ds18b20.h"
#include "driver/gpio.h"
#include <math.h>

#define WIFI_SSID		"brisa-1564207"
#define WIFI_PASSWORD	"ws6uqosn"
#define MAXIMUM_RETRY   5
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define TEMP_BUS 26
#define LED 2
#define HIGH 1
#define LOW 0
#define digitalWrite gpio_set_level
#define BOMBA_STATE GPIO_NUM_32
#define ZEROVAL 858602765
#define CALIBVAL 8586059026.8
#define PESOBASE 250

DeviceAddress tempSensors[2];

static EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0;
static const char *TAG = "hx711-example";
const char tempChar[10];
const char volChar[10];
int aux = 1;
int auxVol = 1;

static uint8_t estadoBomba = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() retorna os bits antes da chamada ser retornada, portanto, podemos testar 
    qual evento realmente aconteceu. */
    if (bits & WIFI_CONNECTED_BIT) {
        //ESP_LOGI(TAG, "Conectado ao AP SSID:%s password:%s",
                 //WIFI_SSID, WIFI_PASSWORD);
    } else if (bits & WIFI_FAIL_BIT) {
        //ESP_LOGI(TAG, "Falha em conectar ao AP SSID:%s, password:%s",
                 //WIFI_SSID, WIFI_PASSWORD);
    } else {
        //ESP_LOGE(TAG, "EVENTO INESPERADO");
    }
}

const char menu_resp[] = "<h3>Sistema de Automacao Hidroponia</h3><button><a href=\"/volume\">Volume</a></button><br><button><a href=\"/temperatura\">Temperatura</a></button><br><button><a href=\"/ligar\">Ligar Bomba</button></a><br><button><a href=\"/telegram\">Telegram</button></a>";
const char telegram_resp[] = "<object width='0' height='0' type='text/html' data='https://api.telegram.org/bot5631568641:AAFePicn19oVp3fiNxkqtY1mnZ90bdApaDE/sendMessage?chat_id=-662165667&text=sistema_funcionando'></object>Mensagem Enviada para o Telegram!<br><br><a href=\"/\"><button>VOLTAR</button</a>";
const char bomba_resp[] = "<h3>Bomba ligada!</h3><button><a href=\"/desligar\">Desligar Bomba</button></a><br><a href=\"/\"><button>VOLTAR</button</a>";

void hx711_uso() {

    hx711_t dev = {
        .dout = 4,
        .pd_sck = 2,
        .gain = HX711_GAIN_A_64
    };

    ESP_ERROR_CHECK(hx711_init(&dev));

    esp_err_t r = hx711_wait(&dev, 500);
        if (r != ESP_OK) {
            ESP_LOGE(TAG, "Device not found: %d (%s)\n", r, esp_err_to_name(r));
        }

        int32_t data;
        r = hx711_read_average(&dev, 5, &data);
        if (r != ESP_OK) {
            ESP_LOGE(TAG, "Could not read data: %d (%s)\n", r, esp_err_to_name(r));
        }

        ESP_LOGI(TAG, "Raw data: %" PRIi32, data);
        printf("%d", data);

        float unit = (float) PESOBASE / (float) (ZEROVAL - CALIBVAL);
        float grama = (float) (ZEROVAL - data) * unit;

        //printf("\n\nGRAMA %f\n\n", grama);

        float volume = grama*1000000;
        
        printf("\n\nPESO: %.2f gr\n\n", volume);

        snprintf(volChar, 50, "%.2f", volume);
        printf("%s", volChar);

        vTaskDelay(pdMS_TO_TICKS(500));
}

void ds18b20_uso() {

    ds18b20_requestTemperatures();
    float temp1 = ds18b20_getTempF((DeviceAddress *)tempSensors[0]);
    float temp2 = ds18b20_getTempF((DeviceAddress *)tempSensors[1]);
    float temp3 = ds18b20_getTempC((DeviceAddress *)tempSensors[0]);
    float temp4 = ds18b20_getTempC((DeviceAddress *)tempSensors[1]);

    float cTemp = 0;
    
    cTemp = ds18b20_get_temp();
    printf("Temperatura: %0.1fC\n", cTemp);

    if (cTemp > 15 && cTemp < 50) {
        memset(tempChar, 0, 5);
        snprintf(tempChar, 50, "%.1f", cTemp);
        printf("%s", tempChar);
    } 

    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

esp_err_t get_handler(httpd_req_t *req) {	
	httpd_resp_send(req, menu_resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t volume_handler(httpd_req_t *req) {
    hx711_uso();

    const char vol_resp[26] = "<h3>Resultado do Volume: ";
    const char resto_vol_resp[100] = "</h3><a href=\"/\"><button>Voltar</button</a><a href=\"/volume\"><button>Atualizar</button></a>";
    const char void_resp[150] = "";

    strcat(void_resp, vol_resp);
    strcat(void_resp, volChar);
    strcat(void_resp, resto_vol_resp);
    printf("%s", void_resp);

    httpd_resp_send(req, void_resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t temperatura_handler(httpd_req_t *req) {
    ds18b20_uso();

    const char on_resp[50] = "<h3>Resultado da Temperatura: ";
    const char resto_on_resp[200] = "</h3><a href=\"/\"><button>Voltar</button</a><a href=\"/temperatura\"><button>Atualizar</button></a>";
    const char void_temp_resp[300] = "";

    strcat(void_temp_resp, on_resp);
    strcat(void_temp_resp, tempChar);
    strcat(void_temp_resp, resto_on_resp);

    httpd_resp_send(req, void_temp_resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t off_bomb_handler(httpd_req_t *req) {
    gpio_set_level(BOMBA_STATE, 0);
	estadoBomba = 0;
    httpd_resp_send(req, bomba_resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t on_bomb_handler(httpd_req_t *req) {
    gpio_set_level(BOMBA_STATE, 1);
	estadoBomba = 1;
    httpd_resp_send(req, bomba_resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t telegram_handler(httpd_req_t *req) {

    const char telegram_resp[200] = "<object width='0' height='0' type='text/html' data='https://api.telegram.org/bot5631568641:AAFePicn19oVp3fiNxkqtY1mnZ90bdApaDE/sendMessage?chat_id=-662165667&text=";
    const char telegram_resto_resp[150] = "'></object>Mensagem Enviada para o Telegram!<br><br><a href=\"/\"><button>VOLTAR</button</a>";
    const char void_telegram_resp[450] = "";

    strcat(void_telegram_resp, telegram_resp);
    strcat(void_telegram_resp, "STATUS%0ATemperatura: ");
    strcat(void_telegram_resp, tempChar);
    strcat(void_telegram_resp, "%0AVolume: ");
    strcat(void_telegram_resp, volChar);

    if (estadoBomba == 1) {
        strcat(void_telegram_resp, "%0ABomba ligada!");
    } else {
        strcat(void_telegram_resp, "%0ABomba desligada!");
    }

    strcat(void_telegram_resp, telegram_resto_resp);
    
    httpd_resp_send(req, void_telegram_resp, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

httpd_uri_t uri_get = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_volume = {
    .uri      = "/volume",
    .method   = HTTP_GET,
    .handler  = volume_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_temperatura = {
    .uri      = "/temperatura",
    .method   = HTTP_GET,
    .handler  = temperatura_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_on_bomb = {
    .uri      = "/ligar",
    .method   = HTTP_GET,
    .handler  = on_bomb_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_off_bomb = {
    .uri      = "/desligar",
    .method   = HTTP_GET,
    .handler  = off_bomb_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_telegram = {
    .uri      = "/telegram",
    .method   = HTTP_GET,
    .handler  = telegram_handler,
    .user_ctx = NULL
};

httpd_handle_t setup_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_get);
		httpd_register_uri_handler(server, &uri_volume);
		httpd_register_uri_handler(server, &uri_temperatura);
		httpd_register_uri_handler(server, &uri_on_bomb);
		httpd_register_uri_handler(server, &uri_off_bomb);
        httpd_register_uri_handler(server, &uri_telegram);
    }

    return server;
}

void getTempAddresses(DeviceAddress *tempSensorAddresses) {
	unsigned int numberFound = 0;
	reset_search();

	while (search(tempSensorAddresses[numberFound],true)) {
		numberFound++;
		if (numberFound == 2) break;
	}

	while (numberFound != 2) {
        
		numberFound = 1;
		digitalWrite(LED, HIGH);
		vTaskDelay(100 / portTICK_PERIOD_MS);
		digitalWrite(LED, LOW);
		vTaskDelay(100 / portTICK_PERIOD_MS);
		reset_search();

		while (search(tempSensorAddresses[numberFound],true)) {
			numberFound++;
			if (numberFound == 2) break;
		}
	}
	return;
}

void app_main(void) {

    system("start https://blog.eletrogate.com/nodemcu-esp12-alarme-residencial-iot-3/");
    //system("open https://blog.eletrogate.com/nodemcu-esp12-alarme-residencial-iot-3/");
    //ShellExecute(NULL, "open", "https://blog.eletrogate.com/nodemcu-esp12-alarme-residencial-iot-3/", NULL, NULL, SW_SHOWNORMAL);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();        
	setup_server();

    gpio_set_direction(BOMBA_STATE, GPIO_MODE_OUTPUT);
	gpio_set_level(BOMBA_STATE, 0);
	estadoBomba = 0;

	ds18b20_init(TEMP_BUS);
	getTempAddresses(tempSensors);
	ds18b20_setResolution(tempSensors,2,10);

    printf("Address 0: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n", tempSensors[0][0],tempSensors[0][1],tempSensors[0][2],tempSensors[0][3],tempSensors[0][4],tempSensors[0][5],tempSensors[0][6],tempSensors[0][7]);
	printf("Address 1: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n", tempSensors[1][0],tempSensors[1][1],tempSensors[1][2],tempSensors[1][3],tempSensors[1][4],tempSensors[1][5],tempSensors[1][6],tempSensors[1][7]);

    while (1) {
        ds18b20_uso();
        hx711_uso();
    } 

}
