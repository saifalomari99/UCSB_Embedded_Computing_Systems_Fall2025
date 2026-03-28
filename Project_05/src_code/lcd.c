/*
 * lcd.c
 *
 *  Created on: Oct 21, 2015
 *      Author: atlantis
 */

/*
 UTFT.cpp - Multi-Platform library support for Color TFT LCD Boards
 Copyright (C)2015 Rinky-Dink Electronics, Henning Karlsen. All right reserved

 This library is the continuation of my ITDB02_Graph, ITDB02_Graph16
 and RGB_GLCD libraries for Arduino and chipKit. As the number of
 supported display modules and controllers started to increase I felt
 it was time to make a single, universal library as it will be much
 easier to maintain in the future.

 Basic functionality of this library was origianlly based on the
 demo-code provided by ITead studio (for the ITDB02 modules) and
 NKC Electronics (for the RGB GLCD module/shield).

 This library supports a number of 8bit, 16bit and serial graphic
 displays, and will work with both Arduino, chipKit boards and select
 TI LaunchPads. For a full list of tested display modules and controllers,
 see the document UTFT_Supported_display_modules_&_controllers.pdf.

 When using 8bit and 16bit display modules there are some
 requirements you must adhere to. These requirements can be found
 in the document UTFT_Requirements.pdf.
 There are no special requirements when using serial displays.

 You can find the latest version of the library at
 http://www.RinkyDinkElectronics.com/

 This library is free software; you can redistribute it and/or
 modify it under the terms of the CC BY-NC-SA 3.0 license.
 Please see the included documents for further information.

 Commercial use of this library requires you to buy a license that
 will allow commercial use. This includes using the library,
 modified or not, as a tool to sell products.

 The license applies to all part of the library including the
 examples and tools supplied with the library.
 */

#include <stdio.h>
#include "lcd.h"
#include <string.h>

static const float SAMPLE_RATE_HZ = 48000.0f;

// Global variables
int fch;
int fcl;
int bch;
int bcl;
struct _current_font cfont;

// Write command to LCD controller
void LCD_Write_COM(char VL) {
	Xil_Out32(SPI_DC, 0x0);
	Xil_Out32(SPI_DTR, VL);

	while (0 == (Xil_In32(SPI_IISR) & XSP_INTR_TX_EMPTY_MASK))
		;
	Xil_Out32(SPI_IISR, Xil_In32(SPI_IISR) | XSP_INTR_TX_EMPTY_MASK);
}

// Write 8-bit data to LCD controller
void LCD_Write_DATA(char VL) {
	Xil_Out32(SPI_DC, 0x01);
	Xil_Out32(SPI_DTR, VL);

	while (0 == (Xil_In32(SPI_IISR) & XSP_INTR_TX_EMPTY_MASK))
		;
	Xil_Out32(SPI_IISR, Xil_In32(SPI_IISR) | XSP_INTR_TX_EMPTY_MASK);
}

// Initialize LCD controller
void initLCD(void) {
	int i;

	// Reset
	LCD_Write_COM(0x01);
	for (i = 0; i < 500000; i++)
		; //Must wait > 5ms

	LCD_Write_COM(0xCB);
	LCD_Write_DATA(0x39);
	LCD_Write_DATA(0x2C);
	LCD_Write_DATA(0x00);
	LCD_Write_DATA(0x34);
	LCD_Write_DATA(0x02);

	LCD_Write_COM(0xCF);
	LCD_Write_DATA(0x00);
	LCD_Write_DATA(0XC1);
	LCD_Write_DATA(0X30);

	LCD_Write_COM(0xE8);
	LCD_Write_DATA(0x85);
	LCD_Write_DATA(0x00);
	LCD_Write_DATA(0x78);

	LCD_Write_COM(0xEA);
	LCD_Write_DATA(0x00);
	LCD_Write_DATA(0x00);

	LCD_Write_COM(0xED);
	LCD_Write_DATA(0x64);
	LCD_Write_DATA(0x03);
	LCD_Write_DATA(0X12);
	LCD_Write_DATA(0X81);

	LCD_Write_COM(0xF7);
	LCD_Write_DATA(0x20);

	LCD_Write_COM(0xC0);   //Power control
	LCD_Write_DATA(0x23);  //VRH[5:0]

	LCD_Write_COM(0xC1);   //Power control
	LCD_Write_DATA(0x10);  //SAP[2:0];BT[3:0]

	LCD_Write_COM(0xC5);   //VCM control
	LCD_Write_DATA(0x3e);  //Contrast
	LCD_Write_DATA(0x28);

	LCD_Write_COM(0xC7);   //VCM control2
	LCD_Write_DATA(0x86);  //--

	LCD_Write_COM(0x36);   // Memory Access Control
	LCD_Write_DATA(0x48);

	LCD_Write_COM(0x3A);
	LCD_Write_DATA(0x55);

	LCD_Write_COM(0xB1);
	LCD_Write_DATA(0x00);
	LCD_Write_DATA(0x18);

	LCD_Write_COM(0xB6);   // Display Function Control
	LCD_Write_DATA(0x08);
	LCD_Write_DATA(0x82);
	LCD_Write_DATA(0x27);

	LCD_Write_COM(0x11);   //Exit Sleep
	for (i = 0; i < 100000; i++)
		;

	LCD_Write_COM(0x29);   //Display on
	LCD_Write_COM(0x2c);

	//for (i = 0; i < 100000; i++);

	// Default color and fonts
	fch = 0xFF;
	fcl = 0xFF;
	bch = 0x00;
	bcl = 0x00;
	setFont(SmallFont);
}

// Set boundary for drawing
void setXY(int x1, int y1, int x2, int y2) {
	LCD_Write_COM(0x2A);
	LCD_Write_DATA(x1 >> 8);
	LCD_Write_DATA(x1);
	LCD_Write_DATA(x2 >> 8);
	LCD_Write_DATA(x2);
	LCD_Write_COM(0x2B);
	LCD_Write_DATA(y1 >> 8);
	LCD_Write_DATA(y1);
	LCD_Write_DATA(y2 >> 8);
	LCD_Write_DATA(y2);
	LCD_Write_COM(0x2C);
}

// Remove boundry
void clrXY(void) {
	setXY(0, 0, DISP_X_SIZE, DISP_Y_SIZE);
}

// Set foreground RGB color for next drawing
void setColor(u8 r, u8 g, u8 b) {
	// 5-bit r, 6-bit g, 5-bit b
	fch = (r & 0x0F8) | g >> 5;
	fcl = (g & 0x1C) << 3 | b >> 3;
}

// Set background RGB color for next drawing
void setColorBg(u8 r, u8 g, u8 b) {
	// 5-bit r, 6-bit g, 5-bit b
	bch = (r & 0x0F8) | g >> 5;
	bcl = (g & 0x1C) << 3 | b >> 3;
}

// Clear display
void clrScr(void) {
	// Black screen
	setColor(0, 0, 0);

	fillRect(0, 0, DISP_X_SIZE, DISP_Y_SIZE);
}

// Draw horizontal line
void drawHLine(int x, int y, int l) {
	int i;

	if (l < 0) {
		l = -l;
		x -= l;
	}

	setXY(x, y, x + l, y);
	for (i = 0; i < l + 1; i++) {
		LCD_Write_DATA(fch);
		LCD_Write_DATA(fcl);
	}

	clrXY();
}

// Fill a rectangular
void fillRect(int x1, int y1, int x2, int y2) {
	int i;

	if (x1 > x2)
		swap(int, x1, x2);

	if (y1 > y2)
		swap(int, y1, y2);

	setXY(x1, y1, x2, y2);
	for (i = 0; i < (x2 - x1 + 1) * (y2 - y1 + 1); i++) {
		LCD_Write_DATA(fch);
		LCD_Write_DATA(fcl);
	}

	clrXY();
}


// Select the font used by print() and printChar()
void setFont(u8* font) {
	cfont.font = font;
	cfont.x_size = font[0];
	cfont.y_size = font[1];
	cfont.offset = font[2];
	cfont.numchars = font[3];
}

// Print a character
void printChar(u8 c, int x, int y) {
	u8 ch;
	int i, j, pixelIndex;

	setXY(x, y, x + cfont.x_size - 1, y + cfont.y_size - 1);

	pixelIndex = (c - cfont.offset) * (cfont.x_size >> 3) * cfont.y_size + 4;
	for (j = 0; j < (cfont.x_size >> 3) * cfont.y_size; j++) {
		ch = cfont.font[pixelIndex];
		for (i = 0; i < 8; i++) {
			if ((ch & (1 << (7 - i))) != 0) {
				LCD_Write_DATA(fch);
				LCD_Write_DATA(fcl);
			} else {
				LCD_Write_DATA(bch);
				LCD_Write_DATA(bcl);
			}
		}
		pixelIndex++;
	}

	clrXY();
}

// Print string
void lcdPrint(char *st, int x, int y) {
	int i = 0;
	while (*st != '\0')
		printChar(*st++, x + cfont.x_size * i++, y);
}

// Print Triangle -------------------------------------------------------------------------------
void trianglePrint(int x1, int x2, int y1, int y2) { //y is lower position of triangle

	if (x1 > x2)				// Make sure dimensions are correct
		swap(int, x1, x2);

	if (y1 > y2)				// Only drawing upright triangle
		swap(int, y1, y2);


	int x2_set = (x2 > DISP_X_SIZE) ? DISP_X_SIZE : x2;
	int y2_set = (y2 > DISP_Y_SIZE) ? DISP_Y_SIZE : y2;

	setXY(x1, y1, x2_set, y2_set);

	int W = x2 - x1;  // width of the triangle base
    int H = y2 - y1;  // height of the triangle

    for (int y= H; y >= 0; y--) {
        for (int x = 0; x <= W; x++) {
        	int tri = (W * (H - y)) >> 1;	// edge of triangle
        	int cx = W >> 1;				// center of triangle
        	int pt = (x - cx)*H;			// center x on triangle (scaling by H to avoid divide)

        	if (pt < 0) pt = -pt;			// abs() to flip across center

        	if((x + x1 > x2_set) || (y + y1 > y2_set)) break;

        	if(pt <= tri) { 				// point is inside of triangle boundary
        		LCD_Write_DATA(fch);
        		LCD_Write_DATA(fcl);
        	} else {						// point is outside of triangle boundary
        		LCD_Write_DATA(bch);
        		LCD_Write_DATA(bcl);
        	}
        }
    }

	clrXY();
}

void setColorScheme() {
	//setColor(255, 0, 0);
	//setColor(0, 255, 0);
	setColor(0, 0, 255);

	//setColorBg(235, 52, 225);
	//setColorBg(235, 171, 52);
	setColorBg(52, 201, 235);
}

#define TILE_W 40
#define TILE_H 40

// Set background with triangles
void setBackground(){
	setColorScheme();
	for (int y = 0; y <= DISP_Y_SIZE; y+=TILE_H) {
		for (int x = 0; x <= DISP_X_SIZE; x+=TILE_W) {
			trianglePrint(x, x+TILE_W, y, y+TILE_H);
		}
	}
}
// -------------------------------------------------------------------------------------------

// Drawing volume bar ----------------------------------------------------
#define VBAR_X1 20
#define VBAR_X2 219
#define VBAR_Y1 200
#define VBAR_Y2 220
#define VBAR_W (VBAR_X2 - VBAR_X1)
#define VBAR_H (VBAR_Y2 - VBAR_Y1)
#define VBAR_B (VBAR_W / 63)

struct _volume_bar
{
    int vol;
    int drawn;
};

struct _volume_bar _vbar;

static int vol_to_x[64];			// Volume table

void initVolumeTable(void) {
    for (int v = 0; v <= 63; v++) {
        int rhs = (VBAR_W * v) / 63;   // done once
        // if (rhs > VBAR_W - 1) rhs = VBAR_W - 1;
        vol_to_x[v] = rhs;
    }
}

void drawVolumeBar(int vol) {
	setColor(255, 0, 0);			// Modify to change colors
	setColorBg(255, 255, 255);

	if(vol < 0) vol = 0;
	if(vol > 63) vol = 63;

	int rhs = vol_to_x[vol]; 		// RHS of volume bar

	setXY(VBAR_X1, VBAR_Y1, VBAR_X2, VBAR_Y2);

	for (int y = 0; y <= (VBAR_H); y++) {
		for (int x = 0; x <= (VBAR_W); x++) {
			if(x <= rhs && vol != 0) {					// point is inside volume bar
				LCD_Write_DATA(fch);
				LCD_Write_DATA(fcl);
			} else {						// point is outside of volume bar
				LCD_Write_DATA(bch);
				LCD_Write_DATA(bcl);
			}
		}
	}

	_vbar.drawn = 1;
	_vbar.vol = vol;

	clrXY();
}

// Updating volume bar to draw less pixels
void updateVolumeBar(int vol){
	if(_vbar.drawn == 0){			// Only update if bar is drawn
		drawVolumeBar(vol);
		return;
	}

	if(vol == _vbar.vol) return; 	// No need to redraw

	setColor(255, 0, 0);			// Modify to change colors
	setColorBg(255, 255, 255);

	int x_old = VBAR_X1 + vol_to_x[_vbar.vol];
	int x_new = VBAR_X1 + vol_to_x[vol];

	// xil_printf("This is my old volume: %d\nThis is my new volume:%d\n", _vbar.vol, vol);
	// xil_printf("x_old: %d\nx_new:%d\n\n", x_old, x_new);

	if (vol > _vbar.vol) {						// increasing volume
		// int x1 = VBAR_X1 + VBAR_B * _vbar.vol;
		// int x2 = VBAR_X1 + VBAR_B * vol;

		setXY(x_old, VBAR_Y1, x_new, VBAR_Y2);

		for(int i = 0; i <= (x_new-x_old+1)*(VBAR_H+1); i++){
			LCD_Write_DATA(fch);
			LCD_Write_DATA(fcl);
		}
	}
	else {										// decreasing volume
		// int x1 = VBAR_X1 + VBAR_B * vol;
		// int x2 = VBAR_X1 + VBAR_B * _vbar.vol;

		setXY(x_new, VBAR_Y1, x_old, VBAR_Y2);

		for(int i = 0; i <= (x_old-x_new+1)*(VBAR_H+1); i++){
			LCD_Write_DATA(bch);
			LCD_Write_DATA(bcl);
		}
	}

	_vbar.vol = vol;
}

void clearVolumeBar() {
	if (_vbar.drawn == 0) return; // No point if not drawn

	setColorScheme();
	for (int y = 0; y <= DISP_Y_SIZE; y+=TILE_H) {
		for (int x = 0; x <= DISP_X_SIZE; x+=TILE_W) {
			if((x <= VBAR_X2) && (x+TILE_W >= VBAR_X1) && (y <= VBAR_Y2) && (y+TILE_W >= VBAR_Y1)) {
				trianglePrint(x, x+TILE_W, y, y+TILE_H);
			}
		}
	}

	_vbar.drawn = 0;
}
// --------------------------------------------------------------------------

// Button Message Handling --------------------------------------------------
#define MSG_BOX_X1 40
#define MSG_BOX_Y1 40

struct _message_box {
	char *st;
	int num;
	int width;
	int height;
	int drawn;
};

struct _message_box _msg_box;

// Message Box initializer
void messageBoxInit() {
	_msg_box.num = -1;
	_msg_box.width = 0;
	_msg_box.height = 0;
	_msg_box.drawn = 0;
}

// Print message
//void printButtonMessage(int btn) {
//	setColor(0, 0, 0);
//	setColorBg(255, 255, 255);
//
//	switch(btn) {
//	case 0:
//	case 1:
//	case 2:
//	case 3:
//	default:
//		if(_msg_box.num != 4 || _msg_box.drawn == 0) {
//			_msg_box.num = 4;
//			_msg_box.st = "Button 4: Message";
//		} else {
//			return;
//		}
//		break;
//	}
//
//	if(_msg_box.st != NULL) {
//		int i = 0;
//		char *message = _msg_box.st;
//		while (*message != '\0')
//			printChar(*message++, MSG_BOX_X1 + cfont.x_size * i++, MSG_BOX_Y1);
//		_msg_box.width = i * cfont.x_size;
//		_msg_box.height = cfont.y_size;
//		_msg_box.drawn = 1;
//	}
//}
void printButtonMessage(int btn) {
    // text colors: black on white
    setColor(0, 0, 0);
    setColorBg(255, 255, 255);

    // choose message by button id
    switch (btn) {
        case 0: _msg_box.st = "Up: Saif is here";     break;
        case 1: _msg_box.st = "LEFT: Dominic is here";   break;
        case 2: _msg_box.st = "RIGHT: ECE253";  break;
        case 3: _msg_box.st = "DOWN: FALL2025";   break;
        case 4: _msg_box.st = "CENTER: Lab2b"; break;
        default:_msg_box.st = "Button: clicked default"; break;
    }

    // draw the string at MSG_BOX_X1, MSG_BOX_Y1
    int i = 0;
    char *message = _msg_box.st;
    while (*message != '\0') {
        printChar(*message++, MSG_BOX_X1 + cfont.x_size * i++, MSG_BOX_Y1);
    }
    _msg_box.width  = i * cfont.x_size;
    _msg_box.height = cfont.y_size;
    _msg_box.drawn  = 1;
    _msg_box.num    = btn;
}




// Clearing button messages
void clearButtonMessage() {
	if(_msg_box.drawn == 0) return;

	setColorScheme();

	for (int y = 0; y <= DISP_Y_SIZE; y+=TILE_H) {
		for (int x = 0; x <= DISP_X_SIZE; x+=TILE_W) {
			if((x <= MSG_BOX_X1 + _msg_box.width) && (x+TILE_W >= MSG_BOX_X1) &&
					(y <= MSG_BOX_Y1 + _msg_box.height) && (y+TILE_W >= MSG_BOX_Y1)) {
				trianglePrint(x, x+TILE_W, y, y+TILE_H);
			}
		}
	}

	_msg_box.drawn = 0;
	_msg_box.num = -1;
}
// --------------------------------------------------------------------------

// LCD Init
void startScreen() {
	setBackground();
	initVolumeTable();
	messageBoxInit();
}

// ======================================================================
//                    New UI helpers
// ======================================================================
static float samples_to_ms(uint32_t samples) {
    return 1000.0f * ((float)samples / SAMPLE_RATE_HZ);
}





void drawWelcomeScreen(void) {
    clrScr();
    setColor(255, 255, 255);
    setColorBg(0, 0, 0);
    setFont(BigFont);
    lcdPrint("Welcome", 60, 80);

    setFont(SmallFont);
    lcdPrint("Press CENTER to continue", 30, 140);
}


#define BORDER_THICKNESS     4   // change this to make the border thicker or thinner


// X position for labels (leave room for cursor on the left)
#define LABEL_X            15
// X position for all values (leave room for cursor and labels)
#define VAL_X               60


// Operator block
#define VAL_OP_ON_Y         60
#define VAL_OP_WAVE_Y       80
#define VAL_OP_RATIO_Y      100

// Oscillator block
#define VAL_OSC_WAVE_Y      160

// Envelope block
#define VAL_ENV_ATTACK_Y    210
#define VAL_ENV_DECAY_Y     225
#define VAL_ENV_RELEASE_Y   240
#define VAL_ENV_SUSTAIN_Y   255

#define VALUE_BOX_W         150
#define VALUE_BOX_H         14

#define CURSOR_X            5
#define CURSOR_BOX_SIZE     6
#define CURSOR_Y_OFFSET     2   // tweak this to center the cursor better




static void drawScreenBorder(u8 r, u8 g, u8 b) {
    int t     = BORDER_THICKNESS;
    int x_max = DISP_X_SIZE;
    int y_max = DISP_Y_SIZE;

    // Save current colors
    int save_fch = fch;
    int save_fcl = fcl;
    int save_bch = bch;
    int save_bcl = bcl;

    setColor(r, g, b);        // border color
    setColorBg(0, 0, 0);      // black background

    // Top border
    fillRect(0, 0, x_max, t - 1);

    // Bottom border
    fillRect(0, y_max - (t - 1), x_max, y_max);

    // Left border
    fillRect(0, 0, t - 1, y_max);

    // Right border
    fillRect(x_max - (t - 1), 0, x_max, y_max);

    // Restore previous colors
    fch = save_fch;
    fcl = save_fcl;
    bch = save_bch;
    bcl = save_bcl;
}







void drawMainStaticLayout(void) {
    clrScr();

    // Draw border first so all text sits inside it (white border)
    drawScreenBorder(255, 255, 255);

    setColor(255, 255, 255);
    setColorBg(0, 0, 0);
    setFont(BigFont);
    lcdPrint("Main Screen", 40, 10);

    setFont(SmallFont);
    setColor(255, 255, 255);
    setColorBg(0, 0, 0);
    // Operator group
    lcdPrint("Operator", LABEL_X, 40);
    lcdPrint("Depth:",   LABEL_X, VAL_OP_ON_Y);
    lcdPrint("Wave:",    LABEL_X, VAL_OP_WAVE_Y);
    lcdPrint("Ratio:",   LABEL_X, VAL_OP_RATIO_Y);

    // Oscillator group
    lcdPrint("Oscillator", LABEL_X, 140);
    lcdPrint("Wave:",      LABEL_X, VAL_OSC_WAVE_Y);

    // Envelope group
    lcdPrint("Envelope", LABEL_X, 190);
    lcdPrint("Atk:",     LABEL_X, VAL_ENV_ATTACK_Y);
    lcdPrint("Dcy:",     LABEL_X, VAL_ENV_DECAY_Y);
    lcdPrint("Rel:",     LABEL_X, VAL_ENV_RELEASE_Y);
    lcdPrint("Sus:",     LABEL_X, VAL_ENV_SUSTAIN_Y);
}



void drawEditStaticLayout(void) {
    clrScr();

    // Yellow border for edit window
    drawScreenBorder(255, 255, 0);

    // Shorter title, also yellow
    setColor(255, 255, 0);
    setColorBg(0, 0, 0);
    setFont(BigFont);
    lcdPrint("Edit Params", 30, 10);

    setFont(SmallFont);
    setColor(255, 255, 255);
    setColorBg(0, 0, 0);

    // Operator group
    lcdPrint("Operator", LABEL_X, 40);
    lcdPrint("Depth:",   LABEL_X, VAL_OP_ON_Y);
    lcdPrint("Wave:",    LABEL_X, VAL_OP_WAVE_Y);
    lcdPrint("Ratio:",   LABEL_X, VAL_OP_RATIO_Y);

    // Oscillator group
    lcdPrint("Oscillator", LABEL_X, 140);
    lcdPrint("Wave:",      LABEL_X, VAL_OSC_WAVE_Y);

    // Envelope group
    lcdPrint("Envelope", LABEL_X, 190);
    lcdPrint("Atk:",     LABEL_X, VAL_ENV_ATTACK_Y);
    lcdPrint("Dcy:",     LABEL_X, VAL_ENV_DECAY_Y);
    lcdPrint("Rel:",     LABEL_X, VAL_ENV_RELEASE_Y);
    lcdPrint("Sus:",     LABEL_X, VAL_ENV_SUSTAIN_Y);
}

static void drawWaveOptions(int x, int y, wave_type_t selected) {
    const char *names[WAVE_COUNT] = {
        "Sin", "Tri", "Saw", "Sqr"
    };

    int cursor_x = x;

    /* Save current colors so we can restore them */
    int save_fch = fch;
    int save_fcl = fcl;
    int save_bch = bch;
    int save_bcl = bcl;

    for (int i = 0; i < WAVE_COUNT; i++) {
        if ((wave_type_t)i == selected) {
            /* Use whatever color the caller chose for this parameter name */
            /* Do not call setColor here, just keep the current fch/fcl */
            setColorBg(0, 0, 0);
        } else {
            /* Dim or normal for non selected items */
            setColor(150, 150, 150);    /* gray foreground */
            setColorBg(0, 0, 0);        /* black background */
        }

        lcdPrint((char *)names[i], cursor_x, y);

        /* Move cursor to the right for the next label */
        cursor_x += (int)strlen(names[i]) * cfont.x_size + 6;

        /* Restore caller color after each label so the selected one can use it */
        fch = save_fch;
        fcl = save_fcl;
        bch = save_bch;
        bcl = save_bcl;
    }

    /* Final restore just in case */
    fch = save_fch;
    fcl = save_fcl;
    bch = save_bch;
    bcl = save_bcl;
}

static void clearValueBox(int x, int y) {
    /* Save current colors */
    int save_fch = fch;
    int save_fcl = fcl;
    int save_bch = bch;
    int save_bcl = bcl;

    /* Use black to clear the box */
    setColor(0, 0, 0);
    setColorBg(0, 0, 0);
    fillRect(x, y, x + VALUE_BOX_W, y + VALUE_BOX_H);

    /* Restore previous colors */
    fch = save_fch;
    fcl = save_fcl;
    bch = save_bch;
    bcl = save_bcl;
}


void drawAllParameterValues(const Lab2A *me) {
    char line[40];

    setFont(SmallFont);
    setColor(255, 255, 255);
    setColorBg(0, 0, 0);

    clearValueBox(VAL_X, VAL_OP_ON_Y);
    snprintf(line, sizeof(line), "%.1f", (double)me->operator_depth);
    lcdPrint(line, VAL_X, VAL_OP_ON_Y);

    clearValueBox(VAL_X, VAL_OP_WAVE_Y);
    setColor(255, 0, 0);
    drawWaveOptions(VAL_X, VAL_OP_WAVE_Y, me->operator_wave);

    setColor(255, 255, 255);
    clearValueBox(VAL_X, VAL_OP_RATIO_Y);
    snprintf(line, sizeof(line), "%lu", (unsigned long)me->operator_ratio);
    lcdPrint(line, VAL_X, VAL_OP_RATIO_Y);

    clearValueBox(VAL_X, VAL_OSC_WAVE_Y);
    setColor(0, 255, 0);
    drawWaveOptions(VAL_X, VAL_OSC_WAVE_Y, me->osc_wave);
    setColor(255, 255, 255);
    float attack_ms  = samples_to_ms(me->env_attack_samples);
    float decay_ms   = samples_to_ms(me->env_decay_samples);
    float release_ms = samples_to_ms(me->env_release_samples);

    clearValueBox(VAL_X, VAL_ENV_ATTACK_Y);
    snprintf(line, sizeof(line), "%.1f ms", attack_ms);
    lcdPrint(line, VAL_X, VAL_ENV_ATTACK_Y);

    clearValueBox(VAL_X, VAL_ENV_DECAY_Y);
    snprintf(line, sizeof(line), "%.1f ms", decay_ms);
    lcdPrint(line, VAL_X, VAL_ENV_DECAY_Y);

    clearValueBox(VAL_X, VAL_ENV_RELEASE_Y);
    snprintf(line, sizeof(line), "%.1f ms", release_ms);
    lcdPrint(line, VAL_X, VAL_ENV_RELEASE_Y);

    clearValueBox(VAL_X, VAL_ENV_SUSTAIN_Y);
    snprintf(line, sizeof(line), "%.2f", (double)me->env_sustain);
    lcdPrint(line, VAL_X, VAL_ENV_SUSTAIN_Y);
}

void drawMainParameterScreen(const Lab2A *me) {
    drawMainStaticLayout();
    drawAllParameterValues(me);
}

// --- Selection cursor helpers ------------------------------------------------
static void drawCursorBox(int x, int y) {
    setColor(255, 255, 0);
    setColorBg(0, 0, 0);
    fillRect(x, y, x + CURSOR_BOX_SIZE, y + CURSOR_BOX_SIZE);
}

static int cursor_y_from_param(param_index_t p) {
    int base_y;
    switch (p) {
    case PARAM_OP_DEPTH:    base_y = VAL_OP_ON_Y;       break;
    case PARAM_OP_WAVE:     base_y = VAL_OP_WAVE_Y;     break;
    case PARAM_OP_RATIO:    base_y = VAL_OP_RATIO_Y;    break;
    case PARAM_OSC_WAVE:    base_y = VAL_OSC_WAVE_Y;    break;
    case PARAM_ENV_ATTACK:  base_y = VAL_ENV_ATTACK_Y;  break;
    case PARAM_ENV_DECAY:   base_y = VAL_ENV_DECAY_Y;   break;
    case PARAM_ENV_RELEASE: base_y = VAL_ENV_RELEASE_Y; break;
    case PARAM_ENV_SUSTAIN: base_y = VAL_ENV_SUSTAIN_Y; break;
    default:                return -1;
    }

    return base_y + CURSOR_Y_OFFSET;
}


void drawSelectionCursor(const Lab2A *me) {
    int y = cursor_y_from_param(me->selected_param);
    if (y < 0) return;
    drawCursorBox(CURSOR_X, y);
}

void eraseSelectionCursor(const Lab2A *me) {
    int y = cursor_y_from_param(me->selected_param);
    if (y < 0) return;

    setColor(0, 0, 0);  // background color
    setColorBg(0, 0, 0);
    fillRect(CURSOR_X, y, CURSOR_X + CURSOR_BOX_SIZE, y + CURSOR_BOX_SIZE);
}

void lcdUpdateParameterValue(const Lab2A *me, param_index_t which) {
    char line[40];

    setFont(SmallFont);
    setColor(255, 255, 255);
    setColorBg(0, 0, 0);

    switch (which) {
    case PARAM_OP_DEPTH:
        clearValueBox(VAL_X, VAL_OP_ON_Y);
        snprintf(line, sizeof(line), "%.1f", (double)me->operator_depth);
        lcdPrint(line, VAL_X, VAL_OP_ON_Y);
        break;

    case PARAM_OP_WAVE:
        clearValueBox(VAL_X, VAL_OP_WAVE_Y);
        setColor(255, 0, 0);   /* Operator color */
        drawWaveOptions(VAL_X, VAL_OP_WAVE_Y, me->operator_wave);
        setColor(255, 255, 255);
        break;

    case PARAM_OP_RATIO:
        clearValueBox(VAL_X, VAL_OP_RATIO_Y);
        snprintf(line, sizeof(line), "%lu", (unsigned long)me->operator_ratio);
        lcdPrint(line, VAL_X, VAL_OP_RATIO_Y);
        break;

    case PARAM_OSC_WAVE:
        clearValueBox(VAL_X, VAL_OSC_WAVE_Y);
        setColor(0, 255, 0);   /* Oscillator color */
        drawWaveOptions(VAL_X, VAL_OSC_WAVE_Y, me->osc_wave);
        setColor(255, 255, 255);
        break;

    case PARAM_ENV_ATTACK: {
        clearValueBox(VAL_X, VAL_ENV_ATTACK_Y);
        float attack_ms = samples_to_ms(me->env_attack_samples);
        snprintf(line, sizeof(line), "%.1f ms", attack_ms);
        lcdPrint(line, VAL_X, VAL_ENV_ATTACK_Y);
        break;
    }
    case PARAM_ENV_DECAY: {
        clearValueBox(VAL_X, VAL_ENV_DECAY_Y);
        float decay_ms = samples_to_ms(me->env_decay_samples);
        snprintf(line, sizeof(line), "%.1f ms", decay_ms);
        lcdPrint(line, VAL_X, VAL_ENV_DECAY_Y);
        break;
    }
    case PARAM_ENV_RELEASE: {
        clearValueBox(VAL_X, VAL_ENV_RELEASE_Y);
        float release_ms = samples_to_ms(me->env_release_samples);
        snprintf(line, sizeof(line), "%.1f ms", release_ms);
        lcdPrint(line, VAL_X, VAL_ENV_RELEASE_Y);
        break;
    }
    case PARAM_ENV_SUSTAIN:
        clearValueBox(VAL_X, VAL_ENV_SUSTAIN_Y);
        snprintf(line, sizeof(line), "%.2f", (double)me->env_sustain);
        lcdPrint(line, VAL_X, VAL_ENV_SUSTAIN_Y);
        break;

    default:
        break;
    }
}
