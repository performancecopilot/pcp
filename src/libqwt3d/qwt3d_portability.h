#ifndef qwt3d_portability_h__2005_07_02_11_55_begin_guarded_code
#define qwt3d_portability_h__2005_07_02_11_55_begin_guarded_code

//! Portability classes providing transparent Qt3/4 support

#include <qnamespace.h>
#include "qwt3d_global.h"

#if QT_VERSION < 0x040000

namespace Qwt3D
{
  #define QWT3DLOCAL8BIT(qstring) \
  ((const char*)(qstring.local8Bit()))

  typedef int MouseState;
  typedef int KeyboardState;
  const Qt::TextFlags SingleLine = Qt::SingleLine;
} // ns


#else // Qt4

#include <QMouseEvent>

namespace Qwt3D
{

  #define QWT3DLOCAL8BIT(qstring) \
  ((const char*)(qstring.toLocal8Bit()))
  
  const Qt::TextFlag SingleLine = Qt::TextSingleLine;
  
  //! This class creates a (mouse-button,modifier) pair (ordinary typedef for int if Qt3 is used)
  class MouseState
  {
  public:
    MouseState(Qt::MouseButtons mb = Qt::NoButton, Qt::KeyboardModifiers km = Qt::NoModifier)
      : mb_(mb), km_(km)
    {
    }

    MouseState(Qt::MouseButton mb, Qt::KeyboardModifiers km = Qt::NoModifier)
      : mb_(mb), km_(km)
    {
    }

    bool operator==(const MouseState& ms)
    {
      return mb_ == ms.mb_ && km_ == ms.km_;
    }

    bool operator!=(const MouseState& ms)
    {
      return !operator==(ms);
    }

  private:
    Qt::MouseButtons mb_;
    Qt::KeyboardModifiers km_;
  };
  
  //! This class creates a (key-button,modifier) pair (ordinary typedef for int if Qt3 is used)
  class KeyboardState
  {
  public:
    KeyboardState(int key = Qt::Key_unknown, Qt::KeyboardModifiers km = Qt::NoModifier)
      : key_(key), km_(km)
    {
    }

    bool operator==(const KeyboardState& ms)
    {
      return key_ == ms.key_ && km_ == ms.km_;
    }

    bool operator!=(const KeyboardState& ms)
    {
      return !operator==(ms);
    }

  private:
    int key_;
    Qt::KeyboardModifiers km_;
  };
} // ns

#endif


#endif
