#ifndef qwt3d_parametricsurface_h__2004_03_05_23_43_begin_guarded_code
#define qwt3d_parametricsurface_h__2004_03_05_23_43_begin_guarded_code

#include "qwt3d_gridmapping.h"

namespace Qwt3D
{

class SurfacePlot;


//! Abstract base class for parametric surfaces
/**

*/
class QWT3D_EXPORT ParametricSurface : public GridMapping
{

public:
  ParametricSurface(); //!< Constructs ParametricSurface object w/o assigned SurfacePlot.
  //! Constructs ParametricSurface object and assigns a SurfacePlot
  explicit ParametricSurface(Qwt3D::SurfacePlot& plotWidget); 
  //! Constructs ParametricSurface object and assigns a SurfacePlot
  explicit ParametricSurface(Qwt3D::SurfacePlot* plotWidget); 
  //! Overwrite this
  virtual Qwt3D::Triple operator()(double u, double v) = 0; 
	//! Assigns a new SurfacePlot and creates a data representation for it.
	virtual bool create(Qwt3D::SurfacePlot& plotWidget);
	//! Creates data representation for the actual assigned SurfacePlot.
	virtual bool create();
  //! Assigns the object to another widget. To see the changes, you have to call this function before create().
  void assign(Qwt3D::SurfacePlot& plotWidget);
  //! Assigns the object to another widget. To see the changes, you have to call this function before create().
  void assign(Qwt3D::SurfacePlot* plotWidget);
  //! Provide information about periodicity of the 'u' resp. 'v' domains.
  void setPeriodic(bool u, bool v); 

private:
  bool uperiodic_, vperiodic_;
};

} // ns

#endif /* include guarded */
