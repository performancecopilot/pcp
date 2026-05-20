/*
htop - SELinuxMeter.c
(C) 2020 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/SELinuxMeter.h"

#include "CRT.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>

#include "Object.h"

#include "linux/Compat.h"


static const int SELinuxMeter_attributes[] = {
   METER_TEXT,
};

typedef enum {
   SELINUX_PERMISSIVE,
   SELINUX_ENFORCING,
   SELINUX_UNKNOWN,
   SELINUX_DISABLED,
} EnforcingMode;

static const char* const enforcingText[] = {
   [SELINUX_PERMISSIVE] = "enabled; mode: permissive",
   [SELINUX_ENFORCING]  = "enabled; mode: enforcing",
   [SELINUX_UNKNOWN]    = "enabled; mode: unknown",
   [SELINUX_DISABLED]   = "disabled",
};

static bool hasSELinuxMount(void) {
   struct statfs sfbuf;
   int r = statfs("/sys/fs/selinux", &sfbuf);
   if (r != 0) {
      return false;
   }

   if ((uint32_t)sfbuf.f_type != /* SELINUX_MAGIC */ 0xf97cff8cU) {
      return false;
   }

   struct statvfs vfsbuf;
   r = statvfs("/sys/fs/selinux", &vfsbuf);
   if (r != 0 || (vfsbuf.f_flag & ST_RDONLY)) {
      return false;
   }

   return true;
}

static bool isSelinuxEnabled(void) {
   return hasSELinuxMount();
}

static EnforcingMode getSelinuxEnforcing(void) {
   if (!isSelinuxEnabled())
      return SELINUX_DISABLED;

   char buf[20];
   ssize_t r = Compat_readfile("/sys/fs/selinux/enforce", buf, sizeof(buf));
   if (r < 0)
      return (r == -ENOENT) ? SELINUX_DISABLED : SELINUX_UNKNOWN;

   int enforce = 0;
   if (sscanf(buf, "%d", &enforce) != 1)
      return SELINUX_UNKNOWN;

   return enforce ? SELINUX_ENFORCING : SELINUX_PERMISSIVE;
}

static void SELinuxMeter_updateValues(Meter* this) {
   EnforcingMode enforcing = getSelinuxEnforcing();
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s", enforcingText[enforcing]);
}

const MeterClass SELinuxMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
   },
   .updateValues = SELinuxMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = SELinuxMeter_attributes,
   .name = "SELinux",
   .uiName = "SELinux",
   .description = "SELinux state overview",
   .caption = "SELinux: "
};
