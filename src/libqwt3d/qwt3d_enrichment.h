#ifndef qwt3d_enrichment_h__2004_02_23_19_24_begin_guarded_code
#define qwt3d_enrichment_h__2004_02_23_19_24_begin_guarded_code

#include "qwt3d_global.h"
#include "qwt3d_types.h"

namespace Qwt3D
{

class Plot3D;


//! Abstract base class for data dependent visible user objects
/**
Enrichments provide a framework for user defined OPenGL objects. The base class has a pure virtuell 
function clone(). 2 additional functions are per default empty and could also get a new implementation
in derived classes. They can be used for initialization issues or actions not depending on the related
primitive. 
*/
class QWT3D_EXPORT Enrichment
{
public:
  enum TYPE{
    VERTEXENRICHMENT,
    EDGEENRICHMENT,
    FACEENRICHMENT,
    VOXELENRICHMENT
  }; //!< Type of the Enrichment - only VERTEXENRICHMENT's are defined at this moment.
  
  Enrichment() : plot(0) {}
  virtual ~Enrichment(){}
  virtual Enrichment* clone() const = 0; //!< The derived class should give back a new Derived(something) here
  virtual void drawBegin(){}; //!< Empty per default. Can be overwritten.
  virtual void drawEnd(){}; //!< Empty per default. Can be overwritten.
  virtual void assign(Plot3D const& pl) {plot = &pl;} //!< Assign to existent plot;
  virtual TYPE type() const = 0; //!< Overwrite 

protected:
  const Plot3D* plot;
};

//! Abstract base class for vertex dependent visible user objects
/**
VertexEnrichments introduce a specialized draw routine for vertex dependent data.
draw() is called, when the Plot realizes its internal OpenGL data representation 
for every Vertex associated to his argument.
*/
class QWT3D_EXPORT VertexEnrichment : public Enrichment
{
public:
  
  VertexEnrichment() : Qwt3D::Enrichment() {}
  virtual Enrichment* clone() const = 0; //!< The derived class should give back a new Derived(something) here
  virtual void draw(Qwt3D::Triple const&) = 0; //!< Overwrite this
  virtual TYPE type() const {return Qwt3D::Enrichment::VERTEXENRICHMENT;} //!< This gives VERTEXENRICHMENT
};

// todo EdgeEnrichment, FaceEnrichment, VoxelEnrichment etc.

} // ns

#endif
