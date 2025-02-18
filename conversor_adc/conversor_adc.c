#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "hardware/pwm.h"

#define VRX_PIN 26
#define VRY_PIN 27
#define SW_PIN 22

#define bnt_A 5

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define LED_RED 11
#define LED_GREEN 12
#define LED_AZUL 13

// Variáveis que vão ser utilizadas na interrupção
static volatile uint32_t last_time = 0;
volatile int pwm_ativo = 1;
volatile int led_green_estado = 0;
volatile int animation_oled_op = 1;

// Algumas variáveis globais para o controle do quadrado no display
int rect_x_pos;
int rect_y_pos;

// Função de interrupção
static void gpio_irq_handler(uint gpio, uint32_t events);

// Configurações do PWM do LED
const uint16_t PERIOD = 4096;

// Funções para iniciar o PWM do led
void setup_pwm_leds();

int main()
{
    stdio_init_all();

    // Iniciando o ADC e o botão do joystick
    adc_init();
    adc_gpio_init(VRX_PIN);
    adc_gpio_init(VRY_PIN);
    gpio_init(SW_PIN);
    gpio_set_dir(SW_PIN, GPIO_IN);
    gpio_pull_up(SW_PIN);

    // Iniciando o botão A
    gpio_init(bnt_A);
    gpio_set_dir(bnt_A, GPIO_IN);
    gpio_pull_up(bnt_A);

    // Interrupções
    gpio_set_irq_enabled_with_callback(bnt_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(SW_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // Display 128x64
    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA); // Pull up the data line
    gpio_pull_up(I2C_SCL); // Pull up the clock line
    ssd1306_t ssd; // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    bool cor = true;

    // PWM 
    setup_pwm_leds();

    while (true) {
        adc_select_input(0); // Para o pino 26 do VRX
        uint16_t vrx_value = adc_read();

        adc_select_input(1); // Para o pino 26 do VRX
        uint16_t vry_value = adc_read();

        printf("VRX: %u, VRY: %u\n", vrx_value, vry_value);

        if (pwm_ativo) {
            if (vrx_value >= 2047) {
                uint16_t newValue =  (vrx_value - 2047)*2;
                rect_x_pos = (int)((newValue/4096.0)*100);
                printf("Aplicado no led vermelho a seguinte intensidade(0-4096): %u\n", newValue);
                pwm_set_gpio_level(LED_RED, newValue);
            } else {
                uint16_t newValue =  -1 * ((vrx_value - 2047)*2);
                rect_x_pos = -1 * (int)((newValue/4096.0)*100);
                printf("Aplicado no led vermelho a seguinte intensidade(0-4096): %u\n", newValue);
                pwm_set_gpio_level(LED_RED, newValue);
            }

            if (vry_value >= 2047) {
                uint16_t newValue =  (vry_value - 2047)*2;
                rect_y_pos = (int)((newValue/4096.0)*100);
                printf("Aplicado no led azul a seguinte intensidade(0-4096): %u\n\n", newValue);
                pwm_set_gpio_level(LED_AZUL, newValue);
            } else {
                uint16_t newValue =  -1 * ((vry_value - 2047)*2);
                rect_y_pos = -1 * (int)((newValue/4096.0)*100);
                printf("Aplicado no led azul a seguinte intensidade(0-4096): %u\n\n", newValue);
                pwm_set_gpio_level(LED_AZUL, newValue);
            }
        } else {
            printf("Leds desativados\n\n");

            pwm_set_gpio_level(LED_GREEN, 0);
            pwm_set_gpio_level(LED_RED, 0);
            pwm_set_gpio_level(LED_AZUL, 0);
        }
        
        uint8_t ajuste_pos_x;
        uint8_t ajuste_pos_y;

        if (rect_x_pos > 0) { // Porcentagem Positiva, pra direita
            ajuste_pos_x = (int)((rect_x_pos*64)/100) + 54; 
        } else {
            ajuste_pos_x = 60 + (int)((rect_x_pos*64)/100) < 0 ? 3: 60 + (int)((rect_x_pos*64)/100);
        }

        if (rect_y_pos > 0) { // Porcentagem Positiva, para baixo
            ajuste_pos_y = (int)((rect_y_pos*32)/100) + 20; 
        } else {
            ajuste_pos_y = 20 + (int)((rect_y_pos*32)/100) < 0 ? 4: 20 + (int)((rect_y_pos*32)/100);
        }

        // Atualiza o conteúdo do display com animações
        ssd1306_fill(&ssd, !cor); // Limpa o display
        ssd1306_rect(&ssd, ajuste_pos_y, ajuste_pos_x, 8, 8, cor, cor);
        if (animation_oled_op == 1) {
            ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Desenha um retângulo
        } else if (animation_oled_op == 2) {
            ssd1306_hline(&ssd, 3, 122, 3, cor);
        } else if (animation_oled_op == 3) {
            ssd1306_vline(&ssd, 3, 3, 58, cor);
        } else if (animation_oled_op == 4) {
            ssd1306_vline(&ssd, 122, 3, 58, cor);
        } else if (animation_oled_op == 5) {
            ssd1306_hline(&ssd, 3, 122, 58, cor);
        }
        
        ssd1306_send_data(&ssd); // Atualiza o display
        sleep_ms(400);
    }
}

static void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    if (current_time - last_time >= 200000) {
        if (gpio == 5) {
            pwm_ativo = !pwm_ativo;
            printf("Btn pressionado A\n");
        } else {
            printf("Btn pressionado SW\n");
            
            if (pwm_ativo) {
                if (led_green_estado) {
                    pwm_set_gpio_level(LED_GREEN, 0);
                    led_green_estado = !led_green_estado;
                } else {
                    pwm_set_gpio_level(LED_GREEN, 4096);
                    led_green_estado = !led_green_estado;
                }
            }

            if (animation_oled_op == 5) {
                animation_oled_op = 1;
            } else {
                animation_oled_op++;
            }
        }
        
        last_time = current_time;
    }
}

void setup_pwm_leds() {
    uint slice1;
    gpio_set_function(LED_RED, GPIO_FUNC_PWM);
    slice1 = pwm_gpio_to_slice_num(LED_RED);
    pwm_set_wrap(slice1, PERIOD);
    pwm_set_gpio_level(LED_RED, 0);
    pwm_set_enabled(slice1, true);

    uint slice2;
    gpio_set_function(LED_GREEN, GPIO_FUNC_PWM);
    slice2 = pwm_gpio_to_slice_num(LED_GREEN);
    pwm_set_wrap(slice2, PERIOD);
    pwm_set_gpio_level(LED_GREEN, 0);
    pwm_set_enabled(slice2, true);

    uint slice3;
    gpio_set_function(LED_AZUL, GPIO_FUNC_PWM);
    slice3 = pwm_gpio_to_slice_num(LED_AZUL);
    pwm_set_wrap(slice3, PERIOD);
    pwm_set_gpio_level(LED_AZUL, 0);
    pwm_set_enabled(slice3, true);
}