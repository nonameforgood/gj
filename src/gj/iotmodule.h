#pragma once


struct ModulePinSetup
{
  uint32_t m_sensor;
  uint32_t voltageEnablePin;
  uint32_t voltageReadPin;
};


struct IOTModule
{
  uint8_t m_mac[6];
  String m_id;
  String m_description;
  ModulePinSetup m_pinSetup;
  bool m_enableOTA;
};