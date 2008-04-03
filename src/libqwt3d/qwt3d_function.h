#ifndef qwt3d_function_h__2004_03_05_13_51_begin_guarded_code
#define qwt3d_function_h__2004_03_05_13_51_begin_guarded_code

#include "qwt3d_gridmapping.h"

namespace Qwt3D
{

class SurfacePlot;

//! Abstract base class for mathematical functions
/**
	A Function encapsulates a mathematical function with rectangular domain. The user has to adapt the pure virtual operator() 
	to get a working object. Also, the client code should call setDomain, setMesh and create for reasonable operating conditions.
*/
class QWT3D_EXPORT Function : public GridMapping
{

public:
	
  Function(); //!< Constructs Function object w/o assigned SurfacePlot.
  explicit Function(Qwt3D::SurfacePlot& plotWidget); //!< Constructs Function object and assigns a SurfacePlot
  explicit Function(Qwt3D::SurfacePlot* plotWidget); //!< Constructs Function object and assigns a SurfacePlot
	virtual double operator()(double x, double y) = 0; //!< Overwrite this.
		
	void setMinZ(double val); //!< Sets minimal z value.
	void setMaxZ(double val); //!< Sets maximal z value.

	//! Assigns a new SurfacePlot and creates a data representation for it.
	virtual bool create(Qwt3D::SurfacePlot& plotWidget);
	//! Creates data representation for the actual assigned SurfacePlot.
	virtual bool create();
  //! Assigns the object to another widget. To see the changes, you have to call this function before create().
  void assign(Qwt3D::SurfacePlot& plotWidget); 
  //! Assigns the object to another widget. To see the changes, you have to call this function before create().
  void assign(Qwt3D::SurfacePlot* plotWidget); 
};

} // ns

#endif /* include guarded */
