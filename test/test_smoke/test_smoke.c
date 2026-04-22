/*
 Minimal Unity smoke test for PIO + stm32cube on NUCLEO-H753ZI.
 Production src/main.c is excluded during `pio test` (see platformio.ini test_build_src).
*/

#include "stm32h7xx_hal.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_always_passes(void)
{
    TEST_ASSERT_TRUE(1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_always_passes);
    UNITY_END();

    while (1)
    {
    }
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}
