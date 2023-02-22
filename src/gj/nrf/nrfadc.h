#if defined(NRF)
#include "../base.h"
#include "../serial.h"

#if defined(NRF51)
    #include "nrf_drv_adc.h"

    #define GJ_ADC_CHANNEL_VDD 0      //ADC_CONFIG_PSEL_Disabled
#elif NRF_MODULE_ENABLED(SAADC)
    #include "nrf_drv_saadc.h"

    #define GJ_ADC_CHANNEL_VDD 9      //SAADC_CH_PSELP_PSELP_VDD
#endif

class Adc
{
public:
  Adc();
  ~Adc();

  struct FinishInfo
  {
    const int16_t *m_values;
    uint32_t m_sampleCount;
    int16_t m_divider;
  };
  typedef std::function<void(const FinishInfo &info)> Function;

  void Init(Function callback);
  void StartSampling(uint32_t analogInput, uint32_t sampleCount);

  static void CreateInstance();
  static Adc* GetInstance();

  static uint16_t GetPinChannel(uint32_t pin);

private:
  uint32_t m_sampleCount = 0;
  Function m_callback = {};
  uint32_t m_eventIndex = 0;

  uint32_t m_sampleCapacity = 0;
  int16_t *m_buffer = nullptr;

  static Adc *ms_instance;

  static void TimerCallback();
  static void CallUserCallback();

#if NRF_MODULE_ENABLED(ADC)
  typedef nrf_drv_adc_evt_t ArgType;
  nrf_drv_adc_channel_t m_channel_config;
  const nrf_drv_adc_evt_type_t m_doneEvent = NRF_DRV_ADC_EVT_DONE;
#elif NRF_MODULE_ENABLED(SAADC)
  typedef nrf_drv_saadc_evt_t ArgType;
  const nrf_drv_saadc_evt_type_t m_doneEvent = NRF_DRV_SAADC_EVT_DONE;
#endif

  static void DriverCallback(ArgType const * p_event);
};

#endif //NRF