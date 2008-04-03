#ifndef __qwt3d_autoscaler_2003_08_18_12_05__
#define __qwt3d_autoscaler_2003_08_18_12_05__

#include <vector>
#include "qwt3d_global.h"
#include "qwt3d_autoptr.h"

namespace Qwt3D
{

//! ABC for autoscaler
class QWT3D_EXPORT AutoScaler
{
friend class qwt3d_ptr<AutoScaler>;
protected:
  //! Returns a new heap based object of the derived class.  
  virtual AutoScaler* clone() const = 0;
  //! To implement from subclasses
  virtual int execute(double& a, double& b, double start, double stop, int ivals) = 0;
  virtual ~AutoScaler(){}

private:
  void destroy() const {delete this;} //!< Used by qwt3d_ptr      
};

//! Automatic beautifying of linear scales
class QWT3D_EXPORT LinearAutoScaler : public AutoScaler
{
friend class LinearScale;
protected:
  LinearAutoScaler();
  explicit LinearAutoScaler(std::vector<double>& mantisses);
  //! Returns a new heap based object utilized from qwt3d_ptr
  AutoScaler* clone() const {return new LinearAutoScaler(*this);}
	int execute(double& a, double& b, double start, double stop, int ivals);

private:
	
	double start_, stop_;
	int intervals_; 

	void init(double start, double stop, int ivals);
	double anchorvalue(double start, double mantisse, int exponent);
	int segments(int& l_intervals, int& r_intervals, double start, double stop, double anchor, double mantissa, int exponent);
  std::vector<double> mantissi_;
};

} // ns


#endif
