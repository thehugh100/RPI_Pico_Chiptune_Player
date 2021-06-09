#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "pico/time.h"
//#include "atlantis.h"
//#include "bamboo.h"
#include "soundboy.h"
#include <math.h>

uint16_t getSample12bit(const unsigned char *data, uint32_t idx, uint32_t size)
{
    return (int16_t)(data[(idx % size) * 2] | data[(idx % size) * 2 + 1] << 8) >> 4;
}

int main()
{
    //set_sys_clock_khz(131000, true);
    stdio_init_all();

    printf("Hello, world!\n");

    const uint LED_PIN = 2;

    gpio_set_function(LED_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(LED_PIN);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, 4096);
    pwm_config_set_clkdiv(&config, 1.f);
    pwm_init(slice_num, &config, true);

    uint32_t cycles = 1200;
    uint32_t sampleSize = sizeof(rawData) / 2;
    uint32_t idx = 0;
    float i = 0;
    float i2 = 0;
    uint32_t time = 0;

    while (true)
    {
        int16_t sample16 = getSample12bit(rawData, i + i2, sampleSize);

        pwm_set_gpio_level(LED_PIN, (sample16 + 2048));
        sleep_us(31);

        i += .05f;
        i = fmodf(i, sampleSize);

        i2 += 1.f;
        i2 = fmodf(i2, cycles);

        time++;
    }
}