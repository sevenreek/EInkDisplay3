#include "project.h"
#include "GUI.h"
#include "pervasive_eink_hardware_driver.h"
#include "cy_eink_library.h"
#include "LCDConf.h"

/* Frame buffers */
uint8 imageBufferCache[CY_EINK_FRAME_SIZE] = {0};
uint8 imageBuffer[CY_EINK_FRAME_SIZE] = {0};
void WakeupInterruptHandler(void);

/*******************************************************************************
* Function Name: void UpdateDisplay(void)
********************************************************************************
*
* Summary: This function updates the display with the data in the display 
*			buffer.  The function first transfers the content of the EmWin
*			display buffer to the primary EInk display buffer.  Then it calls
*			the Cy_EINK_ShowFrame function to update the display, and then
*			it copies the EmWin display buffer to the Eink display cache buffer
*
* Parameters:
*  None
*
* Return:
*  None
*
* Side Effects:
*  It takes about a second to refresh the display.  This is a blocking function
*  and only returns after the display refresh
*
*******************************************************************************/
void UpdateDisplay(cy_eink_update_t updateMethod, bool powerCycle)
{
    /* Copy the EmWin display buffer to imageBuffer*/
    LCD_CopyDisplayBuffer(imageBuffer, CY_EINK_FRAME_SIZE);

    /* Update the EInk display */
    Cy_EINK_ShowFrame(imageBufferCache, imageBuffer, updateMethod, powerCycle);

    /* Copy the EmWin display buffer to the imageBuffer cache*/
    LCD_CopyDisplayBuffer(imageBufferCache, CY_EINK_FRAME_SIZE);
}


/*******************************************************************************
* Function Name: void ClearScreen(void)
********************************************************************************
*
* Summary: This function clears the screen
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void ClearScreen(void)
{
    GUI_SetColor(GUI_BLACK);
    GUI_SetBkColor(GUI_WHITE);
    GUI_Clear();
    UpdateDisplay(CY_EINK_FULL_4STAGE, true);
}
cy_stc_sysint_t WakeupIsrPin =
{
    /* Wake-up pin is located in Port 0 */
    .intrSrc = NvicMux2_IRQn,
    .intrPriority = 1,
    .cm0pSrc = ioss_interrupts_gpio_0_IRQn
};

void WakeupInterruptHandler(void)
{
    /* Clear any pending interrupt */
    if (0u != Cy_GPIO_GetInterruptStatusMasked(SW2_PORT, SW2_NUM))
    {
        Cy_GPIO_ClearInterrupt(SW2_PORT, SW2_NUM);
    }
}

/*******************************************************************************
* Function Name: int main(void)
********************************************************************************
*
* Summary: This is the main function.  Following functions are performed
*			1. Initialize the EmWin library
*			2. Display the startup screen for 3 seconds
*			3. Display the instruction screen and wait for key press and release
*			4. Inside a while loop scroll through the 6 demo pages on every
*				key press and release
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
#include <stdio.h>
char stringBuffer[20];
// #define UPDATE_MODE CY_EINK_PARTIAL CY_EINK_FULL_4STAGE
#define UPDATE_MODE CY_EINK_FULL_4STAGE
#define ONE_PIX 0
#define DIGIT 1
#define DRAW_MODE ONE_PIX
#define XSIZE_PHYS 264
#define YSIZE_PHYS 176
void enterLowPowerEINK();
void exitLowPowerEINK();
void BLECallBack(uint32 event, void *eventParam);
void BLECallBack(uint32 event, void *eventParam)
{
    switch(event)
    {
        case CY_BLE_EVT_STACK_ON:
        case CY_BLE_EVT_GAP_DEVICE_DISCONNECTED:
        {
            Cy_BLE_GAPP_StartAdvertisement(CY_BLE_ADVERTISING_SLOW, CY_BLE_PERIPHERAL_CONFIGURATION_0_INDEX);
        }
        break;
        //case CY_BLE_EVT_GATTS_EXEC_WRITE_REQ:
        //case CY_BLE_EVT_GATTS_WRITE_REQ:
        case CY_BLE_EVT_GATTS_WRITE_CMD_REQ:
        //case CY_BLE_EVT_GATTS_PREP_WRITE_REQ:
        {
            cy_stc_ble_gattc_write_req_t* writeParams = (cy_stc_ble_gattc_write_req_t*) eventParam;
            char* string = (char*)writeParams->handleValPair.value.val;
            uint16_t strLength = writeParams->handleValPair.value.actualLen;
            string[strLength-1] = '\0';
            GUI_DispStringAt(string,0,0);
            UpdateDisplay(UPDATE_MODE,true);
        }
        break;
        
    }
}
int main(void)
{    
    
    __enable_irq(); /* Enable global interrupts.*/ 
    Cy_BLE_Start(BLECallBack);
    Cy_SysInt_Init(&WakeupIsrPin, WakeupInterruptHandler);
    Cy_GPIO_SetInterruptMask(SW2_PORT, SW2_NUM, 0x01);
    Cy_GPIO_SetInterruptEdge(SW2_PORT, SW2_NUM, CY_GPIO_INTR_RISING);
    NVIC_EnableIRQ(WakeupIsrPin.intrSrc);
    Cy_SysEnableCM4(CY_CORTEX_M4_APPL_ADDR); 
    /* Initialize emWin Graphics */
    GUI_Init();
    /* Start the eInk display interface and turn on the display power */
    Cy_EINK_Start(20);
    Cy_EINK_Power(1);
    ClearScreen();
    GUI_SetFont(GUI_FONT_13B_1);
    GUI_SetColor(GUI_BLACK);
    GUI_SetBkColor(GUI_WHITE);
    GUI_SetTextMode(GUI_TM_NORMAL);
    GUI_SetTextStyle(GUI_TS_NORMAL);
    GUI_SetTextAlign(GUI_TA_LEFT);
    
    //Cy_EINK_Power(0);
    for(;;)
    {
        Cy_BLE_ProcessEvents();
        
        enterLowPowerEINK();
        Cy_SysPm_CpuEnterDeepSleep(CY_SYSPM_WAIT_FOR_INTERRUPT);
        exitLowPowerEINK();
    }
}
void enterLowPowerEINK()
{
    EINK_Clock_Disable();
    CY_EINK_Timer_Disable();
    CY_EINK_SPIM_Disable();
    Cy_GPIO_SetDrivemode(CY_EINK_DispRst_0_PORT, CY_EINK_DispRst_0_NUM, CY_GPIO_DM_HIGHZ);
    Cy_GPIO_SetDrivemode(CY_EINK_DispEn_0_PORT, CY_EINK_DispEn_0_NUM, CY_GPIO_DM_HIGHZ);
    Cy_GPIO_SetDrivemode(CY_EINK_Discharge_0_PORT, CY_EINK_Discharge_0_NUM, CY_GPIO_DM_HIGHZ);
    Cy_GPIO_SetDrivemode(CY_EINK_Border_0_PORT, CY_EINK_Border_0_NUM, CY_GPIO_DM_HIGHZ);
    Cy_GPIO_SetDrivemode(CY_EINK_DispIoEn_0_PORT, CY_EINK_DispIoEn_0_NUM, CY_GPIO_DM_HIGHZ);
    Cy_GPIO_SetDrivemode(CY_EINK_Ssel_0_PORT, CY_EINK_Ssel_0_NUM, CY_GPIO_DM_HIGHZ);
    
    Cy_GPIO_Clr(CY_EINK_DispRst_0_PORT, CY_EINK_DispRst_0_NUM);
    Cy_GPIO_Clr(CY_EINK_DispEn_0_PORT, CY_EINK_DispEn_0_NUM);
    Cy_GPIO_Clr(CY_EINK_Discharge_0_PORT, CY_EINK_Discharge_0_NUM);
    Cy_GPIO_Clr(CY_EINK_Border_0_PORT, CY_EINK_Border_0_NUM);
    Cy_GPIO_Clr(CY_EINK_DispIoEn_0_PORT, CY_EINK_DispIoEn_0_NUM);
    Cy_GPIO_Clr(CY_EINK_Ssel_0_PORT, CY_EINK_Ssel_0_NUM);
    
}
void exitLowPowerEINK()
{
    EINK_Clock_Enable();
    CY_EINK_Timer_Enable();
    CY_EINK_SPIM_Enable();
    Cy_GPIO_SetDrivemode(CY_EINK_DispRst_0_PORT, CY_EINK_DispRst_0_NUM, CY_GPIO_DM_STRONG_IN_OFF);
    Cy_GPIO_SetDrivemode(CY_EINK_DispEn_0_PORT, CY_EINK_DispEn_0_NUM, CY_GPIO_DM_STRONG_IN_OFF);
    Cy_GPIO_SetDrivemode(CY_EINK_Discharge_0_PORT, CY_EINK_Discharge_0_NUM, CY_GPIO_DM_STRONG_IN_OFF);
    Cy_GPIO_SetDrivemode(CY_EINK_Border_0_PORT, CY_EINK_Border_0_NUM, CY_GPIO_DM_STRONG_IN_OFF);
    Cy_GPIO_SetDrivemode(CY_EINK_DispIoEn_0_PORT, CY_EINK_DispIoEn_0_NUM, CY_GPIO_DM_STRONG_IN_OFF);
    Cy_GPIO_SetDrivemode(CY_EINK_Ssel_0_PORT, CY_EINK_Ssel_0_NUM, CY_GPIO_DM_STRONG_IN_OFF);
}


/* [] END OF FILE */
