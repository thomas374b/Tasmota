/*
  xdsp_21_vfdm_serial.ino - Samsung Serial VFDM Display support for Tasmota

  Copyright (C) 2023  Thomas Pantzer

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifdef USE_DISPLAY
#ifdef USE_DISPLAY_VFDM

#define XDSP_33                33

#ifndef VFDM_BAUDRATE
	#define 	VFDM_BAUDRATE		9600
#else
	#if (VFDM_BAUDRATE == 19200)
		#warning "must assign txPin to hardwareSerial"
	#else
		#if (VFDM_BAUDRATE == 2400) || (VFDM_BAUDRATE == 4800) || (VFDM_BAUDRATE == 9600)
		#else
			#error "only 2400,4800,9600 and 19200 baud are supported for VFDM serial Display"
		#endif
	#endif
#endif


#define 	BYTE_TIME  			((1000000UL * (1+8+1)) / VFDM_BAUDRATE)			// in µs
#define		INTER_COMMAND_CHARS	10		// maybe we need more
#define		INTER_CMD_DELAY		((INTER_COMMAND_CHARS * 1000000UL * (1+8+1)) / VFDM_BAUDRATE)

/*********************************************************************************************/
//instantiate Serial Object
TasmotaSerial *VfdmSerial = nullptr;

void vfdmSend(uint8_t b)
{
	if (VfdmSerial == nullptr) {
		return;
	}
//	VfdmSerial->flush();
	VfdmSerial->write(b);
}

void vfdmSendBytes(uint8_t *bytes, uint8_t len)
{
	for (uint8_t i=0; i<len; i++) {
		vfdmSend(bytes[i]);
	}
	delayMicroseconds(INTER_CMD_DELAY);
}

void VfdmClear()
{
	vfdmSend(0x0c);
	delayMicroseconds(INTER_CMD_DELAY);
}

void VfdmSetCursor(uint8_t col, uint8_t row)
{
	uint8_t cursor[4];
	cursor[0] = 0x1F;
	cursor[1] = 0x24;
	cursor[2] = col+1;
	cursor[3] = row+1;
	vfdmSendBytes(cursor, 4);
}

void VfdmPrint(char *line)
{
	vfdmSendBytes((uint8_t*)line, strlen(line));
}


void VfdmInitMode(void)
{
	uint8_t init[2];
	init[0] = 0x1B;
	init[1] = 0x40;
	vfdmSendBytes(init, 2);
	// TODO: 0x1B 0x74 <codepage>   {0..2}	 437, 850, 866
	// TODO: 0x1B 0x52 <language>   {0..10} USA,France,Germany,UK,Denmark1,Sweden,Italy,Spain,Japan,Norway,Denmark2
    // TODO: 0x1B 0x58 <brightness> {0..4}  0,40%,60%,80%,100%
	VfdmClear();
}

void VfdmInit(uint8_t mode)
{
  switch(mode) {
    case DISPLAY_INIT_MODE:
      VfdmInitMode();
#ifdef USE_DISPLAY_MODES1TO5
      DisplayClearScreenBuffer();
#endif  // USE_DISPLAY_MODES1TO5
      break;
    case DISPLAY_INIT_PARTIAL:
    case DISPLAY_INIT_FULL:
      break;
  }
}

// TODO: add preinit function
// if defined VFDM_TX => set Settings->display_model := XDSP_33



void VfdmInitDriver(void)
{
	if (!PinUsed(GPIO_VFDM_TX)) {
		AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("VFDM: TxPin not defined") );
		return;
	}
	AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("VFDM: have pin"));

	if (VfdmSerial != nullptr) {
		AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("VFDM: driver already initialized") );
		return;
	}

//	AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("VFDM: new serial"));

	// search
	// TODO: find some way to detect the device (or its absence)
	int txPin = Pin(GPIO_VFDM_TX);
	TasmotaSerial *tms = new TasmotaSerial(-1, txPin, 1);

	if (tms == nullptr) {
		AddLog(LOG_LEVEL_ERROR, PSTR("VFDM: can't allocate serial"));
		return;
	}

//	AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("VFDM: have serial 0x%08lx"), (long)tms);

	if (!tms->begin(VFDM_BAUDRATE)) {   // TODO: make baudRate configurable
		delete tms;
		Settings->display_model = 0;
		AddLog(LOG_LEVEL_ERROR, PSTR("VFDM: can't begin with baud %d"), VFDM_BAUDRATE);
		return;
	}

	bool hws = tms->hardwareSerial();
	if (hws) {
		ClaimSerial();
	}

	VfdmSerial = tms;

	if (!Settings->display_model) {
		Settings->display_model = XDSP_33;
	}

	if (XDSP_33 == Settings->display_model) {
		// Display is 20x2, Font is 5x7
	    Settings->display_width = 20;
	    Settings->display_height = 2;
	    Settings->display_cols[0] = 20;
	    Settings->display_cols[1] = 20;
	    Settings->display_rows = 2;

#ifdef USE_DISPLAY_MODES1TO5
	    DisplayAllocScreenBuffer();
#endif  // USE_DISPLAY_MODES1TO5

	    VfdmInitMode();
		AddLog(LOG_LEVEL_DEBUG, PSTR("VFDM: pin: %d, baud %d, %cw"), txPin, VFDM_BAUDRATE, hws?'h':'s');
	    AddLog(LOG_LEVEL_INFO, PSTR("DSP: VFDM initialized"));
	}
}

void VfdmDrawStringAt(void)
{
  if (dsp_flag) {  // Supply Line and Column starting with Line 1 and Column 1
    dsp_x--;
    dsp_y--;
  }
  VfdmSetCursor(dsp_x, dsp_y);
  VfdmPrint(dsp_str);
}

void VfdmDisplayOnOff()
{
	uint8_t bright[4];
	bright[0] = 0x1F;
	bright[1] = 0x58;
	if (disp_power) {
		bright[2] = 4;
	} else {
		bright[2] = 0;
	}
	vfdmSendBytes(bright, 3);
}

/*********************************************************************************************/

#ifdef USE_DISPLAY_MODES1TO5

void VfdmCenter(uint8_t row, char* txt)
{
  char line[Settings->display_cols[0] +2];

  int len = strlen(txt);
  int offset = 0;
  if (len >= Settings->display_cols[0]) {
    len = Settings->display_cols[0];
  } else {
    offset = (Settings->display_cols[0] - len) / 2;
  }
  memset(line, 0x20, Settings->display_cols[0]);
  line[Settings->display_cols[0]] = 0;
  for (uint32_t i = 0; i < len; i++) {
    line[offset +i] = txt[i];
  }

  VfdmSetCursor(0, row);
  VfdmPrint(line);
}

bool VfdmPrintLog(void)
{
  bool result = false;

  disp_refresh--;
  if (!disp_refresh) {
    disp_refresh = Settings->display_refresh;
    if (!disp_screen_buffer_cols) { DisplayAllocScreenBuffer(); }

    char* txt = DisplayLogBuffer('\337');
    if (txt != nullptr) {
      uint8_t last_row = Settings->display_rows -1;

      for (uint32_t i = 0; i < last_row; i++) {
        strlcpy(disp_screen_buffer[i], disp_screen_buffer[i +1], disp_screen_buffer_cols);
        VfdmSetCursor(0, i);            // Col 0, Row i
        VfdmPrint(disp_screen_buffer[i +1]);
      }
      strlcpy(disp_screen_buffer[last_row], txt, disp_screen_buffer_cols);
      DisplayFillScreen(last_row);

      AddLog(LOG_LEVEL_DEBUG, PSTR(D_LOG_DEBUG "[%s]"), disp_screen_buffer[last_row]);

      VfdmSetCursor(0, last_row);
      VfdmPrint(disp_screen_buffer[last_row]);

      result = true;
    }
  }
  return result;
}

void VfdmTime(void)
{
  char line[Settings->display_cols[0] +1];

  snprintf_P(line, sizeof(line), PSTR("%02d" D_HOUR_MINUTE_SEPARATOR "%02d" D_MINUTE_SECOND_SEPARATOR "%02d"), RtcTime.hour, RtcTime.minute, RtcTime.second);
  VfdmCenter(0, line);
  snprintf_P(line, sizeof(line), PSTR("%02d" D_MONTH_DAY_SEPARATOR "%02d" D_YEAR_MONTH_SEPARATOR "%04d"), RtcTime.day_of_month, RtcTime.month, RtcTime.year);
  VfdmCenter(1, line);
}

void VfdmRefresh(void)  // Every second
{
  if (Settings->display_mode) {  // Mode 0 is User text
    switch (Settings->display_mode) {
      case 1:  // Time
        VfdmTime();
        break;
      case 2:  // Local
      case 4:  // Mqtt
        VfdmPrintLog();
        break;
      case 3:  // Local
      case 5: {  // Mqtt
        if (!VfdmPrintLog()) { VfdmTime(); }
        break;
      }
    }
  }
}

#endif  // USE_DISPLAY_MODES1TO5

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdsp33(uint32_t function)
{
  bool result = false;

  if (FUNC_DISPLAY_INIT_DRIVER == function) {
    VfdmInitDriver();
  }
  else if (XDSP_33 == Settings->display_model) {
    switch (function) {
      case FUNC_DISPLAY_MODEL:
        result = true;
        break;
      case FUNC_DISPLAY_INIT:
        VfdmInit(dsp_init);
        break;
      case FUNC_DISPLAY_POWER:
        VfdmDisplayOnOff();
        break;
      case FUNC_DISPLAY_CLEAR:
        VfdmClear();
        break;
//        case FUNC_DISPLAY_DRAW_HLINE:
//          break;
//        case FUNC_DISPLAY_DRAW_VLINE:
//          break;
//        case FUNC_DISPLAY_DRAW_CIRCLE:
//          break;
//        case FUNC_DISPLAY_FILL_CIRCLE:
//          break;
//        case FUNC_DISPLAY_DRAW_RECTANGLE:
//          break;
//        case FUNC_DISPLAY_FILL_RECTANGLE:
//          break;
//        case FUNC_DISPLAY_DRAW_FRAME:
//          break;
//        case FUNC_DISPLAY_TEXT_SIZE:
//          break;
//        case FUNC_DISPLAY_FONT_SIZE:
//          break;
      case FUNC_DISPLAY_DRAW_STRING:
        VfdmDrawStringAt();
        break;
//        case FUNC_DISPLAY_ROTATION:
//          break;
#ifdef USE_DISPLAY_MODES1TO5
      case FUNC_DISPLAY_EVERY_SECOND:
        VfdmRefresh();
        break;
#endif  // USE_DISPLAY_MODES1TO5
    }
  }
  return result;
}

#endif  // USE_DISPLAY_VFDM
#endif  // USE_DISPLAY

