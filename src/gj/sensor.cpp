#include "sensor.h"
#include "esputils.h"
#include "platform.h"
#include "eventmanager.h"

#ifdef ESP32
  #include "gjulp_main.h"
  //#define CONFIG_RTCIO_SUPPORT_RTC_GPIO_DESC

  #include <driver/rtc_io.h>
  #include <driver/adc.h>
  #include <esp_adc_cal.h>
  #include <esp32-hal-gpio.h>
#elif defined(NRF)
  #include <nrf_gpio.h>
  #include <nrf_gpiote.h>
  #include <nrf_drv_gpiote.h>

  #include <nrf_drv_timer.h>
  #include <nrf_soc.h>
#endif

void Sensor::SetFlags(Flags flags)
{
  m_flags = flags;
}
Sensor::Flags Sensor::GetFlags() const
{
  return m_flags;
}

bool Sensor::IsFlagSet(Flags flags) const
{
  return (m_flags & flags) != Flags::None;
}
#if defined(NRF)

static nrf_drv_timer_t timer = NRF_DRV_TIMER_INSTANCE(1);
void timer_dummy_handler(nrf_timer_event_t event_type, void * p_context){}


static bool s_gpioteInit = false;

void InitGPIOTE()
{
  if (s_gpioteInit)
    return;

  int32_t err_code;
  if(!nrf_drv_gpiote_is_init())
  {
    err_code = nrf_drv_gpiote_init();
    APP_ERROR_CHECK(err_code);
  }
  /*

    nrf_drv_timer_config_t timer_cfg =
    {
		.frequency          = (nrf_timer_frequency_t)3,
		.mode               = (nrf_timer_mode_t)NRF_TIMER_MODE_TIMER,
		.bit_width          = (nrf_timer_bit_width_t)NRF_TIMER_BIT_WIDTH_32,
		.interrupt_priority = 0,
		.p_context          = NULL
	};
    err_code = nrf_drv_timer_init(&timer, &timer_cfg, timer_dummy_handler);
    APP_ERROR_CHECK(err_code);

    // Enable timer
    nrf_drv_timer_enable(&timer);*/
  s_gpioteInit = true;
}
#endif

void Sensor::SetPin(uint16_t pin)
{
  m_pin = pin; 
}

DigitalSensor::DigitalSensor(uint16_t refresh)
: m_refresh(refresh)
, m_fallCount(0)
{

}
/*
void DigitalSensor::SetInteruptHandler(PfnInterruptHandler handler)
{
  m_interruptHandler = handler;
}*/

void DigitalSensor::EnableInterrupts(bool enable)
{
  if (enable && !m_enableInterrupts)
  {
#if defined(NRF)
    InitGPIOTE();

    nrf_gpio_pin_pull_t nrfPull = NRF_GPIO_PIN_NOPULL;
    if (m_pull < 0)
      nrfPull = NRF_GPIO_PIN_PULLDOWN;
    else if (m_pull > 0)
      nrfPull = NRF_GPIO_PIN_PULLUP;

    bool is_watcher = false;
    bool hi_accuracy = false;
  nrf_drv_gpiote_in_config_t cfg = {
    NRF_GPIOTE_POLARITY_TOGGLE,
    nrfPull,
    is_watcher,
    hi_accuracy
  };

  const int32_t pin = GetPin();

  int32_t err_code = nrf_drv_gpiote_in_init(pin, &cfg, InterruptHandler);
  APP_ERROR_CHECK(err_code);

  nrf_drv_gpiote_in_event_enable(pin, false);

  int8_t remap = s_remap[pin];

  if (s_remap[pin] == 255)
  {
    for (int32_t i = 0 ; i < MaxRemaps ; ++i)
    {
      if (s_sensors[i] == nullptr)
      {
        remap = i;
        break;
      }
    }
  }

  if (remap == 255)
  {
    SER("ERROR:Out of pin remaps\n");
  }
  else
  {
    s_remap[pin] = remap;
    s_sensors[remap] = this;
  }

#endif
  }
  else if (!enable && m_enableInterrupts)
  {
    #if defined(NRF)
      const int32_t pin = GetPin();

      nrf_drv_gpiote_in_event_disable(pin);

      if (s_remap[pin] != 255)
      {
        int32_t remap = s_remap[pin];
        s_sensors[remap] = nullptr;
        s_remap[pin] = 255;
      }

      nrf_drv_gpiote_in_uninit(pin);
      
    #endif
  }

  m_enableInterrupts = enable;
}

void DigitalSensor::SetPin(uint16_t pin, int32_t pull)
{
  Sensor::SetPin(pin);

  m_pull = pull;
  SetupPin(pin, true, pull);

#ifdef ESP32
  UpdateValue();
  attachInterruptArg(digitalPinToInterrupt(pin), InterruptHandler, this, CHANGE);
#elif defined(NRF)
  if (m_enableInterrupts)
  {
    EnableInterrupts(false);
    EnableInterrupts(true);
  }
#endif
}

//Anything called from interrupt routine must be IRAM_ATTR.
//IRAM_ATTR needed because interrupt calls bypasses i-cache loads
// https://github.com/espressif/arduino-esp32/issues/954
void GJ_IRAM DigitalSensor::OnChange()
{
  const bool updated = UpdateValue();

  if (updated && m_postISR)
  {
    m_postISR(*this);
  }
}

#ifdef ESP32
void GJ_IRAM DigitalSensor::InterruptHandler(void *param)
{
  DigitalSensor *sensor = (DigitalSensor*)param;
  sensor->OnChange();
}
#elif defined(NRF)

DigitalSensor* DigitalSensor::s_sensors[DigitalSensor::MaxRemaps]; 
uint8_t DigitalSensor::s_remap[32] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

void DigitalSensor::InterruptHandler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
  uint8_t index = s_remap[pin];

  if (index < MaxRemaps && s_sensors[index] != nullptr)
  {
    DigitalSensor *sensor = s_sensors[index];
    sensor->OnChange();
  }
}
#endif

void DigitalSensor::Update()
{
  uint32_t newChangeCount = 0;
  uint32_t newFallCount = 0;
  uint32_t newRiseCount = 0;

  //interrupts are missing on fast changing state, update it here
  UpdateValue();
  
  if (m_fallCount)
  {
    uint32_t count = m_fallCount;
    #ifdef ESP32
      m_fallCount -= count;
    #elif defined(NRF)
      GJAtomicSub(m_fallCount, count);
    #endif

    newFallCount += count;
  }

  if (m_changeCount)
  {
    uint32_t count = m_changeCount;
    #ifdef ESP32
      m_changeCount -= count;
    #elif defined(NRF)
      GJAtomicSub(m_changeCount, count);
    #endif

    newChangeCount += count;
  }
#ifdef ESP32
  if (!m_wakeHandled && IsSleepEXTWakeUp())
  {
    //uint64_t const wakePins = esp_sleep_get_ext1_wakeup_status();
    //uint64_t const pinMask = (uint64_t)1 << GetPin();
    //LOG("Wake from ext1 %x%x pinmask:%x%x\n\r", (uint32_t)(wakePins >> 32), (uint32_t)wakePins, (uint32_t)(pinMask >> 32), (uint32_t)pinMask);
    
    //esp_sleep_get_ext1_wakeup_status always returns 0

    #if 0
    uint32_t const current = digitalRead( GetPin() );
    
    if (current == 0)
    {
      LOG("Adding 1 fall event from GPIO %d wake\n\r", GetPin());
      newFallCount += 1;
    }
    #endif
  }
  else if (!m_wakeHandled && IsSleepUlpWakeUp())
  {
    uint32_t ulpCount = 0;
    
    uint32_t const rtcGPIO = digitalPinToRtcPin((gpio_num_t)GetPin());
    
    uint32_t const changeEvents = ulp_sensor_events ? (*ulp_sensor_events)[rtcGPIO * UlpCount_Count + UlpCount_Change] : 0;
    uint32_t const riseEvents =   ulp_sensor_events ? (*ulp_sensor_events)[rtcGPIO * UlpCount_Count + UlpCount_Rise] : 0;
    uint32_t const fallEvents =   ulp_sensor_events ? (*ulp_sensor_events)[rtcGPIO * UlpCount_Count + UlpCount_Fall] : 0;
        
    newChangeCount += changeEvents;
    newFallCount += fallEvents;
    newRiseCount += riseEvents;
  }
#endif

  m_wakeHandled = true;
  
  if (newChangeCount)
  {
    if (m_onChange)
      m_onChange(*this, newChangeCount);  
    else if (m_onConstChange)
      m_onConstChange(*this, newChangeCount);  
    else
      DO_ONCE(SER("WARNING:No registered 'onChange' callback for sensor on pin %d\n\r", GetPin()));
  }

  if (newFallCount)
  {
    if (m_onFall)
      m_onFall(*this, newFallCount);  
    else
      DO_ONCE(SER("WARNING:No registered 'onFall' callback for sensor on pin %d\n\r", GetPin()));
  }
}

bool GJ_IRAM DigitalSensor::UpdateValue()
{
  bool updated = false;

  uint32_t current = ReadPin( GetPin() );
  
  //printf("DigitalSensor::UpdateValue current=%d\n\r", current);

  if ( current != m_value )
  {
    //use gpio pin to feed V+ instead of using directly VCC
    //and then set it to LOW to check if connected
    m_detected = true;
    
    #ifdef ESP32
      m_changeCount++;
    #elif defined(NRF)
      GJAtomicAdd(m_changeCount, 1);
    #endif

    uint32_t const m = GetElapsedMillis();
    uint32_t const e = m - m_lastChange;

    if ( e > m_refresh )
    {
      if (m_value != 0 && current == 0)
      {
        #ifdef ESP32
          m_fallCount++;
        #elif defined(NRF)
          GJAtomicAdd(m_fallCount, 1);
        #endif
      }
      
      m_value = current;
      m_lastChange = m;

      updated = true;
    }
  }

  return updated;
}

void DigitalSensor::OnChangeConst(ConstCallback cb)
{
  m_onConstChange = cb;
}
void DigitalSensor::OnChange(TCallback cb)
{
  m_onChange = cb;
}

void DigitalSensor::OnFall(ConstCallback cb)
{
  m_onFall = cb;
}

void DigitalSensor::SetPostISRCB(TPostISRCallback cb)
{
  m_postISR = cb;
}

AutoToggleSensor::AutoToggleSensor(U16 refresh)
: DigitalSensor(refresh)
{

}

void AutoToggleSensor::SetPin(uint16_t pin)
{
  SetupPin(pin, true, 0);
  
  uint32_t val = ReadPin(pin);
  int32_t pull = 0;
  if (val == 0)
    pull = -1;
  else
    pull = 1;

  DigitalSensor::SetPin(pin, pull);
  DigitalSensor::OnChange(OnDigitalChange);
  DigitalSensor::EnableInterrupts(true);
}

void AutoToggleSensor::SetOnChange(TCallback cb)
{
  m_onChange = cb;
}

void AutoToggleSensor::OnDigitalChange(DigitalSensor &digitalSensor, uint32_t count)
{
  AutoToggleSensor &sensor = *(AutoToggleSensor*)&digitalSensor;

  const int32_t val = sensor.GetValue();

  int32_t pull = 0;
  if (val == 0)
    pull = -1;
  else
    pull = 1;

  digitalSensor.SetPin(sensor.GetPin(), pull);

  if (sensor.m_onChange)
    sensor.m_onChange(sensor, count);
}


#ifdef ESP32
bool GetADC1Channel(uint32_t gpio, adc1_channel_t &channel)
{
  channel = ADC1_CHANNEL_MAX;
  
  //find gpio channel
  gpio_num_t gpio_num = {};
  for (uint32_t i = ADC1_CHANNEL_0 ; i < ADC1_CHANNEL_MAX ; ++i)
  {
    gpio_num = {};
    adc1_pad_get_io_num((adc1_channel_t)i, &gpio_num);
    
    if (gpio_num == gpio)
    {
      channel = (adc1_channel_t)i;
      break;
    }
  }

  return channel != ADC1_CHANNEL_MAX;
}

bool GetADC2Channel(uint32_t gpio, adc2_channel_t &channel)
{
  channel = ADC2_CHANNEL_MAX;
  
  //find gpio channel
  gpio_num_t gpio_num = {};
  for (uint32_t i = ADC2_CHANNEL_0 ; i < ADC2_CHANNEL_MAX ; ++i)
  {
    gpio_num = {};
    adc1_pad_get_io_num((adc1_channel_t)i, &gpio_num);
    
    if (gpio_num == gpio)
    {
      channel = (adc2_channel_t)i;
      break;
    }
  }
    
  return channel != ADC2_CHANNEL_MAX;
}


bool GetADCChannel(uint32_t gpio, bool &isAdc1, uint32_t &channel)
{
  if (GetADC1Channel(gpio, *(adc1_channel_t*)&channel))
  {
    isAdc1 = true;
  }
  else if (GetADC2Channel(gpio, *(adc2_channel_t*)&channel))
  {
    isAdc1 = false;
  }
  else
  {
    return false;
  }
  return true;
}
#endif

AnalogSensor::AnalogSensor(uint32_t reads)
: m_reads(reads)
{

}

void AnalogSensor::SetPin(uint16_t pin)
{
  Sensor::SetPin(pin);

#if defined(ESP32)
  if (!GetADCChannel(pin, m_isADC1, m_channel))
    return;

  if (m_isADC1)
  {
    ESP_ERROR_CHECK_WITHOUT_ABORT(adc1_config_width(ADC_WIDTH_BIT_12));
    ESP_ERROR_CHECK_WITHOUT_ABORT(adc1_config_channel_atten((adc1_channel_t)m_channel, ADC_ATTEN_11db));
  }
  else
  {
    ESP_ERROR_CHECK_WITHOUT_ABORT(adc2_config_channel_atten((adc2_channel_t)m_channel, ADC_ATTEN_11db));
  }

/*
  gpio_num_t gpio = (gpio_num_t)pin; 
  pinMode(GetPin(), INPUT_PULLDOWN);   //pull down important so that sensor can be detected reliably
  esp_err_t ret;
  ret = gpio_pulldown_en(gpio);
  LOG_ON_ERROR(ret, "gpio_pulldown_en:%s\n\r", esp_err_to_name(ret));

  //using rtc pulldown because on some pins, GPIO pull down is not working because of a silicon bug
  ret = rtc_gpio_init(gpio);
  LOG_ON_ERROR(ret, "rtc_gpio_init:%s\n\r", esp_err_to_name(ret));
  ret = rtc_gpio_set_direction(gpio, RTC_GPIO_MODE_INPUT_ONLY);
  LOG_ON_ERROR(ret, "rtc_gpio_set_direction:%s\n\r", esp_err_to_name(ret));
  ret = rtc_gpio_pullup_dis(gpio);
  LOG_ON_ERROR(ret, "rtc_gpio_pullup_dis:%s\n\r", esp_err_to_name(ret));
  ret = rtc_gpio_pulldown_en(gpio);
  LOG_ON_ERROR(ret, "rtc_gpio_pulldown_en:%s\n\r", esp_err_to_name(ret));  //pull down important so that sensor can be detected reliably
  */
#endif

  Update();
}

void AnalogSensor::Update()
{

}

uint32_t AnalogSensor::GetRawValue() const
{
  int32_t pin = 0;
 
#if defined(ESP32)
  if (m_isADC1)
    pin = adc1_get_raw((adc1_channel_t)m_channel);
  else
    adc2_get_raw((adc2_channel_t)m_channel, ADC_WIDTH_BIT_12, &pin);
#endif

  return pin;
}

uint32_t AnalogSensor::GetValue() const
{
  uint32_t value = 0;
#if defined(ESP32)
  auto readPin = [&]()
  {
    int32_t pin = 0;
    if (m_isADC1)
      pin = adc1_get_raw((adc1_channel_t)m_channel);
    else
      adc2_get_raw((adc2_channel_t)m_channel, ADC_WIDTH_BIT_12, &pin);

    return pin;
  };

  //readPin();
  //delay(10);

  for (int i = 0 ; i < m_reads ; ++i)
  {
    value += readPin();
    //delay(5);
  }
#endif
  return value / m_reads + m_offset;
}

uint32_t AnalogSensor::GetDetailedRawValue(uint32_t *userMin, uint32_t *userMax) const
{
  
  uint32_t value = 0;
  uint32_t min = 0xffffffff;
  uint32_t max = 0;
  for (int i = 0 ; i < m_reads ; ++i)
  {
    uint32_t v = GetRawValue();
    value += v;
    min = std::min(min, v);
    max = std::max(max, v);
  }
  if (userMin)
    *userMin = min;
  
  if (userMax)
    *userMax = max;
     
  return value / m_reads;
}

void AnalogSensor::SetOffset(uint32_t offset)
{
  m_offset = offset;
}

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
VoltageSensor::VoltageSensor()
: AnalogSensor(1)
{

}


void VoltageSensor::SetPin(uint16_t pin)
{
  AnalogSensor::SetPin(pin);
  
  /*
  adc1_channel_t adc1Channel = (adc1_channel_t)GetChannel(pin);
  if (adc1Channel == ADC1_CHANNEL_MAX)
  {
    SER("VoltageSensor:ERROR:adc channel not found\n\r");
    return;
  }

  SER("VoltageSensor:adc channel:%d\n\r", (uint32_t)adc1Channel);
  
  ESP_ERROR_CHECK_WITHOUT_ABORT(adc1_config_width(ADC_WIDTH_BIT_12));
  ESP_ERROR_CHECK_WITHOUT_ABORT(adc1_config_channel_atten(adc1Channel, ADC_ATTEN_11db));
*/
  //this increases power consumption by 22 uAmps while in deep sleep
  //adc_ll_vref_output can be used to lower it to 4 uAmps
  //Have not found a way to cancel it completely
  //ESP_ERROR_CHECK_WITHOUT_ABORT(adc2_vref_to_gpio(GPIO_NUM_27));
}

void VoltageSensor::Update()
{

}

//extern "C" int rom_phy_get_vdd33(); 

uint32_t VoltageSensor::GetValue() const
{  
  //esp_adc_cal_characteristics_t adc_chars;
  //esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_11db, ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_chars);

  int vRaw = AnalogSensor::GetValue();
  
  int vRefRaw = 0;//analogRead(GPIO_NUM_27);

  //the value returned doesn't match anything
  //int phy33 = rom_phy_get_vdd33();

  //also look at __analogReadMilliVolts in esp32-hal-adc.c

  int32_t voltage = 0;//esp_adc_cal_raw_to_voltage(vRaw, &adc_chars);

  LOG("Volt raw:%d ref:%d cal:%d\n\r", vRaw, vRefRaw, voltage);
  
  return vRaw;
}

#if defined(ESP32)
extern "C" uint32_t temprature_sens_read();
#endif

void BuiltInTemperatureSensor::SetPin(uint16_t pin)
{

}
void BuiltInTemperatureSensor::Update()
{

}
uint32_t BuiltInTemperatureSensor::GetValue() const
{
#if defined(ESP32) || defined(ESP_PLATFORM)
  return temprature_sens_read(); 
#elif defined(NRF)
  int32_t temp = 0;
  uint32_t ret = sd_temp_get(&temp);  //Die temperature in 0.25 degrees celsius.
  if (ret != NRF_SUCCESS)
    return -1;

  temp = temp * 10 / 4;   //convert from 0.25 increments to 0.1 increments

  return temp;
#endif
}


const int DATA_SIZE = 10; // 10byte data (2byte version + 8byte tag)
const int DATA_VERSION_SIZE = 2; // 2byte version (actual meaning of these two bytes may vary)
const int DATA_TAG_SIZE = 8; // 8byte tag
const int CHECKSUM_SIZE = 2; // 2byte checksum


RFIDReader::RFIDReader(uint32_t retention)
: m_retention(retention)
{
}

void RFIDReader::Init(uint16_t rx, uint16_t tx)
{
#ifdef ESP32
  m_comm.begin(9600, SWSERIAL_8N1, rx, tx);
  m_comm.listen(); 
#endif
}

void RFIDReader::Update()
{
  Read();

  uint32_t m = GetElapsedMillis();
  if ((m_timeStamp + m_retention) < m)
  {
    uint32_t const tag = 0;
    SetTag(tag);
  }
}

uint32_t RFIDReader::GetValue() const
{
  return m_tag;
}

void RFIDReader::OnChange(TCallback cb)
{
  m_onChange = cb;
}

void RFIDReader::EnableDebug(bool enable)
{
  m_debug = enable;
}

long hexstr_to_value(unsigned char *str, unsigned int length) { // converts a hexadecimal value (encoded as ASCII string) to a numeric value
  char* copy = (char*)malloc((sizeof(char) * length) + 1); 
  memcpy(copy, str, sizeof(char) * length);
  copy[length] = '\0'; 
  // the variable "copy" is a copy of the parameter "str". "copy" has an additional '\0' element to make sure that "str" is null-terminated.
  long value = strtol(copy, NULL, 16);  // strtol converts a null-terminated string to a long value
  free(copy); // clean up 
  return value;
}

void RFIDReader::ExtractTag() 
{
    uint8_t msg_head = m_buffer[0];
    uint8_t *msg_data = m_buffer + 1; // 10 byte => data contains 2byte version + 8byte tag
    uint8_t *msg_data_version = msg_data;
    uint8_t *msg_data_tag = msg_data + 2;
    uint8_t *msg_checksum = m_buffer + 11; // 2 byte
    uint8_t msg_tail = m_buffer[13];

    uint32_t tag = hexstr_to_value(msg_data_tag, DATA_TAG_SIZE);

    long checksum = 0;
    for (int i = 0; i < DATA_SIZE; i+= CHECKSUM_SIZE) {
      long val = hexstr_to_value(msg_data + i, CHECKSUM_SIZE);
      checksum ^= val;
    }
    bool checksumOK = hexstr_to_value(msg_checksum, CHECKSUM_SIZE);


    // print message that was sent from RDM630/RDM6300
    if (m_debug)
    {
      SER("--------\n");
      SER("Message-Head: %d\n", msg_head);

      SER("Message-Data (HEX): ");
      for (int i = 0; i < DATA_VERSION_SIZE; ++i) {
        SER("%d", char(msg_data_version[i]));
      }
      SER(" (version)\n");
      for (int i = 0; i < DATA_TAG_SIZE; ++i) {
        SER("%d", char(msg_data_tag[i]));
      }
      SER(" (tag)\n");

      SER("Message-Checksum (HEX): ");
      for (int i = 0; i < CHECKSUM_SIZE; ++i) {
        SER("%d",char(msg_checksum[i]));
      }
      SER("\n");
      SER("Message-Tail: %d\n", msg_tail);
      SER("--\n");
      SER("Extracted Tag: %d\n", tag);

      
      SER("Extracted Checksum (HEX): 0x%x (%s)\n", checksum, checksumOK ? "OK" : "NOT OK");
      SER("--------\n");
    }

    if (checksumOK)
    {
      SetTag(tag);
    }
}

void RFIDReader::SetTag(uint32_t tag)
{
  if (tag != 0)
  {
    SER("RFIDReader read:0x%x\n", tag);
  }
  
  if (m_tag != tag && m_onChange)
    m_onChange(*this, tag);

  m_timeStamp = GetElapsedMillis();
  m_tag = tag;
}

void RFIDReader::Read()
{
#ifdef ESP32
  if (m_comm.available() > 0){
    
    bool call_extract_tag = false;
    
    int ssvalue = m_comm.read(); // read 
    if (ssvalue == -1) { // no data was read
      return;
    }

    if (ssvalue == 2) { // RDM630/RDM6300 found a tag => tag incoming 
      m_bufferIndex = 0;
    } else if (ssvalue == 3) { // tag has been fully transmitted       
      call_extract_tag = true; // extract tag at the end of the function call
    }

    if (m_bufferIndex >= BUFFER_SIZE) { // checking for a buffer overflow (It's very unlikely that an buffer overflow comes up!)
      SER("Error: RFIDReader buffer overflow detected!\n");
      return;
    }
    
    m_buffer[m_bufferIndex++] = ssvalue; // everything is alright => copy current value to buffer

    if (call_extract_tag == true) {
      if (m_bufferIndex == BUFFER_SIZE) {
        ExtractTag();
      } else { // something is wrong... start again looking for preamble (value: 2)
        m_bufferIndex = 0;
        return;
      }
    }    
  }    
#endif
}