
#include <stdbool.h>
#include <string.h>
#include "hal_port.h"
#include "defines.h"
#include "chip_init.h"
#include "config.h"

#define NUNCHUK_I2C_ADDRESS 0xA4

uint8_t  ppm_count = 0;
uint8_t  pwm_count = 0;
uint32_t timeoutCntGen = TIMEOUT;
uint8_t  timeoutFlgGen = 0;
uint8_t  nunchuk_data[6] = {0};

uint8_t i2cBuffer[2];
nunchuk_state nunchukState = NUNCHUK_CONNECTING;

#if defined(CONTROL_PPM_LEFT) || defined(CONTROL_PPM_RIGHT)
uint16_t ppm_captured_value[PPM_NUM_CHANNELS + 1] = {500, 500};
uint16_t ppm_captured_value_buffer[PPM_NUM_CHANNELS+1] = {500, 500};
uint32_t ppm_timeout = 0;

bool ppm_valid = true;

void PPM_ISR_Callback(void) {
  // Dummy loop with 16 bit count wrap around
  uint16_t rc_delay = TIM2->CNT;
  TIM2->CNT = 0;

  if (rc_delay > 3000) {
    if (ppm_valid && ppm_count == PPM_NUM_CHANNELS) {
      ppm_timeout = 0;
      timeoutCntGen = 0;
      timeoutFlgGen = 0;
      memcpy(ppm_captured_value, ppm_captured_value_buffer, sizeof(ppm_captured_value));
    }
    ppm_valid = true;
    ppm_count = 0;
  }
  else if (ppm_count < PPM_NUM_CHANNELS && IN_RANGE(rc_delay, 900, 2100)){
    ppm_captured_value_buffer[ppm_count++] = CLAMP(rc_delay, 1000, 2000) - 1000;
  } else {
    ppm_valid = false;
  }
}

// SysTick executes once each ms
void PPM_SysTick_Callback(void) {
  ppm_timeout++;
  // Stop after 500 ms without PPM signal
  if(ppm_timeout > 500) {
    int i;
    for(i = 0; i < PPM_NUM_CHANNELS; i++) {
      ppm_captured_value[i] = 500;
    }
    ppm_timeout = 0;
  }
}

/* PPM_Init() 已搬到 chip_init.c */
#endif


#if defined(CONTROL_PWM_LEFT) || defined(CONTROL_PWM_RIGHT)
 /*
  * Illustration of the PWM functionality
  * CH1 ________|‾‾‾‾‾‾‾‾‾‾|________
  * CH2 ______________|‾‾‾‾‾‾‾‾‾‾‾|________
  *             ↑     ↑    ↑      ↑
  * TIM2       RST  SAVE RC_CH1 RC_CH1
 */

uint16_t pwm_captured_ch1_value = 500;
uint16_t pwm_captured_ch2_value = 500;
uint16_t pwm_CNT_prev_ch1 = 0;
uint16_t pwm_CNT_prev_ch2 = 0;
uint32_t pwm_timeout_ch1 = 0;
uint32_t pwm_timeout_ch2 = 0;
// Last accepted stable value — used to detect motor back-EMF noise spikes.
// A new reading is only accepted if it is within PWM_NOISE_FILTER_US of the
// current stable value, preventing isolated noise pulses from corrupting the
// captured PWM width.
#define PWM_NOISE_FILTER_US  200
static uint16_t pwm_stable_ch1 = 500;
static uint16_t pwm_stable_ch2 = 500;

void PWM_ISR_CH1_Callback(void) {
  // Dummy loop with 16 bit count wrap around
  if(hal_gpio_read(HAL_PIN_PWM_CH1)) {   // Rising  Edge interrupt -> save timer value OR reset timer
    if (hal_gpio_read(HAL_PIN_PWM_CH2)) {
      pwm_CNT_prev_ch1 = TIM2->CNT;
    } else {
      TIM2->CNT = 0;
      pwm_CNT_prev_ch1 = 0;
    }
  } else {                                    // Falling Edge interrupt -> measure pulse duration
    uint16_t rc_signal = TIM2->CNT - pwm_CNT_prev_ch1;
    if (IN_RANGE(rc_signal, 900, 2100)){
      uint16_t new_val = CLAMP(rc_signal, 1000, 2000) - 1000;
      int16_t  delta   = (int16_t)new_val - (int16_t)pwm_stable_ch1;
      if (delta < 0) delta = -delta;
      // Accept only if change is gradual (real RC) or channel has timed out
      if (delta <= PWM_NOISE_FILTER_US || pwm_timeout_ch1 > 100) {
        timeoutCntGen = 0;
        timeoutFlgGen = 0;
        pwm_timeout_ch1 = 0;
        pwm_stable_ch1          = new_val;
        pwm_captured_ch1_value  = new_val;
      }
    }
  }
}

void PWM_ISR_CH2_Callback(void) {
  // Dummy loop with 16 bit count wrap around
  if(hal_gpio_read(HAL_PIN_PWM_CH2)) {   // Rising  Edge interrupt -> save timer value OR reset timer
    if (hal_gpio_read(HAL_PIN_PWM_CH1)) {
      pwm_CNT_prev_ch2 = TIM2->CNT;
    } else {
      TIM2->CNT = 0;
      pwm_CNT_prev_ch2 = 0;
    }
  } else {                                    // Falling Edge interrupt -> measure pulse duration
    uint16_t rc_signal = TIM2->CNT - pwm_CNT_prev_ch2;
    if (IN_RANGE(rc_signal, 900, 2100)){
      uint16_t new_val = CLAMP(rc_signal, 1000, 2000) - 1000;
      int16_t  delta   = (int16_t)new_val - (int16_t)pwm_stable_ch2;
      if (delta < 0) delta = -delta;
      if (delta <= PWM_NOISE_FILTER_US || pwm_timeout_ch2 > 100) {
        timeoutCntGen = 0;
        timeoutFlgGen = 0;
        pwm_timeout_ch2 = 0;
        pwm_stable_ch2          = new_val;
        pwm_captured_ch2_value  = new_val;
      }
    }
  }
}

// SysTick executes once each ms
void PWM_SysTick_Callback(void) {
  pwm_timeout_ch1++;
  pwm_timeout_ch2++;
  // Stop after 500 ms without PWM signal
  if(pwm_timeout_ch1 > 500) {
    pwm_captured_ch1_value = 500;
    pwm_timeout_ch1 = 0;
  }
  if(pwm_timeout_ch2 > 500) {
    pwm_captured_ch2_value = 500;
    pwm_timeout_ch2 = 0;
  }
}

/* PWM_Init() 已搬到 chip_init.c */
#endif

uint8_t Nunchuk_tx(uint8_t i2cBuffer[], uint8_t i2cBufferLength) {
  if(hal_i2c_master_transmit(HAL_I2C_NUNCHUK,NUNCHUK_I2C_ADDRESS,(uint8_t*)i2cBuffer, i2cBufferLength, 100) == HAL_PORT_OK) {
    return true;
  }
  return false;
}

uint8_t Nunchuk_rx(uint8_t i2cBuffer[], uint8_t i2cBufferLength) {
  if(hal_i2c_master_receive(HAL_I2C_NUNCHUK,NUNCHUK_I2C_ADDRESS,(uint8_t*)i2cBuffer, i2cBufferLength, 100) == HAL_PORT_OK) {
    return true;
  }
  return false;
}

uint8_t Nunchuk_Init(void) {
  //-- START -- init WiiNunchuk
  i2cBuffer[0] = 0xF0;
  i2cBuffer[1] = 0x55;

  if(Nunchuk_tx(i2cBuffer, 2) == false) {
    return false;
  }
  hal_delay_ms(10);

  i2cBuffer[0] = 0xFB;
  i2cBuffer[1] = 0x00;

  if(Nunchuk_tx(i2cBuffer, 2) == false) {
    return false;
  }
  hal_delay_ms(10);

  return true;
}

uint8_t Nunchuk_Connect() {
  /* Initialise / re-initialise I2C peripheral */
  I2C_Init();
  
  /* Initialise / re-initialise nunchuk */
  if(Nunchuk_Init() == true) {
    nunchukState = NUNCHUK_CONNECTED;
    return true;
  } else {
    return false;
  }
}

nunchuk_state Nunchuk_Read(void) {
  static uint8_t delay_counter = 0;
  uint16_t checksum = 0;
  uint8_t success = true;
  uint8_t i = 0;

  switch(nunchukState) {
    case NUNCHUK_DISCONNECTED:
      success = false;
      /* Delay a bit before reconnecting */
      if(delay_counter++ > 100) {
        success = Nunchuk_Connect();
        delay_counter = 0;
      }
      break;
      
    case NUNCHUK_CONNECTING:
    case NUNCHUK_RECONNECTING:
        /* Try to reconnect once, if fails again fall back to disconnected state */
        success = Nunchuk_Connect();
        if(!success) {
          nunchukState = NUNCHUK_DISCONNECTED;
        }
      break;

    case NUNCHUK_CONNECTED:
      /* Send read address of 0x00 to the Nunchuk */
      i2cBuffer[0] = 0x00;
      if(!Nunchuk_tx(i2cBuffer, 1)) {
        success = false;
      }
      hal_delay_ms(3);

      /* Clear the receive data buffer */
      for(i = 0; i<6; i++) {
        nunchuk_data[i] = 0;
      }

      /* Read back 6 bytes from the Nunchuk */
      if(!Nunchuk_rx(nunchuk_data, 6)) {
        success = false;
      }
      hal_delay_ms(3);

      /* Checksum the receive buffer to ensure it is not in an error condition, i.e. all 0x00 or 0xFF */
      for(i = 0; i<6; i++) {
        checksum += nunchuk_data[i];
      }
      if(checksum == 0 || checksum == 0x5FA) {
        success = false;
      }

      /* Comms failure or timeout counter reached timeout limit */
      if(success == false || timeoutCntGen > 3) {
        /* Clear the receive data buffer */
        for(i = 0; i<6; i++) {
          nunchuk_data[i] = 0;
        }
        /* Brings motors to safe stop */
        /* Expected values from nunchuk for stopped (mid) position */
        nunchuk_data[0] = 127;
        nunchuk_data[1] = 128;
        timeoutFlgGen = 1;
        nunchukState = NUNCHUK_RECONNECTING;
      }
      break;
  }
  /* Reset the timeout flag and counter if successful communication */
  if(success == true) {
    timeoutCntGen = 0;
    timeoutFlgGen = 0;
  }
  return nunchukState;
  //setScopeChannel(0, (int)nunchuk_data[0]);
  //setScopeChannel(1, (int)nunchuk_data[1]);
  //setScopeChannel(2, (int)nunchuk_data[5] & 1);
  //setScopeChannel(3, ((int)nunchuk_data[5] >> 1) & 1);
}
