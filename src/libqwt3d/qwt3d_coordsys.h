#ifndef __COORDSYS_H__
#define __COORDSYS_H__

#include "qwt3d_axis.h"
#include "qwt3d_colorlegend.h"

namespace Qwt3D
{

//! A coordinate system with different styles (BOX, FRAME)
class QWT3D_EXPORT CoordinateSystem : public Drawable
{

public:
	explicit CoordinateSystem(Qwt3D::Triple blb = Qwt3D::Triple(0,0,0), Qwt3D::Triple ftr = Qwt3D::Triple(0,0,0), Qwt3D::COORDSTYLE = Qwt3D::BOX);
  ~CoordinateSystem();	
	
	void init(Qwt3D::Triple beg = Qwt3D::Triple(0,0,0), Qwt3D::Triple end = Qwt3D::Triple(0,0,0));
	//! Set style for the coordinate system (NOCOORD, FRAME or BOX)
  void setStyle(Qwt3D::COORDSTYLE s,	Qwt3D::AXIS frame_1 = Qwt3D::X1, 
																			Qwt3D::AXIS frame_2 = Qwt3D::Y1, 
																			Qwt3D::AXIS frame_3 = Qwt3D::Z1);
  Qwt3D::COORDSTYLE style() const { return style_;} 	//!< Return style oft the coordinate system 
	void setPosition(Qwt3D::Triple first, Qwt3D::Triple second); //!< first == front_left_bottom, second == back_right_top
	
	void setAxesColor(Qwt3D::RGBA val); //!< Set common color for all axes
	//! Set common font for all axis numberings
  void setNumberFont(QString const& family, int pointSize, int weight = QFont::Normal, bool italic = false);
	//! Set common font for all axis numberings
	void setNumberFont(QFont const& font);
	//! Set common color for all axis numberings
	void setNumberColor(Qwt3D::RGBA val);
  void setStandardScale(); //!< Sets an linear axis with real number items

 	void adjustNumbers(int val); //!< Fine tunes distance between axis numbering and axis body
	void adjustLabels(int val); //!< Fine tunes distance between axis label and axis body

	//! Sets color for the grid lines
  void setGridLinesColor(Qwt3D::RGBA val) {gridlinecolor_ = val;}
	
	//! Set common font for all axis labels
	void setLabelFont(QString const& family, int pointSize, int weight = QFont::Normal, bool italic = false);
	//! Set common font for all axis labels
	void setLabelFont(QFont const& font);
	//! Set common color for all axis labels
	void setLabelColor(Qwt3D::RGBA val);

	//! Set line width for tic marks and axes
	void setLineWidth(double val, double majfac = 0.9, double minfac = 0.5);
	//! Set length for tic marks
	void setTicLength(double major, double minor);

	//! Switch autoscaling of axes
  void setAutoScale(bool val = true);

	Qwt3D::Triple first() const { return first_;}
	Qwt3D::Triple second() const { return second_;}

	void setAutoDecoration(bool val = true) {autodecoration_ = val;}
	bool autoDecoration() const { return autodecoration_;}

	void setLineSmooth(bool val = true) {smooth_ = val;} //!< draw smooth axes
	bool lineSmooth() const {return smooth_;}            //!< smooth axes ? 

	void draw();
	
	//! Defines whether a grid between the major and/or minor tics should be drawn
  void setGridLines(bool majors, bool minors, int sides = Qwt3D::NOSIDEGRID); 
  int grids() const {return sides_;} //!< Returns grids switched on
	
	//! The vector of all12 axes - use them to set axis properties individually.
  std::vector<Axis> axes;


private:
	void destroy();
	
	Qwt3D::Triple first_, second_;
	Qwt3D::COORDSTYLE style_;
	
	Qwt3D::RGBA gridlinecolor_;

	bool smooth_;
	
	void chooseAxes();
	void autoDecorateExposedAxis(Axis& ax, bool left);
  void drawMajorGridLines(); //!< Draws a grid between the major tics on the site
	void drawMinorGridLines(); //!< Draws a grid between the minor tics on the site
  void drawMajorGridLines(Qwt3D::Axis&, Qwt3D::Axis&); //! Helper
  void drawMinorGridLines(Qwt3D::Axis&, Qwt3D::Axis&); //! Helper
  void recalculateAxesTics();

	bool autodecoration_;
	bool majorgridlines_, minorgridlines_;
  int  sides_;
};

} // ns

#endif
