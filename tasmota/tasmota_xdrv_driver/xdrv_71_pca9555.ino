/*
  xdrv_69_PCA9555.ino - PCA9555 GPIO Expander support for Tasmota

  SPDX-FileCopyrightText: 2023 Theo Arends

  SPDX-License-Identifier: GPL-3.0-only
*/


#ifdef USE_I2C
	#ifdef USE_TIANMA_LCD_ON_PCA9555
// this driver is only to be used with TIANMA 16x2 I2C-Display and 4 buttons
// you have to use
//  	{"NAME":"PCA9555","GPIO":[128,129,130,131]}
// in /PCA9555.dat  to get the buttons working, i.e. the buttons are inverted
//    button sequence is
//		<X> <down> <up> <Enter>
// the GPIO port is restricted to use only INPUT on the the lower 4 bits
// other configuring options are not allowed
	#ifndef USE_PCA9555
		#define USE_PCA9555
	#endif
#endif
#if defined(USE_PCA9555)
/*********************************************************************************************\
 * 16-bit PCA9555 I2C GPIO Expander
 *
 * Docs at https://www.nxp.com/docs/en/data-sheet/PCA9555.pdf
 *
 * I2C Addresses: 0x20-0x26
 *
 * mostly derived from xdrv_69_PCA9557.ino
 *
 *  Supported template fields:
 * NAME  - Template name
 * BASE  - Optional. 0 = use relative relays (default), 1 = use absolute relays
 * GPIO  - Sequential list of pin 1 and up with configured GPIO function
 *         Function             Code      Description
 *         -------------------  --------  ----------------------------------------
 *         None                 0         Not used
 *         Button_n1..32   Bn   64..95    Button to Gnd without internal pullup
 *         Button_in1..32  Bin  128..159  Button inverted to Vcc without internal pullup
 *         Switch_n1..28   Sn   192..219  Switch to Gnd without internal pullup
 *         Relay1..32      R    224..255  Relay
 *         Relay_i1..32    Ri   256..287  Relay inverted
 *         Output_Hi       Oh   3840      Fixed output high
 *         Output_lo       Ol   3872      Fixed output low
 *
 * Prepare a template to be loaded either by:
 * - a rule like: rule3 on file#PCA9555.dat do {"NAME":"PCA9555","GPIO":[224,225,226,227,228,229,230,231]} endon
 * - a script like: -y{"NAME":"PCA9555","GPIO":[224,225,226,227,228,229,230,231]}
 * - file called PCA9555.dat with contents: {"NAME":"PCA9555","GPIO":[224,225,226,227,228,229,230,231]}
 *
 * Inverted relays           Ri1 Ri2 Ri3 Ri4 Ri5 Ri6 Ri7 Ri8
 * {"NAME":"PCA9555","GPIO":[256,257,258,259,260,261,262,263]}
 * 
 * Inverted relays and buttons               Ri8 Ri7 Ri6 Ri5 Ri4 Ri3 Ri2 Ri1 B1 B2 B3 B4 B5 B6 B7 B8
 * {"NAME":"PCA9555 A=Ri8-1, B=B1-8","GPIO":[263,262,261,260,259,258,257,256,32,33,34,35,36,37,38,39]}
 *
 * Buttons and relays                       B1 B2 B3 B4 R1  R2  R3  R4
 * {"NAME":"PCA9555 A=B1-8, B=R1-8","GPIO":[32,33,34,35,224,225,226,227]}
 * 
 * 16 relays                 R1  R2  R3  R4  R5  R6  R7  R8  R9  R10 R11 R12 R13 R14 R15 R16
 * {"NAME":"PCA9555","GPIO":[224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239]}
 *
 *
\*********************************************************************************************/

#define XDRV_71                  71
#define XI2C_86                  86     // See I2CDEVICES.md

#define PCA9555_ADDR_START       0x20   // 32
#ifdef USE_TIANMA_LCD_ON_PCA9555	// address is fixed by PCB layout on that specific device
	#define PCA9555_ADDR_END         0x21
	#define PCA9555_MAX_DEVICES      1
#else
#define PCA9555_ADDR_END         0x27  	// 0x27 is reserved for other displays
#define PCA9555_MAX_DEVICES      7
#endif

/*********************************************************************************************\
 * PCA9555 support
\*********************************************************************************************/

typedef enum {
  PCA9555_in0_7   = 0x00, // (R/ ) Register 0 - Input register bits 0-7
  PCA9555_in8_15  = 0x01, // (R/ ) Register 1 - Input register bits 8-15
  PCA9555_out0_7  = 0x02, // (R/W) Register 2 - Output register bits 0-7
  PCA9555_out8_15 = 0x03, // (R/W) Register 3 - Output register bits 8-15
  PCA9555_pol0_7  = 0x04, // (R/W) Register 4 - Polarity inversion register for bits 0-7
  PCA9555_pol8_15 = 0x05, // (R/W) Register 5 - Polarity inversion register for bits 8-15
  PCA9555_dir0_7  = 0x06, // (R/W) Register 6 - Configuration register for bits 0-7
  PCA9555_dir8_15 = 0x07, // (R/W) Register 7 - Configuration register for bits 8-15
} PCA9555Registers;

typedef struct {
  uint8_t rOut0;		// output mirrors
  uint8_t rOut1;

  uint8_t address;
  uint8_t pins;                                        // 8 (PCA9555)
} tPCA9555Device;

struct PCA9555 {
  tPCA9555Device device[PCA9555_MAX_DEVICES];
#ifndef USE_TIANMA_LCD_ON_PCA9555
  uint32_t relay_inverted;
  uint8_t relay_max;
  uint8_t relay_offset;

  uint8_t switch_max;
  int8_t switch_offset;

#endif
  uint32_t button_inverted;
  uint8_t button_max;
  int8_t button_offset;

  uint8_t chip;
  uint8_t max_devices;
  uint8_t max_pins;

  bool base;
} PCA9555;

uint16_t *PCA9555_gpio_pin = nullptr;

/*********************************************************************************************\
 * PCA9555 - I2C
\*********************************************************************************************/

void PCA9555DumpRegs(void) {
  uint8_t data[4];
  for (PCA9555.chip = 0; PCA9555.chip < PCA9555.max_devices; PCA9555.chip++) {
    uint32_t data_size = sizeof(data);
    I2cReadBuffer(PCA9555.device[PCA9555.chip].address, 0, data, data_size);
    AddLog(LOG_LEVEL_DEBUG, PSTR("PCA: Intf %d, Address %02X, Regs %*_H"), PCA9555.device[PCA9555.chip].address, data_size, data);
  }
}

uint32_t PCA9555Read(uint8_t reg) {
  uint32_t value = 0;
  value = I2cRead8(PCA9555.device[PCA9555.chip].address, reg);
  return value;
}

bool PCA9555ValidRead(uint8_t reg, uint8_t *data) {
  return I2cValidRead8(data, PCA9555.device[PCA9555.chip].address, reg);
  return false;
}

void PCA9555Write(uint8_t reg, uint8_t value) {
  I2cWrite8(PCA9555.device[PCA9555.chip].address, reg, value);
}

/*********************************************************************************************/

void PCA9555Update(uint8_t pin, bool pin_value, uint8_t reg_addr) {
	// pin = 0 - 7
	uint8_t bit = pin % 8;
	uint8_t reg_value = 0;

	switch(reg_addr) {
		case PCA9555_out0_7:	reg_value = PCA9555.device[PCA9555.chip].rOut0;	break;
		case PCA9555_out8_15:	reg_value = PCA9555.device[PCA9555.chip].rOut1;	break;
		default:				reg_value = PCA9555Read(reg_addr);		break;
	}
	if (pin_value) {
		reg_value |= 1 << bit;
	} else {
		reg_value &= ~(1 << bit);
	}
	PCA9555Write(reg_addr, reg_value);

	switch(reg_addr) {
		case PCA9555_out0_7:	PCA9555.device[PCA9555.chip].rOut0 = reg_value;		break;
		case PCA9555_out8_15:	PCA9555.device[PCA9555.chip].rOut1 = reg_value; 	break;
		default: break;
	}
}

/*********************************************************************************************/

uint32_t PCA9555SetChip(uint8_t pin) {
  // Calculate chip based on number of pins per chip. 16 for PCA9555
  // pin 0 - 15
  for (PCA9555.chip = 0; PCA9555.chip < PCA9555.max_devices; PCA9555.chip++) {
    if (PCA9555.device[PCA9555.chip].pins > pin) { break; }
    pin -= PCA9555.device[PCA9555.chip].pins;
  }
  return pin;                                          // relative pin number within chip (0 ... 7)
}

void PCA9555PinMode(uint8_t pin, uint8_t flags) {
  // pin 0 - 15
  pin = PCA9555SetChip(pin);
  uint8_t iodir = PCA9555_dir0_7;
  if (pin > 7) {
	  iodir = PCA9555_dir8_15;
  }

  switch (flags) {
    case INPUT:
      PCA9555Update(pin, true, iodir);
      break;
    case OUTPUT:
      PCA9555Update(pin, false, iodir);
      break;
  }

//  AddLog(LOG_LEVEL_DEBUG, PSTR("DBG: PCA9555PinMode chip %d, pin %d, flags %d, regs %d"), PCA9555.chip, pin, flags, iodir);
}


void PCA9555SetPinModes(uint8_t pin, uint8_t flags) {
  // pin 0 - 15
  PCA9555PinMode(pin, flags);
}

bool PCA9555DigitalRead(uint8_t pin) {
  // pin 0 - 15
  pin = PCA9555SetChip(pin);
  uint8_t reg_addr = PCA9555_in0_7;
  if (pin > 7) {
	  reg_addr = PCA9555_in8_15;
  }
  uint8_t value = PCA9555Read(reg_addr);
  uint8_t bit = pin % 8;
  return value & (1 << bit);
}

void PCA9555DigitalWrite(uint8_t pin, bool value) {
  // pin 0 - 15
  pin = PCA9555SetChip(pin);
  uint8_t reg_addr = PCA9555_out0_7;
  if (pin > 7) {
	  reg_addr = PCA9555_out8_15;
  }
//  AddLog(LOG_LEVEL_DEBUG, PSTR("DBG: PCA9555DigitalWrite chip %d, pin %d, state %d, reg %d"), PCA9555.chip, pin, value, reg_addr);
  PCA9555Update(pin, value, reg_addr);
}

/*********************************************************************************************\
 * Tasmota
\*********************************************************************************************/

int PCA9555Pin(uint32_t gpio, uint32_t index = 0);
int PCA9555Pin(uint32_t gpio, uint32_t index) {
  uint16_t real_gpio = gpio << 5;
  uint16_t mask = 0xFFE0;
  if (index < GPIO_ANY) {
    real_gpio += index;
    mask = 0xFFFF;
  }
  for (uint32_t i = 0; i < PCA9555.max_pins; i++) {
    if ((PCA9555_gpio_pin[i] & mask) == real_gpio) {
      return i;                                        // Pin number configured for gpio
    }
  }
  return -1;                                           // No pin used for gpio
}

bool PCA9555PinUsed(uint32_t gpio, uint32_t index = 0);
bool PCA9555PinUsed(uint32_t gpio, uint32_t index) {
  return (PCA9555Pin(gpio, index) >= 0);
}

uint32_t PCA9555GetPin(uint32_t lpin) {
  if (lpin < PCA9555.max_pins) {
    return PCA9555_gpio_pin[lpin];
  } else {
    return GPIO_NONE;
  }
}

/*********************************************************************************************/

String PCA9555TemplateLoadFile(void) {
  String pcatmplt = "";
#ifdef USE_UFILESYS
  pcatmplt = TfsLoadString("/PCA9555.dat");
#endif  // USE_UFILESYS
#ifdef USE_RULES
  if (!pcatmplt.length()) {
    pcatmplt = RuleLoadFile("PCA9555.DAT");
  }
#endif  // USE_RULES
#ifdef USE_SCRIPT
  if (!pcatmplt.length()) {
    pcatmplt = ScriptLoadSection(">y");
  }
#endif  // USE_SCRIPT
  return pcatmplt;
}

bool PCA9555LoadTemplate(void) {
  AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("PCA: PCA9555LoadTemplate() entry"));

  String pcatmplt = PCA9555TemplateLoadFile();
  uint32_t len = pcatmplt.length() +1;
  if (len < 7) {
	  AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("PCA: template len(%d) < 7"), len);
	  return false;
  }                       // No PcaTmplt found

  JsonParser parser((char*)pcatmplt.c_str());
  JsonParserObject root = parser.getRootObject();
  if (!root) {
	  AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("PCA: no root from JsonParser"));
	  return false;
  }

  // rule3 on file#PCA9555.dat do {"NAME":"PCA9555","GPIO":[32,33,34,35,36,37,38,39,224,225,226,227,228,229,230,231]} endon
  // rule3 on file#PCA9555.dat do {"NAME":"PCA9555","GPIO":[263,262,261,260,259,258,257,256,32,33,34,35,36,37,38,39]} endon
  // rule3 on file#PCA9555.dat do {"NAME":"PCA9555 A=Ri8-1, B=B1-8","GPIO":[263,262,261,260,259,258,257,256,32,33,34,35,36,37,38,39]} endon
  // rule3 on file#PCA9555.dat do {"NAME":"PCA9555 A=Ri8-1, B=B1-8, C=Ri16-9, D=B9-16","GPIO":[263,262,261,260,259,258,257,256,32,33,34,35,36,37,38,39,271,270,269,268,267,266,265,264,40,41,42,43,44,45,46,47]} endon

  // {"NAME":"PCA9555","GPIO":[32,33,34,35,36,37,38,39,224,225,226,227,228,229,230,231]}
  // {"NAME":"PCA9555","GPIO":[32,33,34,35,36,37,38,39,224,225,226,227,228,229,230,231,40,41,42,43,44,45,46,47,232,233,234,235,236,237,238,239]}
  JsonParserToken val = root[PSTR(D_JSON_BASE)];
  if (val) {
    PCA9555.base = (val.getUInt()) ? true : false;
  }
  val = root[PSTR(D_JSON_NAME)];
  if (val) {
    AddLog(LOG_LEVEL_DEBUG, PSTR("PCA: Base %d, Template '%s'"), PCA9555.base, val.getStr());
  }
  JsonParserArray arr = root[PSTR(D_JSON_GPIO)];
  if (arr) {
    uint32_t pin = 0;
    for (pin; pin < PCA9555.max_pins; pin++) {          // Max number of detected chip pins
      JsonParserToken val = arr[pin];
      if (!val) { break; }
      uint16_t mpin = val.getUInt();
      if (mpin) {                                      // Above GPIO_NONE
#ifndef USE_TIANMA_LCD_ON_PCA9555
        if ((mpin >= AGPIO(GPIO_SWT1_NP)) && (mpin < (AGPIO(GPIO_SWT1_NP) + MAX_SWITCHES_SET))) {
          mpin -= (AGPIO(GPIO_SWT1_NP) - AGPIO(GPIO_SWT1));
          PCA9555.switch_max++;
          PCA9555SetPinModes(pin, INPUT);
        } else
#else
        if (pin > 3) {
        	AddLog(LOG_LEVEL_DEBUG, PSTR("PCA: only 4 button inputs allowed with Tianma 16x2 display"), PCA9555.base);
        	mpin = 0;
        	PCA9555_gpio_pin[pin] = mpin;
        	break;
        }
#endif
        if ((mpin >= AGPIO(GPIO_KEY1_NP)) && (mpin < (AGPIO(GPIO_KEY1_NP) + MAX_KEYS_SET))) {
          mpin -= (AGPIO(GPIO_KEY1_NP) - AGPIO(GPIO_KEY1));
          PCA9555.button_max++;
          PCA9555SetPinModes(pin, INPUT);
        }
        else if ((mpin >= AGPIO(GPIO_KEY1_INV_NP)) && (mpin < (AGPIO(GPIO_KEY1_INV_NP) + MAX_KEYS_SET))) {
          bitSet(PCA9555.button_inverted, mpin - AGPIO(GPIO_KEY1_INV_NP));
          mpin -= (AGPIO(GPIO_KEY1_INV_NP) - AGPIO(GPIO_KEY1));
          PCA9555.button_max++;
          PCA9555SetPinModes(pin, INPUT);
        }
#ifndef USE_TIANMA_LCD_ON_PCA9555
        else if ((mpin >= AGPIO(GPIO_REL1)) && (mpin < (AGPIO(GPIO_REL1) + MAX_RELAYS_SET))) {
          PCA9555.relay_max++;
          PCA9555PinMode(pin, OUTPUT);
        }
        else if ((mpin >= AGPIO(GPIO_REL1_INV)) && (mpin < (AGPIO(GPIO_REL1_INV) + MAX_RELAYS_SET))) {
          bitSet(PCA9555.relay_inverted, mpin - AGPIO(GPIO_REL1_INV));
          mpin -= (AGPIO(GPIO_REL1_INV) - AGPIO(GPIO_REL1));
          PCA9555.relay_max++;
          PCA9555PinMode(pin, OUTPUT);
        }
        else if (mpin == AGPIO(GPIO_OUTPUT_HI)) {
          PCA9555PinMode(pin, OUTPUT);
          PCA9555DigitalWrite(pin, 1);
        }
        else if (mpin == AGPIO(GPIO_OUTPUT_LO)) {
          PCA9555PinMode(pin, OUTPUT);
          PCA9555DigitalWrite(pin, 0);
        }
#endif
        else { mpin = 0; }
        PCA9555_gpio_pin[pin] = mpin;
      }
#ifndef USE_TIANMA_LCD_ON_PCA9555
      if ((PCA9555.switch_max >= MAX_SWITCHES_SET) ||
          (PCA9555.button_max >= MAX_KEYS_SET) ||
          (PCA9555.relay_max >= MAX_RELAYS_SET)) {
        AddLog(LOG_LEVEL_INFO, PSTR("PCA: Max reached (S%d/B%d/R%d)"), PCA9555.switch_max, PCA9555.button_max, PCA9555.relay_max);
        break;
      }
#endif
    }
    PCA9555.max_pins = pin;                             // Max number of configured pins
  }
  AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("PCA: Pins %d, PCA9555_gpio_pin %*_V"), PCA9555.max_pins, PCA9555.max_pins, (uint8_t*)PCA9555_gpio_pin);
  return true;
}

uint32_t PCA9555TemplateGpio(void) {
  AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("PCA: PCA9555TemplateGpio"));

  String pcatmplt = PCA9555TemplateLoadFile();
  uint32_t len = pcatmplt.length() +1;
  if (len < 7) {
	  AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("PCA: no template found len(%d)<7"), len);
	  return 0;
  }                           // No PcaTmplt found

  JsonParser parser((char*)pcatmplt.c_str());
  JsonParserObject root = parser.getRootObject();
  if (!root) {
	  AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("PCA: no root object for GPIO template"));
	  return 0;
  }

  JsonParserArray arr = root[PSTR(D_JSON_GPIO)];
  if (arr.isArray()) {
    return arr.size();                                // Number of requested pins
  }

  AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("PCA: GPIO is not an array"));
  return 0;
}

void PCA9555ModuleInit(void) {
  AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("PCA: PCA9555ModuleInit"));

  int32_t pins_needed = PCA9555TemplateGpio();
  if (!pins_needed) {
    AddLog(LOG_LEVEL_DEBUG, PSTR("PCA: Invalid template"));
    return;
  }

   uint8_t PCA9555_address = PCA9555_ADDR_START;
    while ((PCA9555.max_devices < PCA9555_MAX_DEVICES) && (PCA9555_address < PCA9555_ADDR_END)) {
      PCA9555.chip = PCA9555.max_devices;
      if (I2cSetDevice(PCA9555_address)) {
        PCA9555.device[PCA9555.chip].address = PCA9555_address;

        uint8_t buffer;
        if (PCA9555ValidRead(PCA9555_pol0_7, &buffer)) {
          I2cSetActiveFound(PCA9555_address, "PCA9555");
          PCA9555.device[PCA9555.chip].pins = 16;
          PCA9555Write(PCA9555_pol0_7, 0b00000000);     // disable polarity inversion
          PCA9555Write(PCA9555_pol8_15, 0b00000000);    // disable polarity inversion
          PCA9555.max_devices++;

          PCA9555.max_pins += PCA9555.device[PCA9555.chip].pins;
          pins_needed -= PCA9555.device[PCA9555.chip].pins;
        }
      }
      if (pins_needed) {
        PCA9555_address++;
      } else {
        PCA9555_address = PCA9555_ADDR_END;
      }
    }


  if (!PCA9555.max_devices) { return; }

  PCA9555_gpio_pin = (uint16_t*)calloc(PCA9555.max_pins, 2);
  if (!PCA9555_gpio_pin) { return; }

  if (!PCA9555LoadTemplate()) {
    AddLog(LOG_LEVEL_INFO, PSTR("PCA: No valid template found"));  // Too many GPIO's
    PCA9555.max_devices = 0;
    return;
  }
#ifndef USE_TIANMA_LCD_ON_PCA9555
  PCA9555.relay_offset = TasmotaGlobal.devices_present;
  PCA9555.relay_max -= UpdateDevicesPresent(PCA9555.relay_max);
  PCA9555.switch_offset = -1;
#endif
  PCA9555.button_offset = -1;
}

void PCA9555ServiceInput(void) {
  // I found no reliable way to receive interrupts; noise received at undefined moments - unstable usage
  // PCA9555.interrupt = false;
  // This works with no interrupt
  uint32_t pin_offset = 0;
  uint32_t gpio;
  for (PCA9555.chip = 0; PCA9555.chip < PCA9555.max_devices; PCA9555.chip++) {
	gpio = 0;
#ifndef USE_TIANMA_LCD_ON_PCA9555
	gpio = PCA9555Read(PCA9555_in8_15);        	// Read PCA9555 gpio
	gpio <<= 8;
#endif
	gpio |= PCA9555Read(PCA9555_in0_7);         // Read PCA9555 gpio

//    AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("PCA: Chip %d, State %02X"), PCA9555.chip, gpio);

    uint32_t mask = 1;
    for (uint32_t pin = 0; pin < PCA9555.device[PCA9555.chip].pins; pin++) {
      uint32_t state = ((gpio & mask) != 0);
      uint32_t lpin = PCA9555GetPin(pin_offset + pin);  // 0 for None, 32 for KEY1, 160 for SWT1, 224 for REL1
      uint32_t index = lpin & 0x001F;                  // Max 32 buttons or switches
      lpin = BGPIO(lpin);                              // UserSelectablePins number
      if (GPIO_KEY1 == lpin) {
        ButtonSetVirtualPinState(PCA9555.button_offset + index, (state != bitRead(PCA9555.button_inverted, index)));
      }
#ifndef USE_TIANMA_LCD_ON_PCA9555
      else if (GPIO_SWT1 == lpin) {
        SwitchSetVirtualPinState(PCA9555.switch_offset + index, state);
      }
#endif
      mask <<= 1;
    }
    pin_offset += PCA9555.device[PCA9555.chip].pins;
  }
}

void PCA9555Init(void) {
  PCA9555Write(PCA9555_pol0_7, 0b00000000);     // disable polarity inversion
  PCA9555Write(PCA9555_pol8_15, 0b00000000);
}

#ifndef USE_TIANMA_LCD_ON_PCA9555
void PCA9555Power(void) {
  // XdrvMailbox.index = 32-bit rpower bit mask
  // Use absolute relay indexes unique with main template
  power_t rpower = XdrvMailbox.index;
  uint32_t relay_max = TasmotaGlobal.devices_present;
  if (!PCA9555.base) {
    // Use relative and sequential relay indexes
    rpower >>= PCA9555.relay_offset;
    relay_max = PCA9555.relay_max;
  }
  for (uint32_t index = 0; index < relay_max; index++) {
    power_t state = rpower &1;
    if (PCA9555PinUsed(GPIO_REL1, index)) {
      uint32_t pin = PCA9555Pin(GPIO_REL1, index) & 0x3F;   // Fix possible overflow over 63 gpios
      PCA9555DigitalWrite(pin, bitRead(PCA9555.relay_inverted, index) ? !state : state);
    }
    rpower >>= 1;                                      // Select next power
  }
}
#endif

bool PCA9555AddButton(void) {
  // XdrvMailbox.index = button/switch index
  uint32_t index = XdrvMailbox.index;
  if (!PCA9555.base) {
    // Use relative and sequential button indexes
    if (PCA9555.button_offset < 0) { PCA9555.button_offset = index; }
    index -= PCA9555.button_offset;
    if (index >= PCA9555.button_max) { return false; }
  } else {
    // Use absolute button indexes unique with main template
    if (!PCA9555PinUsed(GPIO_KEY1, index)) { return false; }
    PCA9555.button_offset = 0;
  }
  XdrvMailbox.index = (PCA9555DigitalRead(PCA9555Pin(GPIO_KEY1, index)) != bitRead(PCA9555.button_inverted, index));
  return true;
}

#ifndef USE_TIANMA_LCD_ON_PCA9555
bool PCA9555AddSwitch(void) {
  // XdrvMailbox.index = button/switch index
  uint32_t index = XdrvMailbox.index;
  if (!PCA9555.base) {
    // Use relative and sequential switch indexes
    if (PCA9555.switch_offset < 0) { PCA9555.switch_offset = index; }
    index -= PCA9555.switch_offset;
    if (index >= PCA9555.switch_max) { return false; }
  } else {
    // Use absolute switch indexes unique with main template
    if (!PCA9555PinUsed(GPIO_SWT1, index)) { return false; }
    PCA9555.switch_offset = 0;
  }
  XdrvMailbox.index = PCA9555DigitalRead(PCA9555Pin(GPIO_SWT1, index));
  return true;
}
#endif

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv71(uint32_t function) {
  bool i2c_enabled = false;
  i2c_enabled = I2cEnabled(XI2C_86);
  if (!i2c_enabled) { return false; }

  bool result = false;

  if (FUNC_SETUP_RING2 == function) {
    PCA9555ModuleInit();
  } else if (PCA9555.max_devices) {
    switch (function) {
      case FUNC_EVERY_100_MSECOND:
        if (PCA9555.button_max /* || PCA9555.switch_max*/) {
          PCA9555ServiceInput();
        }
        break;

      case FUNC_SET_POWER:
#ifndef USE_TIANMA_LCD_ON_PCA9555
        PCA9555Power();
#endif
        break;

      case FUNC_INIT:
        PCA9555Init();
        break;

      case FUNC_ADD_BUTTON:
        result = PCA9555AddButton();
        break;

      case FUNC_ADD_SWITCH:
#ifndef USE_TIANMA_LCD_ON_PCA9555
        result = PCA9555AddSwitch();
#endif
        break;
    }
  }
  return result;
}

#endif  // USE_PCA9555
#endif  // USE_I2C
