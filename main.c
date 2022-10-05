/*******************************************************************************
* File Name: main.c
*
* Description: This code example demonstrates using the CSD current
*              digital-to-analog converter (IDAC) as a current source and a
*              current sink. CSD IDAC supports two channels - A and B.
*
*              1. IDAC as current sink: Channel A is configured for sinking
*                 current and used for driving an LED. Firmware controls
*                 the sinking current to toggle the LED every second.
*
*              2. IDAC as current source: Channel B is configured as a current
*                 source. The current increases when a switch is pressed. Once
*                 the output reaches its maximum value, it resets to zero and
*                 starts to increase the value again. If the switch is not
*                 pressed, it holds the last value.
*
* Related Document: README.md
*
********************************************************************************
* Copyright 2019-2022, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/*******************************************************************************
 * Include header files
 ******************************************************************************/
#include "cyhal.h"
#include "cybsp.h"
#include "cy_pdl.h"
#include "cycfg.h"
#include "cy_csdidac.h"
#include "cy_retarget_io.h"


/*******************************************************************************
* Macros
*******************************************************************************/
#define CURRENT_MIN_VALUE       (0)            /* in nanoamperes */
#define CURRENT_MAX_VALUE       (600000)       /* in nanoamperes */
#define CURRENT_INCREMENT_VALUE (10000)        /* in nanoamperes */
#define LED_ON_CURRENT          (-600000)      /* in nanoamperes */
#define SWITCH_POLLING_DELAY_MS (200u)         /* in milliseconds */
#define LED_TOGGLE_DELAY_MS     (1000u)        /* in milliseconds */

/* \x1b[2J\x1b[;H - ANSI ESC sequence for clearing screen */
#define CLEAR_SCREEN ("\x1b[2J\x1b[;H")


/*******************************************************************************
* Function Prototypes
********************************************************************************/
static void check_status(const char *message, uint32_t status);


/*******************************************************************************
* Global Variables
********************************************************************************/
cy_stc_csdidac_context_t csdidac_context;


/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*  System entrance point. This function performs
*  - initial setup of device
*  - initializes CSDIDAC
*  - checks the switch status and updates the source current value if switch is
*    pressed
*  - toggles the LED every second (LED_TOGGLE_DELAY_MS) by controlling the sink
*    current of CSDIDAC block
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    uint32_t toggle_delay = 0;
    int32_t current_value = CURRENT_MIN_VALUE;
    bool led_control = true;
    cy_en_csdidac_status_t status;
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    __enable_irq();
    
    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
                        CY_RETARGET_IO_BAUDRATE);

    /* Initialize the user button */
    cyhal_gpio_init((cyhal_gpio_t) CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT,
                    CYHAL_GPIO_DRIVE_PULLUP, CYBSP_BTN_OFF);

    printf(CLEAR_SCREEN);
    printf("****************** "
           "PSoC 6 MCU: CSDIDAC "
           "****************** \r\n\n");

    /* Initialize CSDIDAC */
    status = Cy_CSDIDAC_Init(&CSDIDAC_csdidac_config, &csdidac_context);
    check_status("CSDIDAC initialization failed", status);

    for(;;)
    {
        if(CYBSP_BTN_PRESSED == cyhal_gpio_read(CYBSP_USER_BTN))
        {
            status = Cy_CSDIDAC_OutputEnable(CY_CSDIDAC_B, current_value,
                        &csdidac_context);

            check_status("Unable to set source current", status);

            printf("CSDIDAC is configured for sourcing current: %ld nA\r\n",
                    (long int)current_value);

            current_value += CURRENT_INCREMENT_VALUE;

            if (current_value > CURRENT_MAX_VALUE)
            {
                current_value = CURRENT_MIN_VALUE;
            }
        }

        if(LED_TOGGLE_DELAY_MS == toggle_delay)
        {
            if(led_control)
            {
                /* Turns LED on */
                status = Cy_CSDIDAC_OutputEnable(CY_CSDIDAC_A,
                                                 LED_ON_CURRENT, 
                                                 &csdidac_context);

                check_status("Unable to set sink current", status);
            }
            else
            {
                /* Turns LED off  */
                status = Cy_CSDIDAC_OutputDisable(CY_CSDIDAC_A,
                                                  &csdidac_context);

                check_status("Disable channel A command failed", status);
            }

            led_control = !led_control;
            toggle_delay = 0;
        }

        Cy_SysLib_Delay(SWITCH_POLLING_DELAY_MS);
        toggle_delay += SWITCH_POLLING_DELAY_MS;
    }
}


/*******************************************************************************
* Function Name: check_status
********************************************************************************
* Summary:
*  Asserts the non-zero status and prints the message.
*
* Parameters:
*  char *message : message to print if status is non-zero.
*  uint32_t status : status for evaluation.
*
*******************************************************************************/
static void check_status(const char *message, uint32_t status)
{
    if(0u != status)
    {
        printf("[Error] : %s Error Code: 0x%08lX\r\n", message,
               (unsigned long)status);
        while(1);
    }
}


/* [] END OF FILE */
