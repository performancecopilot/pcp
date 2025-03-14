/*
htop - MetersPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "MetersPanel.h"

#include <stdlib.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Header.h"
#include "ListItem.h"
#include "Meter.h"
#include "Object.h"
#include "ProvideCurses.h"


// Note: In code the meters are known to have bar/text/graph "Modes", but in UI
// we call them "Styles".
static const char* const MetersFunctions[] = {"Style ", "Move  ", "                                       ", "Delete", "Done  ", NULL};
static const char* const MetersKeys[] = {"Space", "Enter", "  ", "Del", "F10"};
static const int MetersEvents[] = {' ', 13, ERR, KEY_DC, KEY_F(10)};

// We avoid UTF-8 arrows ← → here as they might display full-width on Chinese
// terminals, breaking our aligning.
// In <http://unicode.org/reports/tr11/>, arrows (U+2019..U+2199) are
// considered "Ambiguous characters".
static const char* const MetersMovingFunctions[] = {"Style ", "Lock  ", "Up    ", "Down  ", "Left  ", "Right ", "       ", "Delete", "Done  ", NULL};
static const char* const MetersMovingKeys[] = {"Space", "Enter", "Up", "Dn", "<-", "->", "  ", "Del", "F10"};
static const int MetersMovingEvents[] = {' ', 13, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, ERR, KEY_DC, KEY_F(10)};
static FunctionBar* Meters_movingBar = NULL;

void MetersPanel_cleanup(void) {
   if (Meters_movingBar) {
      FunctionBar_delete(Meters_movingBar);
      Meters_movingBar = NULL;
   }
}

static void MetersPanel_delete(Object* object) {
   MetersPanel* this = (MetersPanel*) object;
   Panel_done(&this->super);
   free(this);
}

void MetersPanel_setMoving(MetersPanel* this, bool moving) {
   Panel* super = &this->super;
   this->moving = moving;
   ListItem* selected = (ListItem*)Panel_getSelected(super);
   if (selected) {
      selected->moving = moving;
   }
   if (!moving) {
      Panel_setSelectionColor(super, PANEL_SELECTION_FOCUS);
      Panel_setDefaultBar(super);
   } else {
      Panel_setSelectionColor(super, PANEL_SELECTION_FOLLOW);
      super->currentBar = Meters_movingBar;
   }
}

static inline bool moveToNeighbor(MetersPanel* this, MetersPanel* neighbor, int selected) {
   Panel* super = &this->super;
   if (this->moving) {
      if (neighbor) {
         if (selected < Vector_size(this->meters)) {
            MetersPanel_setMoving(this, false);

            Meter* meter = (Meter*) Vector_take(this->meters, selected);
            Panel_remove(super, selected);
            Vector_insert(neighbor->meters, selected, meter);
            Panel_insert(&(neighbor->super), selected, (Object*) Meter_toListItem(meter, false));
            Panel_setSelected(&(neighbor->super), selected);

            MetersPanel_setMoving(neighbor, true);
            return true;
         }
      }
   }
   return false;
}

static HandlerResult MetersPanel_eventHandler(Panel* super, int ch) {
   MetersPanel* this = (MetersPanel*) super;

   int selected = Panel_getSelectedIndex(super);
   HandlerResult result = IGNORED;
   bool sideMove = false;

   switch (ch) {
      case 0x0a:
      case 0x0d:
      case KEY_ENTER:
         if (!Vector_size(this->meters))
            break;
         MetersPanel_setMoving(this, !(this->moving));
         result = HANDLED;
         break;
      case ' ':
      case KEY_F(4):
      case 't': {
         if (!Vector_size(this->meters))
            break;
         Meter* meter = (Meter*) Vector_get(this->meters, selected);
         MeterModeId mode = Meter_nextSupportedMode(meter);
         Meter_setMode(meter, mode);
         Panel_set(super, selected, (Object*) Meter_toListItem(meter, this->moving));
         result = HANDLED;
         break;
      }
      case KEY_UP:
         if (!this->moving)
            break;
         /* else fallthrough */
      case KEY_F(7):
      case '[':
      case '-':
         Vector_moveUp(this->meters, selected);
         Panel_moveSelectedUp(super);
         result = HANDLED;
         break;
      case KEY_DOWN:
         if (!this->moving)
            break;
         /* else fallthrough */
      case KEY_F(8):
      case ']':
      case '+':
         Vector_moveDown(this->meters, selected);
         Panel_moveSelectedDown(super);
         result = HANDLED;
         break;
      case KEY_RIGHT:
         sideMove = moveToNeighbor(this, this->rightNeighbor, selected);
         if (this->moving && !sideMove) {
            // lock user here until it exits positioning-mode
            result = HANDLED;
         }
         // if user is free, don't set HANDLED;
         // let ScreenManager handle focus.
         break;
      case KEY_LEFT:
         sideMove = moveToNeighbor(this, this->leftNeighbor, selected);
         if (this->moving && !sideMove) {
            result = HANDLED;
         }
         break;
      case KEY_F(9):
      case KEY_DC:
      case KEY_DEL_MAC:
         if (!Vector_size(this->meters))
            break;
         if (selected < Vector_size(this->meters)) {
            Vector_remove(this->meters, selected);
            Panel_remove(super, selected);
         }
         MetersPanel_setMoving(this, false);
         result = HANDLED;
         break;
   }

   if (result == HANDLED || sideMove) {
      Header* header = this->scr->header;
      this->settings->changed = true;
      this->settings->lastUpdate++;
      Header_calculateHeight(header);
      ScreenManager_resize(this->scr);
   }

   return result;
}

const PanelClass MetersPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = MetersPanel_delete
   },
   .eventHandler = MetersPanel_eventHandler
};

MetersPanel* MetersPanel_new(Settings* settings, const char* header, Vector* meters, ScreenManager* scr) {
   MetersPanel* this = AllocThis(MetersPanel);
   Panel* super = &this->super;

   FunctionBar* fuBar = FunctionBar_new(MetersFunctions, MetersKeys, MetersEvents);
   if (!Meters_movingBar) {
      Meters_movingBar = FunctionBar_new(MetersMovingFunctions, MetersMovingKeys, MetersMovingEvents);
   }
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   this->settings = settings;
   this->meters = meters;
   this->scr = scr;
   this->moving = false;
   this->rightNeighbor = NULL;
   this->leftNeighbor = NULL;
   Panel_setHeader(super, header);
   for (int i = 0; i < Vector_size(meters); i++) {
      const Meter* meter = (const Meter*) Vector_get(meters, i);
      Panel_add(super, (Object*) Meter_toListItem(meter, false));
   }
   return this;
}
