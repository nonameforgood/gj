#if defined(NRF)

#include "nrfadc.h"
#include "../commands.h"
#include "../serial.h"
#include "../eventmanager.h"
#include "../config.h"


Adc* Adc::ms_instance = nullptr;

Adc::Adc()
{
} 
Adc::~Adc()
{
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

  m_channel_config = NRF_DRV_ADC_DEFAULT_CHANNEL((uint32_t)analogInput);
  
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
      NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE((nrf_saadc_input_t)(analogInput));
  err_code = nrf_drv_saadc_channel_init(0, &channel_config);
  APP_ERROR_CHECK(err_code);
  }
#endif

  //TimerCallback will start the sampling process
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
  else
  {
    printf("busy\n\r");
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
  int32_t resolution = 1024;
  int32_t gain;
  int32_t ref;

  int16_t *buffer = ms_instance->m_buffer;
  uint32_t sampleCount = ms_instance->m_sampleCount;

#if NRF_MODULE_ENABLED(ADC)
  nrf_drv_adc_channel_disable(&ms_instance->m_channel_config);

  if (ms_instance->m_channel_config.config.config.resolution == NRF_ADC_CONFIG_RES_8BIT)
    resolution = 256;
  else if (ms_instance->m_channel_config.config.config.resolution == NRF_ADC_CONFIG_RES_9BIT)
    resolution = 512;
  else
    resolution = 1024;

  gain = 3;
  ref = 120; //VBG Ref (1.2V) * 100

#elif NRF_MODULE_ENABLED(SAADC)
  ret_code_t err_code;
  err_code = nrf_drv_saadc_channel_uninit(0);
  APP_ERROR_CHECK(err_code);

  gain = 6;
  ref = 60;

  //the last sample value is the closest one to the real value
  buffer = ms_instance->m_buffer + ms_instance->m_sampleCount - 1;
  sampleCount = 1;
#endif

  int32_t divider = resolution * 100 / (gain * ref);

  FinishInfo info = {buffer, sampleCount, (int16_t)divider};

  ms_instance->m_callback(info);
}

uint16_t Adc::GetPinChannel(uint32_t pin)
{
#if defined(NRF51)
  const uint32_t channelPins[] = {
    255, ADC_CONFIG_PSEL_Disabled,      //VDD
    26,  ADC_CONFIG_PSEL_AnalogInput0,  //AIN0
    27,  ADC_CONFIG_PSEL_AnalogInput1,  //AIN1
    1,   ADC_CONFIG_PSEL_AnalogInput2,  //AIN2
    2,   ADC_CONFIG_PSEL_AnalogInput3,  //AIN3
    3,   ADC_CONFIG_PSEL_AnalogInput4,  //AIN4
    4,   ADC_CONFIG_PSEL_AnalogInput5,  //AIN5
    5,   ADC_CONFIG_PSEL_AnalogInput6,  //AIN6
    6,   ADC_CONFIG_PSEL_AnalogInput7,  //AIN7
  }; 
#elif defined(NRF52)
  const uint32_t channelPins[] = {
    2,   SAADC_CH_PSELP_PSELP_AnalogInput0, //AIN0
    3,   SAADC_CH_PSELP_PSELP_AnalogInput1, //AIN1
    4,   SAADC_CH_PSELP_PSELP_AnalogInput2, //AIN2
    5,   SAADC_CH_PSELP_PSELP_AnalogInput3, //AIN3
    28,  SAADC_CH_PSELP_PSELP_AnalogInput4, //AIN4
    29,  SAADC_CH_PSELP_PSELP_AnalogInput5, //AIN5
    30,  SAADC_CH_PSELP_PSELP_AnalogInput6, //AIN6
    31,  SAADC_CH_PSELP_PSELP_AnalogInput7, //AIN7
    255, SAADC_CH_PSELP_PSELP_VDD           //VDD
  }; 
#endif

  constexpr uint32_t elementCount = sizeof(channelPins) / sizeof(channelPins[0]);

  int32_t channel = -1;

  for (int i = 0 ; i < elementCount ; i += 2)
  {
    if (channelPins[i] == pin)
    {
      channel = channelPins[i+1];
      break;
    }
  }

  return channel;
}

#endif