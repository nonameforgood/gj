#if defined(NRF)

#include "nrfadc.h"
#include "../commands.h"
#include "../serial.h"
#include "../eventmanager.h"
#include "../config.h"


Adc* Adc::ms_instance = nullptr;

Adc::Adc()
{
  //ms_instance = this;
} 
Adc::~Adc()
{
  //ms_instance = nullptr;
} 

void Adc::CreateInstance()
{
  ms_instance = new Adc;
}
Adc* Adc::GetInstance()
{
  return ms_instance;
}

void Adc::Init(Function callback)
{
  m_callback = callback;

#if NRF_MODULE_ENABLED(ADC)
  ret_code_t ret_code;
  nrf_drv_adc_config_t config = NRF_DRV_ADC_DEFAULT_CONFIG;

  ret_code = nrf_drv_adc_init(&config, DriverCallback);
  APP_ERROR_CHECK(ret_code);
#elif NRF_MODULE_ENABLED(SAADC)
  ret_code_t err_code;
  err_code = nrf_drv_saadc_init(NULL, DriverCallback);
  APP_ERROR_CHECK(err_code);
#endif
}

void Adc::StartSampling(uint32_t analogInput, uint32_t sampleCount)
{
  if (sampleCount > m_sampleCapacity)
  {
    m_sampleCapacity = sampleCount;
    delete [] m_buffer;
    m_buffer = new int16_t[sampleCount];
  }

  m_sampleCount = sampleCount;


#if NRF_MODULE_ENABLED(ADC)

  /*
    P0.26/AIN0
    P0.27/AIN1
    P0.01/AIN2
    P0.02/AIN3
    P0.03/AIN4
    P0.04/AIN5
    P0.05/AIN6
    P0.06/AIN7
  */

  m_channel_config = NRF_DRV_ADC_DEFAULT_CHANNEL((uint32_t)1 << analogInput);
  
  if (analogInput == 0)
    m_channel_config.config.config.input = NRF_ADC_CONFIG_SCALING_SUPPLY_ONE_THIRD;
  else
    m_channel_config.config.config.input = NRF_ADC_CONFIG_SCALING_INPUT_ONE_THIRD;

  m_channel_config.config.config.resolution = ADC_CONFIG_RES_10bit;
  m_channel_config.config.config.reference = NRF_ADC_CONFIG_REF_VBG;

  nrf_drv_adc_channel_enable(&m_channel_config);
#elif NRF_MODULE_ENABLED(SAADC)
  {
    /*
    P0.02/AIN0
    P0.03/AIN1
    P0.04/AIN2
    P0.05/AIN3
    P0.28/AIN4
    P0.29/AIN5
    P0.30/AIN6
    P0.31/AIN7
    */

  ret_code_t err_code;
  nrf_saadc_channel_config_t channel_config =
      NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE((nrf_saadc_input_t)(analogInput + 1));
  err_code = nrf_drv_saadc_channel_init(0, &channel_config);
  APP_ERROR_CHECK(err_code);
  }
#endif

  m_eventIndex = 0;
  GJEventManager->DelayAdd(TimerCallback, 500);
}

void Adc::TimerCallback()
{
#if NRF_MODULE_ENABLED(ADC)
  if (!nrf_adc_is_busy())
  {
    if (ms_instance->m_eventIndex == 0)
    { 
      nrf_drv_adc_buffer_convert(ms_instance->m_buffer,ms_instance->m_sampleCount);
    }
    //number of calls to nrf_drv_adc_sample must match ms_instance->m_sampleCount
    nrf_drv_adc_sample();
    ++ms_instance->m_eventIndex;
  }
#elif NRF_MODULE_ENABLED(SAADC)
  if (!nrf_saadc_busy_check())
  {
    if (ms_instance->m_eventIndex == 0)
    { 
      nrf_drv_saadc_buffer_convert(ms_instance->m_buffer,ms_instance->m_sampleCount);
    }
    //number of calls to nrf_drv_saadc_sample must match ms_instance->m_sampleCount
    nrf_drv_saadc_sample();
    ++ms_instance->m_eventIndex;
  }
#endif

  if (ms_instance->m_eventIndex < ms_instance->m_sampleCount)
    GJEventManager->DelayAdd(TimerCallback, 500);
}

void Adc::DriverCallback(ArgType const * p_event)
{
  if (p_event->type == ms_instance->m_doneEvent)
  {
      GJEventManager->Add(CallUserCallback);
  }
}


void Adc::CallUserCallback()
{
#if NRF_MODULE_ENABLED(ADC)
  nrf_drv_adc_channel_disable(&ms_instance->m_channel_config);
#elif NRF_MODULE_ENABLED(SAADC)
  ret_code_t err_code;
  err_code = nrf_drv_saadc_channel_uninit(0);
  APP_ERROR_CHECK(err_code);
#endif

  int32_t resolution = 1024;
  int32_t gain = 3;
  int32_t ref = 120; //VBG Ref (1.2V) * 100

  if (ms_instance->m_channel_config.config.config.resolution == NRF_ADC_CONFIG_RES_8BIT)
    resolution = 256;
  else if (ms_instance->m_channel_config.config.config.resolution == NRF_ADC_CONFIG_RES_9BIT)
    resolution = 512;
  else
    resolution = 1024;

  int32_t divider = resolution * 100 / (gain * ref);

  FinishInfo info = {ms_instance->m_buffer, ms_instance->m_sampleCount, (int16_t)divider};

  ms_instance->m_callback(info);
}


#if 0

#if NRF_MODULE_ENABLED(ADC)
  #define DEFAULT_ANALOG_BATT 2
#elif NRF_MODULE_ENABLED(SAADC)
  #define DEFAULT_ANALOG_BATT 0
#endif

DEFINE_CONFIG_INT32(batt.powerpin, batt_powerpin, 7);
DEFINE_CONFIG_INT32(batt.adcchan, batt_adcchan, DEFAULT_ANALOG_BATT);


void AdcCallback(const Adc::FinishInfo &info)
{
  int32_t totalVolt = 0;

  for (int i = 0; i < info.m_sampleCount; i++)
  {
    int32_t volt = info.m_values[i] * 100 / info.m_divider * 2;
    totalVolt += volt;
  }

  totalVolt /= info.m_sampleCount;
  SER("volt=%d\r\n", totalVolt);
}


void Command_ReadBattery()
{
  uint32_t pin = GJ_CONF_INT32_VALUE(batt_adcchan);

  s_adc.StartSampling(pin, 5);
}

void Command_Adc(const char *command)
{
  CommandInfo info;
  GetCommandInfo(command, info);

  if (info.m_argCount < 2)
  {
    return;
  }

  uint32_t input = atoi(info.m_args[0].data());
  uint32_t samples = atoi(info.m_args[1].data());

  s_adc.StartSampling(input, samples);
}


DEFINE_COMMAND_NO_ARGS(readbattery, Command_ReadBattery);
DEFINE_COMMAND_ARGS(adc, Command_Adc);

void InitAdc()
{
  s_adc.Init(AdcCallback);

  REFERENCE_COMMAND(readbattery);
  REFERENCE_COMMAND(adc);

  uint32_t powerPin = GJ_CONF_INT32_VALUE(batt_powerpin);

  bool input = false;
  bool pullUp = false;
  SetupPin(powerPin, input, pullUp);
  WritePin(powerPin, 1);
}


#endif

#endif