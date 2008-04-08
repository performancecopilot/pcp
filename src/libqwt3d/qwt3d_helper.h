#ifndef __HELPER_H__
#define __HELPER_H__

#include <math.h>
#include <float.h>
#include <vector>
#include <algorithm>

namespace
{
	inline double Min_(double a, double b)
	{
		return (a<b) ? a : b;
	}
}

namespace Qwt3D
{

inline bool isPracticallyZero(double a, double b = 0)
{
  if (!b)
		return (fabs (a) <=  DBL_MIN);	

	return (fabs (a - b) <= Min_(fabs(a), fabs(b))*DBL_EPSILON);	
}
 
inline int round(double d)
{
	return (d>0) ? int(d+0.5) : int(d-0.5);
}


} //ns

#endif
