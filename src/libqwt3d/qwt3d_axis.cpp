#include "qwt3d_axis.h"

using namespace Qwt3D;

Axis::Axis()
{
  init();
};

Axis::~Axis()
{
}

Axis::Axis(Triple beg, Triple end)
{
 	init();
	setPosition(beg,end);
}

void Axis::init()
{
	detachAll();

  scale_ = qwt3d_ptr<Scale>(new LinearScale);

  beg_ = Triple(0.0, 0.0, 0.0);  
  end_ = beg_;
	
	majorintervals_ = 0;
	minorintervals_ = 0;
	setMajors(1);	
	setMinors(1);	
	setLimits(0,0);

	setTicOrientation(0.0, 0.0, 0.0);
	setTicLength(0.0, 0.0);
	setColor(0.0, 0.0, 0.0);
	setLineWidth(1.0);
	symtics_ = false;
	drawNumbers_ = false;
	drawLabel_ = false;

	drawTics_ = false;
	autoscale_ = true;
	markerLabel_.clear();
	numberfont_ = QFont("Courier",12);
	setLabelFont(QFont("Courier",14));

  numbercolor_ = RGBA(0,0,0,0);

	setNumberAnchor(Center);

	numbergap_ = 0;
	labelgap_ = 0;
}

void Axis::setPosition(const Triple& beg, const Triple& end)
{
	beg_ = beg;
	end_ = end;
}

void Axis::setMajors(int val)
{
	if (val == majorintervals_)
		return;
	
	majorintervals_ = (val<=0) ? 1 : val; // always >= 1
}

/*!
\see LogScale::setMinors().
*/
void Axis::setMinors(int val)
{
	if (val == minorintervals_)
		return;

	minorintervals_ = (val<=0) ? 1 : val; // always >= 1
}

void Axis::setTicLength(double majorl, double minorl)
{
	lmaj_ = majorl;
	lmin_ = minorl;
}

void Axis::setTicOrientation(double tx, double ty, double tz)
{
	setTicOrientation(Triple(tx,ty,tz));
}

void Axis::setTicOrientation(const Triple& val)
{
	orientation_ = val;
	orientation_.normalize();
}

/**
\param val thickness for axis base line
\param majfac relative thickness for axis major tics (majfac*val)
\param minfac relative thickness for axis minor tics (minfac*val)
*/
void Axis::setLineWidth(double val, double majfac, double minfac)
{
	lineWidth_ = val;
	majLineWidth_ = majfac * lineWidth_;
	minLineWidth_ = minfac * lineWidth_;
}

void Axis::draw()
{
	Drawable::draw();

	saveGLState();

//	GLStateBewarer sb(GL_LINE_SMOOTH, true);
//	glBlendFunc(GL_ONE, GL_ZERO);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4d(color.r,color.g,color.b,color.a);		

	drawBase();
	drawTics();
	drawLabel();	

	restoreGLState();
}

/**
Always use AFTER drawNumbers() ! (Needs length of number string)
*/
void Axis::drawLabel()
{
	if (!drawLabel_)
		return;

  Triple diff = end() - begin();
	Triple center = begin() + diff/2;
	
	Triple bnumber = biggestNumberString(); 
//	double fac = 6*(second()-first()).length() / 100;
	
	switch (scaleNumberAnchor_) 
	{
		case BottomLeft:
		case TopLeft:
		case CenterLeft:
			bnumber.y = 0;
			break;
		case BottomRight:
		case TopRight:
		case CenterRight:
			bnumber.x = -bnumber.x;
			bnumber.y = 0;
			break;
		case TopCenter:
			bnumber.x = 0;
			bnumber.y = -bnumber.y;
			break;
		case BottomCenter:
			bnumber.x = 0;
			break;
		default:
			break;
	}
	
	Triple pos = ViewPort2World(World2ViewPort(center + ticOrientation() * lmaj_) + bnumber);
	setLabelPosition(pos, scaleNumberAnchor_);

	label_.adjust(labelgap_);
	label_.draw();
}

void Axis::drawBase()
{
	setDeviceLineWidth( lineWidth_ );
	glBegin( GL_LINES );
		glVertex3d( beg_.x, beg_.y, beg_.z); 
		glVertex3d( end_.x, end_.y, end_.z);
	glEnd();
}	

bool Axis::prepTicCalculation(Triple& startpoint)
{
  if (isPracticallyZero(start_, stop_))
		return false;

	autostart_ = start_;
	autostop_ = stop_;

 	if (autoScale()) 
  {  
    setMajors(scale_->autoscale(autostart_, autostop_, start_, stop_, majors()));
    if (isPracticallyZero(autostart_, autostop_))
		  return false;
  }
  
  scale_->setLimits(start_,stop_);
  scale_->setMajors(majors());
  scale_->setMinors(minors());
  scale_->setMajorLimits(autostart_,autostop_);
  scale_->calculate();

	Triple normal = (end_ - beg_);
	//normal.normalize();
	Triple beg = beg_ + ((autostart_ - start_) / (stop_ - start_)) * normal;
	Triple end = end_ - ((stop_ - autostop_) / (stop_ - start_))* normal;

	startpoint = end_ - beg_;

	majorpos_.clear();
	minorpos_.clear();

  return true;
}

void Axis::recalculateTics()
{
  Triple runningpoint;
  if (false==prepTicCalculation(runningpoint))
    return;

	unsigned int i;
	
	for (i = 0; i != scale_->majors_p.size(); ++i) 
	{
		double t = (scale_->majors_p[i] - start_) / (stop_-start_);
		majorpos_.push_back(beg_ + t * runningpoint);
	}
	for (i = 0; i != scale_->minors_p.size(); ++i) 
	{
		double t = (scale_->minors_p[i] - start_) / (stop_-start_);
		minorpos_.push_back(beg_ + t * runningpoint);
	}
}

void Axis::drawTics()
{
	Triple runningpoint;
  if (!drawTics_ || false==prepTicCalculation(runningpoint))
		return;
  
	unsigned int i;
  Triple nadir;
	
  markerLabel_.resize(scale_->majors_p.size());
	setDeviceLineWidth(majLineWidth_);
	for (i = 0; i != scale_->majors_p.size(); ++i) 
	{
		double t = (scale_->majors_p[i] - start_) / (stop_-start_);
    nadir = beg_ + t * runningpoint;
    majorpos_.push_back(drawTic(nadir, lmaj_));
		drawTicLabel(nadir + 1.2 * lmaj_ * orientation_, i);
  }
	setDeviceLineWidth(minLineWidth_);
	for (i = 0; i != scale_->minors_p.size(); ++i) 
	{
		double t = (scale_->minors_p[i] - start_) / (stop_-start_);
		nadir = beg_ + t * runningpoint;
    minorpos_.push_back(drawTic(nadir, lmin_));
  }
}

void Axis::drawTicLabel(Triple pos, int mtic)
{
	if (!drawNumbers_ || (mtic < 0))
		return;
	
	markerLabel_[mtic].setFont(numberfont_.family(), numberfont_.pointSize(), numberfont_.weight(), numberfont_.italic());
	markerLabel_[mtic].setColor(numbercolor_);
  markerLabel_[mtic].setString(scale_->ticLabel(mtic));	  
  markerLabel_[mtic].setPosition(pos, scaleNumberAnchor_);
	markerLabel_[mtic].adjust(numbergap_);
	markerLabel_[mtic].draw();
}

Triple Axis::drawTic(Triple nadir, double length)
{
	double ilength = (symtics_) ? -length : 0.0;

	glBegin( GL_LINES );
	glVertex3d( nadir.x  + ilength * orientation_.x,
				      nadir.y  + ilength * orientation_.y,
							nadir.z  + ilength * orientation_.z) ; 
	glVertex3d( nadir.x  + length * orientation_.x,
							nadir.y  + length * orientation_.y,
							nadir.z  + length * orientation_.z);
	glEnd();
	return nadir;
}

void Axis::setNumberFont(QString const& family, int pointSize, int weight, bool italic)
{
	numberfont_ = QFont(family, pointSize, weight, italic );
}

void Axis::setNumberFont(QFont const& font)
{
	numberfont_ = font;
}

void Axis::setNumberColor(RGBA col)
{
	numbercolor_ = col;
}

void Axis::setLabelFont(QString const& family, int pointSize, int weight, bool italic)
{
	labelfont_ = QFont(family, pointSize, weight, italic );
  label_.setFont(family, pointSize, weight, italic);
}

void Axis::setLabelFont(QFont const& font)
{
	setLabelFont(font.family(), font.pointSize(), font.weight(), font.italic());
}

void Axis::setLabelString(QString const& name)
{
	label_.setString(name);
}

/*!
  Sets label position in conjunction with an anchoring strategy
*/
void Axis::setLabelPosition(const Triple& pos,Qwt3D::ANCHOR an)
{
	label_.setPosition(pos, an);
}

//! Sets color for label
void Axis::setLabelColor(RGBA col)
{
	label_.setColor(col);
}

Triple Axis::biggestNumberString()
{
	Triple ret;
	unsigned size = markerLabel_.size();

	double width, height;

	for (unsigned i=0; i!=size; ++i)
	{
		width = fabs( (World2ViewPort(markerLabel_[i].second())-World2ViewPort(markerLabel_[i].first())).x );
		height = fabs( (World2ViewPort(markerLabel_[i].second())-World2ViewPort(markerLabel_[i].first())).y );

		if (width > ret.x)
			ret.x = width + markerLabel_[i].gap();
		if (height > ret.y)
			ret.y = height + markerLabel_[i].gap();;
	}
	return ret;
}

/*! 
  This variant sets a user-defined scale object.
  Use with a heap based initialized pointer only.
  The axis adopts ownership. 
*/
void Axis::setScale(Scale* val)
{
  scale_ = qwt3d_ptr<Scale>(val); 
}

/*!
  Sets one of the predefined scaling types.
  \warning Too small intervals in logarithmic scales lead to  
  empty scales (or perhaps a scale only containing an isolated 
  major tic). Better switch to linear scales in such cases.
*/
void Axis::setScale(Qwt3D::SCALETYPE val)
{
  switch(val) {
  case Qwt3D::LINEARSCALE:
    setScale(new LinearScale);
  	break;
  case Qwt3D::LOG10SCALE:
    setScale(new LogScale);
    setMinors(9);
  	break;
  default:
    break;
  }
}
