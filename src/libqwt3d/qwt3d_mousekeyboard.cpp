#if defined(_MSC_VER) /* MSVC Compiler */
#pragma warning ( disable : 4305 )
#pragma warning ( disable : 4786 )
#endif

#include "qwt3d_plot.h"

using namespace std;
using namespace Qwt3D;


/**
	Standard mouse button Function. Prepares the call to mouseMoveEvent
	\see mouseMoveEvent()
*/
void Plot3D::mousePressEvent( QMouseEvent *e )
{
	lastMouseMovePosition_ = e->pos();
	mpressed_ = true;
}

/**
	Standard mouse button Function. Completes the call to mouseMoveEvent
	\see mouseMoveEvent()
*/
void Plot3D::mouseReleaseEvent( QMouseEvent* )
{
	mpressed_ = false;
}

/**
	Standard mouse button Function
	\see assignMouse()
*/
void Plot3D::mouseMoveEvent( QMouseEvent *e )
{
	if (!mpressed_ || !mouseEnabled())
  {
    e->ignore();
		return;
  }
	
#if QT_VERSION < 0x040000
	MouseState bstate = e->state();
#else
  MouseState bstate(e->buttons(),e->modifiers());
#endif

	QPoint diff = e->pos() - lastMouseMovePosition_;

	setRotationMouse(bstate, 3, diff);	
	setScaleMouse(bstate, 5, diff);	
	setShiftMouse(bstate, 2, diff);	
		
	lastMouseMovePosition_ = e->pos();
}

void Plot3D::setRotationMouse(MouseState bstate, double accel, QPoint diff)
{
	// Rotation
	double w = max(1,width());
	double h = max(1,height());
		
	double relx = accel*360 * diff.x() / w; 
	double relyz = accel*360 * diff.y() / h; 
	
	double new_xrot = xRotation();
	double new_yrot = yRotation();
	double new_zrot = zRotation();
	
	if ( bstate == xrot_mstate_ )
		new_xrot = round(xRotation() + relyz) % 360; 
	if ( bstate == yrot_mstate_ )
		new_yrot = round(yRotation() + relx) % 360; 
	if ( bstate == zrot_mstate_ )
		new_zrot = round(zRotation() + relx) % 360; 
		
	setRotation(new_xrot, new_yrot, new_zrot); 
}

void Plot3D::setScaleMouse(MouseState bstate, double accel, QPoint diff)
{
	// Scale
		double w = max(1,width());
		double h = max(1,height());

		double relx = diff.x() * accel / w; relx = exp(relx) - 1;
		double relyz = diff.y() * accel / h; relyz = exp(relyz) - 1; 

		double new_xscale = xScale();
		double new_yscale = yScale();
		double new_zscale = zScale();

		if ( bstate == xscale_mstate_)
			new_xscale = max(0.0,xScale() + relx);
		if ( bstate == yscale_mstate_)
			new_yscale = max(0.0,yScale() - relyz);
		if ( bstate == zscale_mstate_)
			new_zscale = max(0.0,zScale() - relyz);

		setScale(new_xscale, new_yscale, new_zscale); 

		if ( bstate == zoom_mstate_)
			setZoom(max(0.0,zoom() - relyz));
}

void Plot3D::setShiftMouse(MouseState bstate, double accel, QPoint diff)
{
	// Shift
	double w = max(1,width());
	double h = max(1,height());

	double relx = diff.x() * accel / w; 
	double relyz = diff.y() * accel / h;

	double new_xshift = xViewportShift();
	double new_yshift = yViewportShift();

	if ( bstate == xshift_mstate_)
		new_xshift = xViewportShift() + relx;
	if ( bstate == yshift_mstate_)
		new_yshift = yViewportShift() - relyz;

	setViewportShift(new_xshift, new_yshift); 
}

/**
	Standard wheel Function - zoom (wheel only) or z-scale (shift+wheel)
*/
void Plot3D::wheelEvent( QWheelEvent *e )
{
	if (!mouseEnabled())
		return;
	
	double accel = 0.05;
	
	double step =  accel * e->delta() / WHEEL_DELTA ;
	step = exp(step)-1;

#if QT_VERSION < 0x040000
	if ( e->state() & Qt::ShiftButton )
#else
	if ( e->modifiers() & Qt::ShiftModifier )
#endif
		setScale(xScale(),yScale(), max(0.0,zScale() + step));
	else
		setZoom(max(0.0,zoom() + step ));
}

/**
	Sets the key/mousebutton combination for data/coordinatesystem moves inside the widget\n\n
	default behaviour:\n

	\verbatim
	rotate around x axis: Qt::LeftButton 
	rotate around y axis: Qt::LeftButton | Qt::ShiftButton
	rotate around z axis: Qt::LeftButton 
	scale x:              Qt::LeftButton | Qt::AltButton 
	scale y:              Qt::LeftButton | Qt::AltButton 
	scale z:              Qt::LeftButton | Qt::AltButton | Qt::ShiftButton
	zoom:                 Qt::LeftButton | Qt::AltButton | Qt::ControlButton
	shifting along x:     Qt::LeftButton | Qt::ControlButton 
	shifting along y:     Qt::LeftButton | Qt::ControlButton
	\endverbatim

	mouseMoveEvent() evaluates this function - if overridden, their usefulness becomes somehow limited
*/
void Plot3D::assignMouse(MouseState xrot, MouseState yrot, MouseState zrot,
											 MouseState xscale, MouseState yscale, MouseState zscale,
											 MouseState zoom, MouseState xshift, MouseState yshift)
{
	xrot_mstate_   = 	xrot;  
  yrot_mstate_   =  yrot;  
  zrot_mstate_   =  zrot;  
  xscale_mstate_ =  xscale;
  yscale_mstate_ =  yscale;
  zscale_mstate_ =  zscale;
  zoom_mstate_   =  zoom;  
  xshift_mstate_ =  xshift;
  yshift_mstate_ =  yshift;
}

/** 
The function has no effect if you derive from Plot3D and overrides the mouse Function too careless.
In this case check first against mouseEnabled() in your version of mouseMoveEvent() and wheelEvent().
A more fine grained input control can be achieved by combining assignMouse() with enableMouse(). 
*/
void Plot3D::enableMouse(bool val) {mouse_input_enabled_ = val;}

/** 
\see enableMouse()
*/
void Plot3D::disableMouse(bool val) {mouse_input_enabled_ = !val;}
bool Plot3D::mouseEnabled() const {return mouse_input_enabled_;}




void Plot3D::keyPressEvent( QKeyEvent *e )
{
	if (!keyboardEnabled())
  {
    e->ignore();
    return;
  }	

#if QT_VERSION < 0x040000
  int bstate = e->state() & Qt::KeyButtonMask; // filter kbd modifier only
  KeyboardState keyseq = bstate + e->key();
#else
  KeyboardState keyseq(e->key(), e->modifiers());
#endif

	setRotationKeyboard(keyseq, kbd_rot_speed_);	
	setScaleKeyboard(keyseq, kbd_scale_speed_);	
	setShiftKeyboard(keyseq, kbd_shift_speed_);	
}

void Plot3D::setRotationKeyboard(KeyboardState kseq, double speed)
{
	// Rotation
	double w = max(1,width());
	double h = max(1,height());
		
	double relx = speed*360 / w; 
	double relyz = speed*360 / h; 
	
	double new_xrot = xRotation();
	double new_yrot = yRotation();
	double new_zrot = zRotation();
	
	if ( kseq == xrot_kstate_[0] )
		new_xrot = round(xRotation() + relyz) % 360; 
	if ( kseq == xrot_kstate_[1] )
		new_xrot = round(xRotation() - relyz) % 360; 
	if ( kseq == yrot_kstate_[0] )
		new_yrot = round(yRotation() + relx) % 360; 
	if ( kseq == yrot_kstate_[1] )
		new_yrot = round(yRotation() - relx) % 360; 
	if ( kseq == zrot_kstate_[0] )
		new_zrot = round(zRotation() + relx) % 360; 
	if ( kseq == zrot_kstate_[1] )
		new_zrot = round(zRotation() - relx) % 360; 
		
	setRotation(new_xrot, new_yrot, new_zrot); 
}

void Plot3D::setScaleKeyboard(KeyboardState kseq, double speed)
{
	// Scale
		double w = max(1,width());
		double h = max(1,height());

		double relx = speed / w; relx = exp(relx) - 1;
		double relyz = speed / h; relyz = exp(relyz) - 1; 

		double new_xscale = xScale();
		double new_yscale = yScale();
		double new_zscale = zScale();

		if ( kseq == xscale_kstate_[0])
			new_xscale = max(0.0,xScale() + relx);
		if ( kseq == xscale_kstate_[1])
			new_xscale = max(0.0,xScale() - relx);
		if ( kseq == yscale_kstate_[0])
			new_yscale = max(0.0,yScale() - relyz);
		if ( kseq == yscale_kstate_[1])
			new_yscale = max(0.0,yScale() + relyz);
		if ( kseq == zscale_kstate_[0])
			new_zscale = max(0.0,zScale() - relyz);
		if ( kseq == zscale_kstate_[1])
			new_zscale = max(0.0,zScale() + relyz);

		setScale(new_xscale, new_yscale, new_zscale); 

		if ( kseq == zoom_kstate_[0])
			setZoom(max(0.0,zoom() - relyz));
		if ( kseq == zoom_kstate_[1])
			setZoom(max(0.0,zoom() + relyz));
}

void Plot3D::setShiftKeyboard(KeyboardState kseq, double speed)
{
	// Shift
	double w = max(1,width());
	double h = max(1,height());

	double relx = speed / w; 
	double relyz = speed / h;

	double new_xshift = xViewportShift();
	double new_yshift = yViewportShift();

	if ( kseq == xshift_kstate_[0])
		new_xshift = xViewportShift() + relx;
	if ( kseq == xshift_kstate_[1])
		new_xshift = xViewportShift() - relx;
	if ( kseq == yshift_kstate_[0])
		new_yshift = yViewportShift() - relyz;
	if ( kseq == yshift_kstate_[1])
		new_yshift = yViewportShift() + relyz;

	setViewportShift(new_xshift, new_yshift); 
}

/**
	Sets the keybutton combination for data/coordinatesystem moves inside the widget\n\n
	default behaviour:\n

	\verbatim
	rotate around x axis: [Key_Down, Key_Up] 
	rotate around y axis: SHIFT+[Key_Right, Key_Left]
	rotate around z axis: [Key_Right, Key_Left] 
	scale x:              ALT+[Key_Right, Key_Left] 
	scale y:              ALT+[Key_Up, Key_Down] 
	scale z:              ALT+SHIFT[Key_Down, Key_Up] 
	zoom:                 ALT+CTRL+[Key_Down, Key_Up]
	shifting along x:     CTRL+[Key_Right, Key_Left] 
	shifting along z:     CTRL+[Key_Down, Key_Up]
	\endverbatim
*/
void Plot3D::assignKeyboard(
       KeyboardState xrot_n, KeyboardState xrot_p
      ,KeyboardState yrot_n, KeyboardState yrot_p
      ,KeyboardState zrot_n, KeyboardState zrot_p
			,KeyboardState xscale_n, KeyboardState xscale_p 
      ,KeyboardState yscale_n, KeyboardState yscale_p
      ,KeyboardState zscale_n, KeyboardState zscale_p
			,KeyboardState zoom_n, KeyboardState zoom_p
      ,KeyboardState xshift_n, KeyboardState xshift_p
      ,KeyboardState yshift_n, KeyboardState yshift_p
      )
{
	xrot_kstate_[0]   =  xrot_n;  
  yrot_kstate_[0]   =  yrot_n;  
  zrot_kstate_[0]   =  zrot_n;  
	xrot_kstate_[1]   =  xrot_p;  
  yrot_kstate_[1]   =  yrot_p;  
  zrot_kstate_[1]   =  zrot_p;  
  
  xscale_kstate_[0] =  xscale_n;
  yscale_kstate_[0] =  yscale_n;
  zscale_kstate_[0] =  zscale_n;
  xscale_kstate_[1] =  xscale_p;
  yscale_kstate_[1] =  yscale_p;
  zscale_kstate_[1] =  zscale_p;

  zoom_kstate_[0]   =  zoom_n;  
  xshift_kstate_[0] =  xshift_n;
  yshift_kstate_[0] =  yshift_n;
  zoom_kstate_[1]   =  zoom_p;  
  xshift_kstate_[1] =  xshift_p;
  yshift_kstate_[1] =  yshift_p;
}

/** 
The function has no effect if you derive from Plot3D and overrides the keyboard Functions too careless.
In this case check first against keyboardEnabled() in your version of keyPressEvent()
A more fine grained input control can be achieved by combining assignKeyboard() with enableKeyboard(). 
*/
void Plot3D::enableKeyboard(bool val) {kbd_input_enabled_ = val;}

/** 
\see enableKeyboard()
*/
void Plot3D::disableKeyboard(bool val) {kbd_input_enabled_ = !val;}
bool Plot3D::keyboardEnabled() const {return kbd_input_enabled_;}

/**
Values < 0 are ignored. Default is (3,5,5)
*/
void Plot3D::setKeySpeed(double rot, double scale, double shift)
{
  if (rot > 0)
    kbd_rot_speed_ = rot;
  if (scale > 0)
    kbd_scale_speed_ = scale;
  if (shift > 0)
    kbd_shift_speed_ = shift;
}

void Plot3D::keySpeed(double& rot, double& scale, double& shift) const
{
  rot = kbd_rot_speed_;
  scale = kbd_scale_speed_;
  shift = kbd_shift_speed_;
}
