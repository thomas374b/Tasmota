/*
  xdrv_24_Buzzer.ino - buzzer support for Tasmota

  Copyright (C) 2021  Theo Arends

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

#ifdef USE_BUZZER
/*********************************************************************************************\
 * Buzzer support
\*********************************************************************************************/

#define XDRV_24                    24

#ifdef USE_TUNEABLE_BUZZER
	#ifdef PWM_INDEPENDENT_BUZZER_FREQ
		#ifndef BUZZER_FREQ_LIMIT_LOW
			#define	BUZZER_FREQ_LIMIT_LOW		100
		#endif
		#ifndef BUZZER_FREQ_LIMIT_HIGH
			#define	BUZZER_FREQ_LIMIT_HIGH		2000
		#endif
	#endif
#endif

struct BUZZER {
  uint32_t tune = 0;
  uint32_t tune_reload = 0;
  bool active = true;
  bool enable = false;
  uint8_t inverted = 0;            // Buzzer inverted flag (1 = (0 = On, 1 = Off))
  uint8_t count = 0;               // Number of buzzes
  uint8_t mode = 0;                // Buzzer mode (0 = regular, 1 = infinite, 2 = follow LED)
  uint8_t set[2];
  uint8_t duration;
  uint8_t state = 0;
  uint8_t tune_size = 0;
  uint8_t size = 0;
  uint8_t sleep;                   // Current copy of TasmotaGlobal.sleep
#ifdef USE_TUNEABLE_BUZZER
  uint16_t duty = 128;
  int16_t freq = 977;
#ifdef PWM_INDEPENDENT_BUZZER_FREQ
  int16_t freqHighLim = BUZZER_FREQ_LIMIT_HIGH;
  int16_t freqLowLim = BUZZER_FREQ_LIMIT_LOW;
  int16_t attack = 50;
  uint8_t octave = 1;	// one-lined
  uint8_t pattern = 0;	// 1 = SawTooth, 2 = powerchord, 3 = Triangle
  uint8_t toneIdx = 0;
#endif // PWM_INDEPENDENT_BUZZER_FREQ
#endif // USE_TUNEABLE_BUZZER
} Buzzer;

/*********************************************************************************************/

void BuzzerSet(uint32_t state) {
  if (Buzzer.inverted) {
    state = !state;
  }

  if (Settings->flag4.buzzer_freq_mode) {     // SetOption111 - Enable frequency output mode for buzzer
    static uint8_t last_state = 0;
    if (last_state != state) {
      // Set 50% duty cycle for frequency output
      // Set 0% (or 100% for inverted PWM) duty cycle which turns off frequency output either way
//	  AddLog(LOG_LEVEL_DEBUG, PSTR("state:%d, duty:%d, pwm_val[0]:%d"), state, Settings->pwm_range / 2, TasmotaGlobal.pwm_value[0]);

      int32_t pin = Pin(GPIO_BUZZER);

#ifdef USE_TUNEABLE_BUZZER
      // Buzzer volume changes with duty, frequency and used capacitors in hardware
      // there will be one fine spot of pair(duty,frequency) where the buzzer is at loudest

  	  // TODO: make the buzzer volume configurable, depending on frequency
#ifdef PWM_INDEPENDENT_BUZZER_FREQ
      if ((Buzzer.freq < BUZZER_FREQ_LIMIT_LOW) || (BUZZER_FREQ_LIMIT_HIGH < Buzzer.freq)) {
    	  AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("BUZ: fixing wrong frequency %d to 977Hz"), Buzzer.freq);
    	  Buzzer.freq = 977;
      }
#endif // ESP32
      if (Buzzer.duty <= 0) {
    	  // avoid division by zero
    	  AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("BUZ: fixing wrong duty %d to 1/2-th"), Buzzer.duty);
    	  Buzzer.duty = 2;
      }

      // from 2..10 use fraction  1/2 .. 1/10
#if defined(ESP32) && defined(PWM_INDEPENDENT_BUZZER_FREQ)
      uint16_t duty = 255 / Buzzer.duty;
#else
      uint16_t duty = Settings->pwm_range / Buzzer.duty;
      if (Buzzer.duty == 1) {
    	  duty = 1;				// the most silent option
      }
#endif
      if (Buzzer.duty > 10) {
    	  duty = Buzzer.duty;   // >10 use direct what the user has requested
      }
#if defined(ESP32) && defined(PWM_INDEPENDENT_BUZZER_FREQ)
      // enforce safety margins, analogWrite() seems to be 8bit wide
      if (duty > 266) {	// allow values larger than 255 to retain values used for fractions, they will be wrapped 257..266 =>  1..10
    	  duty = 257;	// the most silent option  257 - 2^8 == 1
      }
#endif
	  AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("BUZ: state:%d, duty:%d, "
#ifdef PWM_INDEPENDENT_BUZZER_FREQ
			  "pwm_freq:%d,%d,  "
#endif // PWM_INDEPENDENT_BUZZER_FREQ
			  "range:%d, value: %d")
			  , state, duty
#ifdef PWM_INDEPENDENT_BUZZER_FREQ
			  , Buzzer.freq, Settings->pwm_frequency
#endif // PWM_INDEPENDENT_BUZZER_FREQ
			  , Settings->pwm_range, Settings->pwm_value[0]
			);
#else
#ifdef PWM_INDEPENDENT_BUZZER_FREQ
	#define 	duty 	127
#else
	#define  	duty	(Settings->pwm_range / 2)
#endif
#endif
#if defined(PWM_INDEPENDENT_BUZZER_FREQ) //&& defined(ESP32)
      if (state) {
    	  analogWriteFrequency(Buzzer.freq);
      }
#endif
#if defined(ESP32) && !defined(PWM_INDEPENDENT_BUZZER_FREQ)
      analogWritePhase(pin, (state) ? duty : 0, 0);
#else
      analogWrite(pin, (state) ? duty : 0);
#endif
      last_state = state;
    }
  } else {
    DigitalWrite(GPIO_BUZZER, 0, state);     // Buzzer On/Off
  }
}

//void BuzzerBeep(uint32_t count = 1, uint32_t on = 1, uint32_t off = 1, uint32_t tune = 0, uint32_t mode = 0);
void BuzzerBeep(uint32_t count, uint32_t on, uint32_t off, uint32_t tune, uint32_t mode) {
  Buzzer.set[0] = off;                       // Off duration in 100 mSec steps
  Buzzer.set[1] = on;                        // On duration in 100 mSec steps
  Buzzer.duration = 1;                       // Start buzzer on first step
  Buzzer.size = 0;
  Buzzer.tune_size = 0;
  Buzzer.tune = 0;
  Buzzer.tune_reload = 0;
  Buzzer.mode = mode;

  if (tune) {
    uint32_t tune1 = tune;
    uint32_t tune2 = tune;
    for (uint32_t i = 0; i < 32; i++) {
      if (!(tune2 & 0x80000000)) {
        tune2 <<= 1;                         // Skip leading silence
      } else {
        Buzzer.tune_size++;                  // Allow trailing silence
        Buzzer.tune_reload <<= 1;            // Add swapped tune
        Buzzer.tune_reload |= tune1 & 1;
        tune1 >>= 1;
      }
    }
    Buzzer.size = Buzzer.tune_size;
    Buzzer.tune = Buzzer.tune_reload;
  }
  Buzzer.count = count * 2;                  // Start buzzer

  AddLog(LOG_LEVEL_DEBUG, PSTR("BUZ: Count %d(%d), Time %d/%d, Tune 0x%08X(0x%08X), Size %d, Mode %d"),
    count, Buzzer.count, on, off, tune, Buzzer.tune, Buzzer.tune_size, Settings->flag4.buzzer_freq_mode);

  Buzzer.enable = (Buzzer.count > 0);
  BuzzerShortSleep();
}

void BuzzerShortSleep()
{
  if (Buzzer.enable) {
    Buzzer.sleep = TasmotaGlobal.sleep;
    if (Settings->sleep > PWM_MAX_SLEEP) {
      TasmotaGlobal.sleep = PWM_MAX_SLEEP;   // Set a maxumum value of 10 milliseconds to ensure that buzzer periods are a bit more accurate
    } else {
      TasmotaGlobal.sleep = Settings->sleep;  // Or keep the current sleep if it's lower than 10
    }
  } else {
    TasmotaGlobal.sleep = Buzzer.sleep;      // Restore original sleep
    BuzzerSet(0);
  }
}

void BuzzerSetStateToLed(uint32_t state) {
  if (Buzzer.enable && (2 == Buzzer.mode)) {
    Buzzer.state = (state != 0);
    BuzzerSet(Buzzer.state);
  }
}

void BuzzerBeep(uint32_t count) {
  BuzzerBeep(count, 1, 1, 0, 0);
}

void BuzzerEnabledBeep(uint32_t count, uint32_t duration) {
  if (Settings->flag3.buzzer_enable) {        // SetOption67 - Enable buzzer when available
    BuzzerBeep(count, duration, 1, 0, 0);
  }
}

/*********************************************************************************************/

bool BuzzerPinState(void) {
  if (XdrvMailbox.index == AGPIO(GPIO_BUZZER_INV)) {
    Buzzer.inverted = 1;
    XdrvMailbox.index -= (AGPIO(GPIO_BUZZER_INV) - AGPIO(GPIO_BUZZER));
    return true;
  }
  return false;
}

void BuzzerInit(void) {
  if (PinUsed(GPIO_BUZZER)) {
    pinMode(Pin(GPIO_BUZZER), OUTPUT);
    BuzzerSet(0);
  } else {
    Buzzer.active = false;
  }
}

#ifdef PWM_INDEPENDENT_BUZZER_FREQ
// TODO: create some different alarm noise patterns: ramp up/down frequency in a
//		{   sinodial,
//			sawtooth,				// only linear increase
//			2-step(powerchord),		// play two frequencies simultaneously
//			3-step(accord)			// play two frequencies simultaneously
//		} manner
// TODO: increase/decrease/change frequency after every full wave


typedef enum {
	// octave_octacontra
	// octave_subcontra
	// octave_contra
	// octave_great
	octave_small = 0,		// this is the range we support from 100..2000 Hz
	octave_oneLined = 1,
	octave_twoLined = 2,
	octave_threeLined = 3,
	// octave_fourLined
	// octave_fiveLined
	// octave_sixLined
	// octave_sevenLined
} octave_key_e;

const char *octaveStr[] = {
	"small",
	"oneLined",
	"twoLined",
	"threeLined",
};

const uint16_t Note_C_freq[] = {
		131,
		262,							// C4
		523,							// C5
		1046,
};

const uint16_t Note_G_freq[] = {
		196,							// G3
		392,							// G4
		784,
		1568,
};

typedef enum {
	freqPatternDisabled = 0,
	freqSawTooth = 1,		// a.k.a. shark-fin
	freqSawToothRev = 2,	//
	freqPowerChord = 3,		// original "PowerChord" means: play note C + G simultaneously
	freqTriangle = 4,
	freqVolChange = 5,

	n_freq_patterns
}  freqPattern_e;


typedef struct {
	uint16_t freq[2];   // start, end
	uint8_t duty;		//
	freqPattern_e pattern;
	int16_t attack;
	octave_key_e octave;
	uint8_t duration;
} alarm_pattern_t;

static alarm_pattern_t alarmPat[] = {
		 {{BUZZER_FREQ_LIMIT_LOW, BUZZER_FREQ_LIMIT_HIGH}, 127, freqSawTooth, 100, octave_oneLined, 60}	// sweep 3 times frequency from 100Hz to 2kHz
		,{{BUZZER_FREQ_LIMIT_HIGH, BUZZER_FREQ_LIMIT_LOW}, 127, freqSawToothRev, 100, octave_small, 20}	// sweep 3 times frequency from 2kHz to 100Hz
		,{{262, 392}, 127, freqPowerChord, 200, octave_oneLined, 20}	//
		,{{523, 784}, 253, freqTriangle, 200, octave_twoLined, 20}
};

typedef struct {
	uint16_t freq;
	uint8_t vol;
} freqVol_t;

const static freqVol_t freqVol[] = {
		 {  50,   1}
		,{ 100,   2}
		,{ 250,   4}
		,{ 450,   8}
		,{ 700,  16}
		,{1000,  32}
		,{1350,  64}
		,{1750, 128}
};

#define N_FREQ_VOL  (sizeof(freqVol) / sizeof(freqVol_t))

void BuzzerEvery50mSec(void)
{
	if (!Settings->flag4.buzzer_freq_mode) {
		return;
	}
	if (!Buzzer.enable) {
		return;
	}
	if (Buzzer.pattern < 1) {
		return;
	}
	if (Buzzer.count < 1) {
		return;
	}
    if (!Buzzer.duration) {
    	return;
    }

    Buzzer.duration--;

    if (!Buzzer.duration) {
    	// switch off
    	BuzzerSet(0);

        TasmotaGlobal.sleep = Buzzer.sleep;      // Restore original sleep
        Buzzer.enable = false;

    	Buzzer.count = 0;
        Buzzer.pattern = 0;
    	return;
    }

	switch(Buzzer.pattern) {
		default:
			return;

		case freqSawTooth:
			Buzzer.freq += Buzzer.attack;
		    if (Buzzer.freq > BUZZER_FREQ_LIMIT_HIGH) {
		    	Buzzer.freq = BUZZER_FREQ_LIMIT_LOW;
		    }
			break;

		case freqSawToothRev:
			Buzzer.freq -= Buzzer.attack;
		    if (Buzzer.freq < BUZZER_FREQ_LIMIT_LOW) {
		    	Buzzer.freq = BUZZER_FREQ_LIMIT_HIGH;
		    }
			break;

		case freqPowerChord:
			    //   but we play it sequentially every 50ms creating a 20Hz tremolo
			if (Buzzer.freq != Note_C_freq[Buzzer.octave]) {
				Buzzer.freq = Note_C_freq[Buzzer.octave];
			} else {
				Buzzer.freq = Note_G_freq[Buzzer.octave];
			}
			break;

		case freqTriangle:
			// TODO: create a sine-table with
			// TODO: re-use attack variable as stepping increment of table index
			// TODO: revert the direction

			// actual following waveform is triangle
		    if (Buzzer.freq > Buzzer.freqHighLim) {
		    	Buzzer.attack = -Buzzer.attack;
		    	Buzzer.freq = Buzzer.freqHighLim;
		    }
//		    else {
		    	if (Buzzer.freq < Buzzer.freqLowLim) {
		    		Buzzer.attack = -Buzzer.attack;
		    		Buzzer.freq = Buzzer.freqLowLim;
//		    	} else {
		    	}
//		    }
		    Buzzer.freq += Buzzer.attack;
			break;

		case freqVolChange:
			Buzzer.attack++;
			if (Buzzer.attack >= N_FREQ_VOL) {
				Buzzer.attack = 0;
			}
			Buzzer.freq = freqVol[Buzzer.attack].freq;
			analogWrite(Pin(GPIO_BUZZER), freqVol[Buzzer.attack].vol);
			break;

	}
    analogWriteFrequency(Buzzer.freq);
}



void CmndBuzzerAlert(void)
{
	if (!Settings->flag4.buzzer_freq_mode) {
		AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("BUZ: Alert() can't change frequencies with non-PWM buzzer"));
		return;
	}
	// <index 1..4> <duration 1..1200> <pattern 1..5> <attack_or_octave 1..255 or 0..3>

	alarm_pattern_t *AP = &alarmPat[Buzzer.toneIdx];

	if (XdrvMailbox.data_len > 0) {
		if (XdrvMailbox.payload != 0) {

			uint32_t parm[4] = { 1, 60, freqSawTooth, 200}; 	// 3 seconds duration, sawtooth

			parm[0] = Buzzer.toneIdx+1;
			parm[1] = AP->duration;
			parm[2] = AP->pattern;

			switch(parm[2]) {
				case freqPowerChord:
					parm[3] = AP->octave;
					break;

				case freqTriangle:
					AP->attack = (Buzzer.freqHighLim - Buzzer.freqLowLim) / 8;
					// fall thru by intention
				default:
					parm[3] = AP->attack;
					break;
			}

			ParseParameters(4, parm);

			parm[0]--;
			parm[0] &= 3;
			Buzzer.toneIdx = parm[0];  // default toneIdx

			AP = &alarmPat[Buzzer.toneIdx];

			if (parm[1] < 1) {
				parm[1] = 1;
			}
			if (parm[1] > 60*20) {	// one minute
				parm[1] = 60*20;
			}
			AP->duration = parm[1];

			parm[2] %= n_freq_patterns;   // 0..5
			AP->pattern = (freqPattern_e)parm[2];

			switch(parm[2]) {
				case freqPowerChord:
					parm[3] &= 0x03;
					AP->octave = (octave_key_e)parm[3];
					AP->freq[0] = Note_C_freq[AP->octave];
					AP->freq[1] = Note_G_freq[AP->octave];
					break;

				default:
					AP->attack = (parm[3] & 0xFF);
					break;
			}

			AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("BUZ: Alert() parm[%d,%d,%d,%d]"), parm[0], parm[1], parm[2], parm[3]);
	    } else {
	    	AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("BUZ: Alert() payload:%d"),XdrvMailbox.payload);
	    }
	} else {
    	AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("BUZ: Alert() data_len:%d"),XdrvMailbox.data_len);
	}

	// set pattern, attack, octave from alarm pattern index
	Buzzer.freqHighLim = BUZZER_FREQ_LIMIT_HIGH;
	Buzzer.freqLowLim = BUZZER_FREQ_LIMIT_LOW;

	Buzzer.octave = AP->octave;
	Buzzer.pattern = AP->pattern;

	if (Buzzer.pattern == freqTriangle) {
		Buzzer.freqLowLim = AP->freq[0];
		Buzzer.freqHighLim = AP->freq[1];

//		int16_t fDelta = freqHighLim - freqLowLim;
//		int16_t steps = AP->duration
	}

	Buzzer.freq = AP->freq[0];
	Buzzer.duration = AP->duration;  // TODO: get/overwrite duration from argument
	Buzzer.attack = AP->attack;

	Buzzer.count = 1;	// always 1
	Buzzer.mode = 2;  	// :=2  means "switching off 100ms callback"
	Buzzer.enable = true;

	BuzzerShortSleep();
	BuzzerSet(1);

	ResponseCmndDone();
}


void CmndBuzzerModeAttackOctave(void)
{
//	AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("BUZ: CmndAttackOctave(dl:%d, pl:%d"), XdrvMailbox.data_len, XdrvMailbox.payload);

	if (XdrvMailbox.data_len > 0) {
		if (XdrvMailbox.payload != 0) {
			uint32_t parm[3] = { Buzzer.pattern, 100, Buzzer.octave};
			ParseParameters(3, parm);

			if ((0 > parm[0]) || (parm[0] > 5)) {
				parm[0] = 0;
			}
			if ((-500 > Buzzer.attack) || (Buzzer.attack > 500)) {
				Buzzer.attack = 100;
			}
			parm[1] = Buzzer.attack;
			parm[2] &= 0x03;

			AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("BUZ: CmndAttackOctave() parm[%d,%d,%d]"), parm[0], parm[1], parm[2]);

			Buzzer.pattern = parm[0];
			Buzzer.attack = parm[1];
			Buzzer.octave = parm[2];
//	    } else {
	    }
	} else {
		AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("BUZ: CmndAttackOctave() mode:%d, attack:%d, octave:%s"), Buzzer.mode, Buzzer.attack, octaveStr[Buzzer.octave]);
	}
	ResponseCmndDone();
}
#endif // PWM_INDEPENDENT_BUZZER_FREQ

#ifdef USE_TUNEABLE_BUZZER
void CmndBuzzerTone(void)
{
	uint32_t parm[3];
	parm[2] = 0;

	AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("BUZ: CmdTone(dl:%d, pl:%d"), XdrvMailbox.data_len, XdrvMailbox.payload);
	if (XdrvMailbox.data_len > 0) {
		if (XdrvMailbox.payload != 0) {
#ifndef PWM_INDEPENDENT_BUZZER_FREQ
			Buzzer.freq = Settings->pwm_frequency;
#endif
			parm[0] = Buzzer.freq;
			parm[1] = Buzzer.duty;

			ParseParameters(2, parm);

#ifdef PWM_INDEPENDENT_BUZZER_FREQ
			Buzzer.freq = parm[0];
#endif
			Buzzer.duty = parm[1];
//			AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("BUZ: CmdTone() parm[%d,%d]"), Buzzer.freq, Buzzer.duty);
	    } else {
	    }
	} else {
	}
	if (parm[2] > 0)  {
		Buzzer.duration = parm[2];
		Buzzer.count = 1;	// always 1
		Buzzer.mode = 0;	// enable 100ms callback
#ifdef PWM_INDEPENDENT_BUZZER_FREQ
		Buzzer.pattern = 0;	// disable 50ms callback
#endif
		Buzzer.enable = true;
		BuzzerSet(1);
	}
	AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("BUZ: CmdTone() freq:%d, duty:%d"), Buzzer.freq, Buzzer.duty);

	ResponseCmndDone();
}

#endif // USE_TUNEABLE_BUZZER

void BuzzerEvery100mSec(void) {
  if (Buzzer.enable && (Buzzer.mode != 2)) {
    if (Buzzer.count) {
      if (Buzzer.duration) {
        Buzzer.duration--;
        if (!Buzzer.duration) {
          if (Buzzer.size) {
            Buzzer.size--;
            Buzzer.state = Buzzer.tune & 1;
            Buzzer.tune >>= 1;
          } else {
            Buzzer.size = Buzzer.tune_size;
            Buzzer.tune = Buzzer.tune_reload;
            Buzzer.count -= (Buzzer.tune_reload) ? 2 : 1;
            Buzzer.state = Buzzer.count & 1;
            if (Buzzer.mode) {
              Buzzer.count |= 2;
            }
          }
          Buzzer.duration = Buzzer.set[Buzzer.state];
        }
      }
      BuzzerSet(Buzzer.state);
    } else {
      TasmotaGlobal.sleep = Buzzer.sleep;      // Restore original sleep
      Buzzer.enable = false;
    }
  }
}

/*********************************************************************************************\
 * Commands
\*********************************************************************************************/

const char kBuzzerCommands[] PROGMEM = "Buzzer|"  // Prefix
#ifdef PWM_INDEPENDENT_BUZZER_FREQ
  "Active|Pwm|Tone|MdAttOct|Alert||";
#else
#ifdef USE_TUNEABLE_BUZZER
  "Active|Pwm|Tone||" ;
#else
  "Active|Pwm||" ;
#endif
#endif

SO_SYNONYMS(kBuzzerSynonyms,
  67, 111
);

void (* const BuzzerCommand[])(void) PROGMEM = {
#ifdef USE_TUNEABLE_BUZZER
  &CmndBuzzerTone,
#endif
#ifdef PWM_INDEPENDENT_BUZZER_FREQ
  &CmndBuzzerModeAttackOctave,
  &CmndBuzzerAlert,
#endif
  &CmndBuzzer };

void CmndBuzzer(void) {
  // Buzzer <number of beeps>,<duration of beep in 100mS steps>,<duration of silence in 100mS steps>,<tune>
  // All parameters are optional
  //
  // Buzzer              = Buzzer 1,1,1 = Beep once with both duration and pause set to 100mS
  // Buzzer 0            = Stop active beep cycle
  // Buzzer 2            = Beep twice with duration 200mS and pause 100mS
  // Buzzer 2,3          = Beep twice with duration 300mS and pause 100mS
  // Buzzer 2,3,4        = Beep twice with duration 300mS and pause 400mS
  // Buzzer 2,3,4,0x0F54 = Beep a sequence twice indicated by 0x0F54 = 1111 0101 0100 with duration 300mS and pause 400mS
  //                         Notice skipped leading zeroes but valid trailing zeroes
  // Buzzer -1           = Beep infinite
  // Buzzer -2           = Beep following link led

  if (XdrvMailbox.data_len > 0) {
    if (XdrvMailbox.payload != 0) {
      uint32_t parm[4] = { 0 };
      ParseParameters(4, parm);
      uint32_t mode = 0;
      if (XdrvMailbox.payload < 0) {
        parm[0] = 1;                         // Default Count
        if (XdrvMailbox.payload > -3) {
          mode = -XdrvMailbox.payload;       // 0, 1 or 2
        }
      }
      for (uint32_t i = 1; i < 3; i++) {
        if (parm[i] < 1) { parm[i] = 1; }    // Default On time, Off time
      }
      BuzzerBeep(parm[0], parm[1], parm[2], parm[3], mode);
    } else {
      BuzzerBeep(0);
    }
  } else {
    BuzzerBeep(1);
  }
  ResponseCmndDone();
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv24(uint32_t function) {
  bool result = false;

  if (Buzzer.active) {
    switch (function) {
#ifdef PWM_INDEPENDENT_BUZZER_FREQ
      case FUNC_EVERY_50_MSECOND:
    	  BuzzerEvery50mSec();
    	  break;
#endif
      case FUNC_EVERY_100_MSECOND:
        BuzzerEvery100mSec();
        break;
      case FUNC_COMMAND:
        result = DecodeCommand(kBuzzerCommands, BuzzerCommand, kBuzzerSynonyms);
        break;
      case FUNC_PRE_INIT:
        BuzzerInit();
        break;
      case FUNC_PIN_STATE:
        result = BuzzerPinState();
        break;
    }
  }
  return result;
}

#endif  // USE_BUZZER
