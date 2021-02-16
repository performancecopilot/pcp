/*
htop - SysArchMeter.c
(C) 2021 htop dev team
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/
#include "config.h"  // IWYU pragma: keep

#include "SysArchMeter.h"

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"
#include "XUtils.h"

static const int SysArchMeter_attributes[] = {HOSTNAME};

static void SysArchMeter_updateValues(Meter* this, char* buffer, size_t size) {
   SysArchInfo data;
   Platform_getSysArch(&data);

   snprintf(buffer, size, "%s %s [%s]", data.name, data.release, data.machine);
}

const MeterClass SysArchMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
   },
   .updateValues = SysArchMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 100.0,
   .attributes = SysArchMeter_attributes,
   .name = "System",
   .uiName = "System",
   .caption = "System: ",
};
