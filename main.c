/******************************************************************************/
/*                                                                            */
/* PmodKYPD.c -- Demo for the use of the Pmod Keypad IP core                  */
/*                                                                            */
/******************************************************************************/
/* Author:   Mikel Skreen                                                     */
/* Copyright 2016, Digilent Inc.                                              */
/******************************************************************************/
/* File Description:                                                          */
/*                                                                            */
/* This demo continuously captures keypad data and prints a message to an     */
/* attached serial terminal whenever a positive edge is detected on any of    */
/* the sixteen keys. In order to receive messages, a serial terminal          */
/* application on your PC should be connected to the appropriate COM port for */
/* the micro-USB cable connection to your board's USBUART port. The terminal  */
/* should be configured with 8-bit data, no parity bit, 1 stop bit, and the   */
/* the appropriate Baud rate for your application. If you are using a Zynq    */
/* board, use a baud rate of 115200, if you are using a MicroBlaze system,    */
/* use the Baud rate specified in the AXI UARTLITE IP, typically 115200 or    */
/* 9600 Baud.                                                                 */
/*                                                                            */
/******************************************************************************/
/* Revision History:                                                          */
/*                                                                            */
/*    06/08/2016(MikelS):   Created                                           */
/*    08/17/2017(artvvb):   Validated for Vivado 2015.4                       */
/*    08/30/2017(artvvb):   Validated for Vivado 2016.4                       */
/*                          Added Multiple keypress error detection           */
/*    01/27/2018(atangzwj): Validated for Vivado 2017.4                       */
/*                                                                            */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "PmodOLED.h"
#include "PmodKYPD.h"
#include "sleep.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xparameters.h"


PmodOLED myDevice;

void DemoInitialize();
void DemoRun();
void DemoCleanup();
void DisableCaches();
void EnableCaches();
void DemoSleep(u32 millis);

PmodKYPD myKYPD;



// To change between PmodOLED and OnBoardOLED is to change Orientation
const u8 orientation = 0x0; // Set up for Normal PmodOLED(false) vs normal
                            // Onboard OLED(true)
const u8 invert = 0x0; // true = whitebackground/black letters
                       // false = black background /white letters


int main(void) {
   DemoInitialize();
   DemoRun();
   DemoCleanup();
   return 0;
}

// keytable is determined as follows (indices shown in Keypad position below)
// 12 13 14 15
// 8  9  10 11
// 4  5  6  7
// 0  1  2  3
#define DEFAULT_KEYTABLE "0FED789C456B123A"

void DemoInitialize() {
   EnableCaches();
   KYPD_begin(&myKYPD, XPAR_PMODKYPD_0_AXI_LITE_GPIO_BASEADDR);
   KYPD_loadKeyTable(&myKYPD, (u8*) DEFAULT_KEYTABLE);
   OLED_Begin(&myDevice, XPAR_PMODOLED_0_AXI_LITE_GPIO_BASEADDR,
           XPAR_PMODOLED_0_AXI_LITE_SPI_BASEADDR, orientation, invert);
}




void DemoRun() {
   // KYPD
   u16 keystate;
   XStatus status, last_status = KYPD_NO_KEY;
   u8 key, last_key = 'x';
   // Initial value of last_key cannot be contained in loaded KEYTABLE string

   // OLED
   int top, right, bottom, left, loc;
   u8 *pat;


   Xil_Out32(myKYPD.GPIO_addr, 0xF);
   Xil_Out32(myDevice.GPIO_addr, 0xAAAA);

   xil_printf("Pmod KYPD demo started. Press any key on the Keypad.\r\n");


   top = 20;
   right = 70;
   bottom = 10;
   left = 60;

   int shipTop = 10;
   int shipRight = 10;
   int shipBottom = 5;
   int shipLeft = 5;

   int count = 0;


   while (1) {
		// Capture state of each key
		keystate = KYPD_getKeyStates(&myKYPD);

		// Determine which single key is pressed, if any
		status = KYPD_getKeyPressed(&myKYPD, keystate, &key);

		// Print key detect if a new key is pressed or if status has changed
		if (status == KYPD_SINGLE_KEY
			&& (status != last_status || key != last_key)) {
		 char c = (char) key;
		 int x = (int) c;
		 if (x == 65) {
			shipTop--;
			shipBottom--;
		 } else if (x == 66) {
			shipTop++;
			shipBottom++;
		 }
		 	 last_key = key;
		} else if (status == KYPD_MULTI_KEY && status != last_status)
		 xil_printf("Error: Multiple keys pressed\r\n");

		last_status = status;


		// Choosing Fill pattern 0
		pat = OLED_GetStdPattern(0);
		OLED_SetFillPattern(&myDevice, pat);
		// Turn automatic updating off
		OLED_SetCharUpdate(&myDevice, 0);


		// Draw a rectangle in center of screen
		OLED_SetDrawMode(&myDevice, OledModeSet);
		OLED_ClearBuffer(&myDevice);

		pat = OLED_GetStdPattern(1);
		OLED_SetFillPattern(&myDevice, pat);
		OLED_MoveTo(&myDevice, shipLeft, shipBottom);
		OLED_FillRect(&myDevice, shipRight, shipTop);
		OLED_DrawRect(&myDevice, shipRight, shipTop);

		OLED_SetFillPattern(&myDevice, pat);
		OLED_MoveTo(&myDevice, shipLeft - 3, shipTop - 1);
		OLED_FillRect(&myDevice, shipLeft, shipTop);
		OLED_DrawRect(&myDevice, shipLeft, shipTop);

		OLED_SetFillPattern(&myDevice, pat);
		OLED_MoveTo(&myDevice, shipLeft - 3, shipBottom + 1);
		OLED_FillRect(&myDevice, shipLeft, shipBottom);
		OLED_DrawRect(&myDevice, shipLeft, shipBottom);


		OLED_SetFillPattern(&myDevice, pat);
		OLED_MoveTo(&myDevice, shipRight, shipBottom + 1);
		OLED_FillRect(&myDevice, shipRight + 3, shipTop - 1);
		OLED_DrawRect(&myDevice, shipRight + 3, shipTop - 1);

		OLED_SetFillPattern(&myDevice, pat);
		OLED_MoveTo(&myDevice, shipRight + 3, shipBottom + 2);
		OLED_FillRect(&myDevice, shipRight + 5, shipTop - 2);
		OLED_DrawRect(&myDevice, shipRight + 5, shipTop - 2);


		pat = OLED_GetStdPattern(7);
		OLED_SetFillPattern(&myDevice, pat);
		OLED_MoveTo(&myDevice, left, bottom);
		OLED_FillRect(&myDevice, right, top);
		OLED_DrawRect(&myDevice, right, top);
		OLED_Update(&myDevice);


		if (count == 5) {
			left--;
			right--;
			if (left < 10) {
				loc = (rand() % 16) + 6;
				left = 100;
				right = 110;
				top = loc - 5;
				bottom = loc + 5;
			}
			count = 0;
		} else {
			count++;
		}


   }

}



void DemoCleanup() {
	OLED_End(&myDevice);
	DisableCaches();
}

void EnableCaches() {
#ifdef __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_ICACHE
   Xil_ICacheEnable();
#endif
#ifdef XPAR_MICROBLAZE_USE_DCACHE
   Xil_DCacheEnable();
#endif
#endif
}

void DisableCaches() {
#ifdef __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_DCACHE
   Xil_DCacheDisable();
#endif
#ifdef XPAR_MICROBLAZE_USE_ICACHE
   Xil_ICacheDisable();
#endif
#endif
}
