#include "report.h"

#ifdef GJ_REPORT

#include "metricsmanager.h"
#include "eventcounter.h"
#include <WebServer.h>
#include <ctime>
#include "datetime.h"


void OutputRawReport(ReportArgs const &args) {
  
  WebServer &server = *args.m_server;
  uint32_t total = 0;

  auto visit = [&total,&server](Chunk const &chunk) -> bool
  {
    char out[64];

    //DBGPRINT( "online visit\n\r" );

    if (chunk.m_format == 0)
    {
      PeriodChunk const *periodChunk = (PeriodChunk*)chunk.m_data.data();
      char date[20];
      ConvertEpoch(periodChunk->m_unixtime, date);

      sprintf(out, "Period chunk:%d(%s)\n", periodChunk->m_unixtime, date);
      server.sendContent_P( out );

      for ( uint16_t i = 0 ; i < PeriodChunk::MaxPeriod ; ++i )
      {
        Period const *p = &periodChunk->m_periods[i];
        sprintf(out, "Period: %d, %d \n", p->m_index, p->m_count );
        server.sendContent_P( out );

        total += p->m_count;
      }
    }

    return true;
  };

  server.sendContent_P("Raw:\n");
  args.m_metricsManager->Visit( visit );
  
  if (args.m_counter->HasData())
  {
    Chunk chunk;
    args.m_counter->GetChunk(chunk);
    visit( chunk );
  }

  char out[64];
  sprintf(out, "Total=%d\n", total);
  server.sendContent_P( out );
}

void OutputReport(ReportArgs const &args) {
  
  WebServer &server = *args.m_server;
  
  uint32_t total = 0;
  uint32_t dayTotal = 0;
  //int32_t epoch = 0;
  std::tm tm = {};

  auto visit = [&]( Chunk const &chunk ) -> bool
  {
    char out[64];

    if (chunk.m_format == 0)
    {
      PeriodChunk const *periodChunk = (PeriodChunk*)chunk.m_data.data();

      for ( uint16_t i = 0 ; i < PeriodChunk::MaxPeriod ; ++i )
      {
        Period const *p = &periodChunk->m_periods[i];

        if (p->m_count)
        {
          char date[20];
          ConvertEpoch(periodChunk->m_unixtime + p->m_index * 60 * 60, date);

          sprintf(out, "%s: %d\n", date, p->m_count );
          server.sendContent_P( out );

          dayTotal += p->m_count;
          total += p->m_count;
        }
      }
    }

    return true;
  };

  args.m_metricsManager->Visit( visit );
  if (args.m_counter->HasData())
  {
    Chunk chunk;
    args.m_counter->GetChunk(chunk);
    visit( chunk );
  }
  
  char out[64];
  sprintf(out, "Total=%d\n", total);
  server.sendContent_P( out );
}

#endif //GJ_REPORT