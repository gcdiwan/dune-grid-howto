// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
#include <dune/grid/common/referenceelements.hh>

template<class G, class M, class V>
void evolve (const G& grid, const M& mapper, V& c, double t, double& dt)
{
  // first we extract the dimensions of the grid
  const int dim = G::dimension;
  const int dimworld = G::dimensionworld;

  // type used for coordinates in the grid
  typedef typename G::ctype ct;

  // iterator type
  typedef typename G::template Codim<0>::LeafIterator LeafIterator;

  // intersection iterator type
  typedef typename G::template Codim<0>::LeafIntersectionIterator IntersectionIterator;

  // entity pointer type
  typedef typename G::template Codim<0>::EntityPointer EntityPointer;

  // allocate a temporary vector for the update
  V update(c.size());                                  /*@\label{evh:update}@*/
  for (typename V::size_type i=0; i<c.size(); i++) update[i] = 0;

  // initialize dt very large
  dt = 1E100;

  // compute update vector and optimum dt in one grid traversal
  LeafIterator endit = grid.template leafend<0>();     /*@\label{evh:loop0}@*/
  for (LeafIterator it = grid.template leafbegin<0>(); it!=endit; ++it)
  {
    // cell geometry type
    Dune::GeometryType gt = it->type();

    // cell center in reference element
    const Dune::FieldVector<ct,dim>&
    local = Dune::ReferenceElements<ct,dim>::general(gt).position(0,0);

    // cell center in global coordinates
    Dune::FieldVector<ct,dimworld>
    global = it->geometry().global(local);

    // cell volume, assume linear map here
    double volume = it->geometry().integrationElement(local)
                    *Dune::ReferenceElements<ct,dim>::general(gt).volume();

    // cell index
    int indexi = mapper.map(*it);

    // variable to compute sum of positive factors
    double sumfactor = 0.0;

    // run through all intersections with neighbors and boundary
    IntersectionIterator isend = it->ileafend();       /*@\label{evh:flux0}@*/
    for (IntersectionIterator is = it->ileafbegin(); is!=isend; ++is)
    {
      // get geometry type of face
      Dune::GeometryType gtf = is.intersectionSelfLocal().type();

      // center in face's reference element
      const Dune::FieldVector<ct,dim-1>&
      facelocal = Dune::ReferenceElements<ct,dim-1>::general(gtf).position(0,0);

      // get normal vector scaled with volume
      Dune::FieldVector<ct,dimworld> integrationOuterNormal
        = is.integrationOuterNormal(facelocal);
      integrationOuterNormal
        *= Dune::ReferenceElements<ct,dim-1>::general(gtf).volume();

      // center of face in global coordinates
      Dune::FieldVector<ct,dimworld>
      faceglobal = is.intersectionGlobal().global(facelocal);

      // evaluate velocity at face center
      Dune::FieldVector<double,dim> velocity = u(faceglobal,t);

      // compute factor occuring in flux formula
      double factor = velocity*integrationOuterNormal/volume;

      // for time step calculation
      if (factor>=0) sumfactor += factor;

      // handle interior face
      if (is.neighbor())             // "correct" version /*@\label{evh:neighbor}@*/
      {
        // access neighbor
        EntityPointer outside = is.outside();
        int indexj = mapper.map(*outside);

        // compute flux from one side only
        // this should become easier with the new IntersectionIterator functionality!
        if ( it->level()>outside->level() ||
             (it->level()==outside->level() && indexi<indexj) )
        {
          // compute factor in neighbor
          Dune::GeometryType nbgt = outside->type();
          const Dune::FieldVector<ct,dim>&
          nblocal = Dune::ReferenceElements<ct,dim>::general(nbgt).position(0,0);
          double nbvolume = outside->geometry().integrationElement(nblocal)
                            *Dune::ReferenceElements<ct,dim>::general(nbgt).volume();
          double nbfactor = velocity*integrationOuterNormal/nbvolume;

          if (factor<0)                         // inflow
          {
            update[indexi] -= c[indexj]*factor;
            update[indexj] += c[indexj]*nbfactor;
          }
          else                         // outflow
          {
            update[indexi] -= c[indexi]*factor;
            update[indexj] += c[indexi]*nbfactor;
          }
        }
      }

      // handle boundary face
      if (is.boundary())                               /*@\label{evh:bndry}@*/
      {
        if (factor<0)                   // inflow, apply boundary condition
          update[indexi] -= b(faceglobal,t)*factor;
        else                   // outflow
          update[indexi] -= c[indexi]*factor;
      }
    }             // end all intersections             /*@\label{evh:flux1}@*/

    // compute dt restriction
    dt = std::min(dt,1.0/sumfactor);                   /*@\label{evh:dt}@*/

  }       // end grid traversal                        /*@\label{evh:loop1}@*/

  // scale dt with safety factor
  dt *= 0.99;                                          /*@\label{evh:.99}@*/

  // update the concentration vector
  for (unsigned int i=0; i<c.size(); ++i)
    c[i] += dt*update[i];                              /*@\label{evh:updc}@*/

  return;
}
