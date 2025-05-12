/*
htop - ClockMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ClockMeter.h"

#include <time.h>
#include <sys/time.h>

#include "CRT.h"
#include "Machine.h"
#include "Object.h"


static const int ClockMeter_attributes[] = {
   CLOCK
};

static void ClockMeter_updateValues(Meter* this) {
   const Machine* host = this->host;

   struct tm result;
   const struct tm* lt = localtime_r(&host->realtime.tv_sec, &result);
   strftime(this->txtBuffer, sizeof(this->txtBuffer), "%H:%M:%S", lt);
}

const MeterClass ClockMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = ClockMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE) | (1 << LED_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = ClockMeter_attributes,
   .name = "Clock",
   .uiName = "Clock",
   .caption = "Time: ",
};
