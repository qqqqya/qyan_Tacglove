/**
 * @file bsp_led_driver.c
 * @brief STM32F042 通过 PA4 发送 SK6805-EC15 单总线数据。
 * @details
 * SK6805 每颗灯接收 24 bit，顺序为 G7..G0、R7..R0、B7..B0。
 * 当前时序按48 MHz Cortex-M0标定，修改编译优化等级后必须重新测量波形。
 */
#include "bsp_led_driver.h"

#include "main.h"

#define SK6805_REQUIRED_CORE_CLOCK_HZ 48000000UL /**< 当前软件时序的标定主频。 */
#define SK6805_RESET_LOOP_COUNT       3600UL     /**< 产生大于 200 us 的复位低电平。 */

#define NOP_4()  do { __NOP(); __NOP(); __NOP(); __NOP(); } while (0)
#define NOP_8()  do { NOP_4(); NOP_4(); } while (0)
#define NOP_16() do { NOP_8(); NOP_8(); } while (0)

/** @brief 逻辑0高电平延时，目标约0.3 us。 */
#define SK6805_DELAY_HIGH_ZERO() do { NOP_8(); } while (0)

/** @brief 逻辑1高电平延时，目标约0.9 us。 */
#define SK6805_DELAY_HIGH_ONE()  do { NOP_16(); NOP_16(); NOP_8(); } while (0)

/** @brief 逻辑0低电平补偿，使码元周期不小于1.2 us。 */
#define SK6805_DELAY_LOW_ZERO()  do { NOP_16(); NOP_16(); NOP_4(); } while (0)

/** @brief 逻辑1低电平补偿，使码元周期不小于1.2 us。 */
#define SK6805_DELAY_LOW_ONE()   do { NOP_4(); } while (0)

/**
 * @brief 7 颗灯的发送缓存。
 * @details 第一维是串行数据顺序，第二维固定为 G、R、B。
 */
static uint8_t s_pixels[BSP_LED_PIXEL_COUNT][3];

/**
 * @brief 按最高位优先顺序发送一个字节。
 * @param value 要发送的 8 bit 数据。
 * @note 单个码元内部使用NOP宏，不依赖函数级编译器attribute。
 */
static void sk6805_write_byte(uint8_t value)
{
    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
        RGB_Ctrl_GPIO_Port->BSRR = RGB_Ctrl_Pin;

        if ((value & 0x80U) != 0U)
        {
            SK6805_DELAY_HIGH_ONE();
            RGB_Ctrl_GPIO_Port->BSRR = (uint32_t)RGB_Ctrl_Pin << 16U;
            SK6805_DELAY_LOW_ONE();
        }
        else
        {
            SK6805_DELAY_HIGH_ZERO();
            RGB_Ctrl_GPIO_Port->BSRR = (uint32_t)RGB_Ctrl_Pin << 16U;
            SK6805_DELAY_LOW_ZERO();
        }

        value <<= 1U;
    }
}

/**
 * @brief 在一帧数据结束后保持低电平，使所有 SK6805 锁存新颜色。
 * @note 当前 SK6805-EC15-001 A/1 要求复位低电平至少 200 us；
 *       该循环按48 MHz标定为大于200 us。
 */
static void sk6805_reset_latch(void)
{
    /* Release构建下循环总时间大于器件要求的200 us。 */
    for (uint32_t count = 0U; count < SK6805_RESET_LOOP_COUNT; ++count)
    {
        __NOP();
    }
}

void bsp_led_driver_init(void)
{
    HAL_GPIO_WritePin(RGB_Ctrl_GPIO_Port, RGB_Ctrl_Pin, GPIO_PIN_RESET);
    bsp_led_driver_clear();
}

led_driver_status_t bsp_led_driver_set_pixel(uint8_t pixel_index, bsp_led_color_t color)
{
    if (pixel_index >= BSP_LED_PIXEL_COUNT)
    {
        return LED_ERRORPARAMETER;
    }

    /* SK6805 协议不是 RGB 顺序，必须转换为 G-R-B 后再存入发送缓存。 */
    s_pixels[pixel_index][0] = color.green;
    s_pixels[pixel_index][1] = color.red;
    s_pixels[pixel_index][2] = color.blue;
    return LED_OK;
}

void bsp_led_driver_clear(void)
{
    for (uint8_t pixel = 0U; pixel < BSP_LED_PIXEL_COUNT; ++pixel)
    {
        s_pixels[pixel][0] = 0U;
        s_pixels[pixel][1] = 0U;
        s_pixels[pixel][2] = 0U;
    }
}

led_driver_status_t bsp_led_driver_show(void)
{
    if (SystemCoreClock != SK6805_REQUIRED_CORE_CLOCK_HZ)
    {
        return LED_ERRORRESOURCE;
    }

    /*
     * 保存进入函数前的中断状态。完整一帧只有 7*24 bit，短暂关中断可避免
     * FreeRTOS tick 或其他 ISR 拉长高电平，发送完成后恢复原始状态。
     */
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    for (uint8_t pixel = 0U; pixel < BSP_LED_PIXEL_COUNT; ++pixel)
    {
        sk6805_write_byte(s_pixels[pixel][0]);
        sk6805_write_byte(s_pixels[pixel][1]);
        sk6805_write_byte(s_pixels[pixel][2]);
    }

    RGB_Ctrl_GPIO_Port->BSRR = (uint32_t)RGB_Ctrl_Pin << 16U;
    sk6805_reset_latch();

    if (primask == 0U)
    {
        __enable_irq();
    }

    return LED_OK;
}
