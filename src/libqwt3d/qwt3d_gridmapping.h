#ifndef qwt3d_gridmapping_h__2004_03_06_12_31_begin_guarded_code
#define qwt3d_gridmapping_h__2004_03_06_12_31_begin_guarded_code

#include "qwt3d_mapping.h"

namespace Qwt3D
{

class SurfacePlot;


//! Abstract base class for mappings acting on rectangular grids
/**

*/
class QWT3D_EXPORT GridMapping : public Mapping
{
public:
  GridMapping(); //!< Constructs GridMapping object w/o assigned SurfacePlot.

	void setMesh(unsigned int columns, unsigned int rows); //!< Sets number of rows and columns. 
	void setDomain(double minu, double maxu, double minv, double maxv); //!< Sets u-v domain boundaries.
  void restrictRange(Qwt3D::ParallelEpiped const&); //!< Restrict the mappings range to the parallelepiped 

protected:
  Qwt3D::ParallelEpiped range_p;
  Qwt3D::SurfacePlot* plotwidget_p;
	unsigned int umesh_p, vmesh_p;
	double minu_p, maxu_p, minv_p, maxv_p;
};

} // ns

#endif /* include guarded */
