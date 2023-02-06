#pragma once

//#define GJ_REPORT

#ifdef GJ_REPORT

class MetricsManager;
class WebServer;
class EventCounter;

struct ReportArgs
{
  WebServer *m_server = nullptr;
  MetricsManager *m_metricsManager = nullptr;
  EventCounter *m_counter = nullptr;
};

void OutputRawReport(ReportArgs const &args);
void OutputReport(ReportArgs const &args);


#endif //GJ_REPORT