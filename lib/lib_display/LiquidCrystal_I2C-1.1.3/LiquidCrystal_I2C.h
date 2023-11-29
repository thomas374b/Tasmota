//YWROBOT
#ifndef LiquidCrystal_I2C_h
#define LiquidCrystal_I2C_h

#include <inttypes.h>
#include "Print.h" 
#include <Wire.h>

// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

// flags for backlight control
#define LCD_BACKLIGHT 0x08
#define LCD_NOBACKLIGHT 0x00

#define En B00000100  // Enable bit
#define Rw B00000010  // Read/Write bit
#define Rs B00000001  // Register select bit

/* possible extra compiler definitions
// #define  USE_TIANMA_LCD_ON_PCA9555		// mod to use this display
// #define  LCD_I2C_PIN_BACKLIGHT			// define this a the pin number for the backlight control, default has backlight always on
*/
#ifdef USE_TIANMA_LCD_ON_PCA9555   // <== one must put this to the build environment
								   // it's not sufficient in user_config_override.h
	#ifndef USE_PCA9555
		#define USE_PCA9555
	#endif
	#ifndef USE_PCA9555_LCD
		#define USE_PCA9555_LCD
	#endif
	#ifndef USE_PCA9555_LCD_ADDR
		#define USE_PCA9555_LCD_ADDR	0x20	// no other options, hardcoded in PCB with via to GND layer
	#endif

    // i2c-gpio-expander layout
	#define		LCD_I2C_KEYREG_ADDR	0x00		//	address of io0 register on pca9555 to read
	#define		LCD_I2C_LCDREG_ADDR	0x03		//  address of io1 register on pca9555 to write
	
	// pin connections from LCD to i2c-gpio-expander
	#define		LCD_I2C_PIN_RS		7		//	io1.7 on pca9555
	#define		LCD_I2C_PIN_RW		6		//	io1.6 on pca9555
	#define		LCD_I2C_PIN_EN		5		//	io1.5 on pca9555
	#define		LCD_I2C_PIN_D4		4		//	io1.4 on pca9555
	#define		LCD_I2C_PIN_D5		3		//	io1.3 on pca9555
	#define		LCD_I2C_PIN_D6		2		//	io1.2 on pca9555
	#define		LCD_I2C_PIN_D7		1		//	io1.1 on pca9555
	
	#ifndef USE_LCD_PORT_BIT_MANGLING
		#define	USE_LCD_PORT_BIT_MANGLING		// 	is a must on this device
	#endif
	

/*

	// buttons connections to i2c-gpio-expander
	#define	 	LCD_KEY_BACK_MASK	_bv(0)		//	io0.0 on pca9555
	#define	 	LCD_KEY_DOWN_MASK	_bv(1)		//	io0.1 on pca9555
	#define	 	LCD_KEY_UP_MASK		_bv(2)		//	io0.2 on pca9555
	#define	 	LCD_KEY_ENTER_MASK	_bv(3)		//	io0.3 on pca9555
*/
	#undef LCD_I2C_HAS_BACKLIGHT			// backlight is always on, hardwired
#endif // USE_TIANMA_LCD_ON_PCA9555

#if defined(LCD_I2C_PIN_BACKLIGHT)
	#if (LCD_I2C_PIN_BACKLIGHT != -1)
		#define		LCD_I2C_HAS_BACKLIGHT		1
	#endif
#endif

class LiquidCrystal_I2C : public Print {
public:
  LiquidCrystal_I2C(uint8_t lcd_Addr,uint8_t lcd_cols,uint8_t lcd_rows);
  void begin(uint8_t cols, uint8_t rows, uint8_t charsize = LCD_5x8DOTS );
  void clear();
  void home();
  void noDisplay();
  void display();
  void noBlink();
  void blink();
  void noCursor();
  void cursor();
  void scrollDisplayLeft();
  void scrollDisplayRight();
  void printLeft();
  void printRight();
  void leftToRight();
  void rightToLeft();
  void shiftIncrement();
  void shiftDecrement();
  void noBacklight();
  void backlight();
  void autoscroll();
  void noAutoscroll(); 
  void createChar(uint8_t, uint8_t[]);
  void setCursor(uint8_t, uint8_t); 
#if defined(ARDUINO) && ARDUINO >= 100
  virtual size_t write(uint8_t);
#else
  virtual void write(uint8_t);
#endif
  void command(uint8_t);
  void init();

////compatibility API function aliases
void blink_on();						// alias for blink()
void blink_off();       					// alias for noBlink()
void cursor_on();      	 					// alias for cursor()
void cursor_off();      					// alias for noCursor()
void setBacklight(uint8_t new_val);				// alias for backlight() and nobacklight()
void load_custom_character(uint8_t char_num, uint8_t *rows);	// alias for createChar()
void printstr(const char[]);

////Unsupported API functions (not implemented in this library)
uint8_t status();
void setContrast(uint8_t new_val);
uint8_t keypad();
void setDelay(int,int);
void on();
void off();
uint8_t init_bargraph(uint8_t graphtype);
void draw_horizontal_graph(uint8_t row, uint8_t column, uint8_t len,  uint8_t pixel_col_end);
void draw_vertical_graph(uint8_t row, uint8_t column, uint8_t len,  uint8_t pixel_col_end);
	 

private:
  void init_priv();
  void send(uint8_t, uint8_t);
  void write4bits(uint8_t);
  void expanderWrite(uint8_t);
  void pulseEnable(uint8_t);
  uint8_t _Addr;
  uint8_t _displayfunction;
  uint8_t _displaycontrol;
  uint8_t _displaymode;
  uint8_t _numlines;
  uint8_t _cols;
  uint8_t _rows;
  uint8_t _backlightval;
};

#endif
