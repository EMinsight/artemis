#include "ParticleContainer.H"

extern "C" {
void geteb3d_energy_conserving(const long* np, 
           const Real* xp, const Real* yp, const Real* zp,
	   const Real* exp, const Real* eyp,const Real* ezp,
	   const Real* bxp, const Real* byp,const Real* bzp,
	   const Real* xmin, const Real* ymin, const Real* zmin,
	   const long* nx, const long* ny, const long* nz,
	   const long* nxguard, const long* nyguard, const long* nzguard,
	   const long* nox, const long* noy, const long* noz,
	   const Real* exg, const Real* eyg, const Real* ezg,
	   const Real* bxg, const Real* byg, const Real* bzg,
	   const bool* ll4symtry, const bool* l_lower_order_in_v); 
}

void
MyParticleContainer::FieldGather(MultiFab& Ex, MultiFab& Ey, MultiFab& Ez,
                                 MultiFab& Bx, MultiFab& By, MultiFab& Bz,
                                 long order)
{
    int             lev         = 0; 
    const Real      strttime    = ParallelDescriptor::second();
    const Geometry& gm          = m_gdb->Geom(lev);
    const BoxArray& ba          = Ex.boxArray();
    const Real*     dx          = gm.CellSize();
    const PMap&     pmap        = m_particles[lev];
    const int       ngrids      = pmap.size();

    //
    // This is a little funky.  What in effect this'll do is force
    // each thread to work on a single (separate) grid at a time.  That
    // way no thread will step on any other.  If there's only one grid per CPU,
    // then oh well ....
    //
    // TODO: implement tiling with OpenMP in this grid loop.
    Array<int>         pgrd(ngrids);
    Array<const PBox*> pbxs(ngrids);

    int j = 0;
    for (typename PMap::const_iterator pmap_it = pmap.begin(), pmapEnd = pmap.end();
         pmap_it != pmapEnd;
         ++pmap_it, ++j)
    {
        pgrd[j] =   pmap_it->first;
        pbxs[j] = &(pmap_it->second);
    }

    // Loop over boxes
    for (int j = 0; j < ngrids; j++)
    {
        const PBox& pbx = *pbxs[j];
	long np = 0;
	Real q = 1.;
	Array<Real>  xp,  yp,  zp, wp;
	Array<Real> exp, eyp, ezp;
	Array<Real> bxp, byp, bzp;

	// 1D Arrays of particle attributes
	 xp.reserve( pbx.size() );
	 yp.reserve( pbx.size() );
	 zp.reserve( pbx.size() );
	 wp.reserve( pbx.size() );
	exp.reserve( pbx.size() );
	eyp.reserve( pbx.size() );
	ezp.reserve( pbx.size() );
	bxp.reserve( pbx.size() );
	byp.reserve( pbx.size() );
	bzp.reserve( pbx.size() );

	// Data on the grid
        FArrayBox& exfab = Ex[pgrd[j]];
        FArrayBox& eyfab = Ey[pgrd[j]];
        FArrayBox& ezfab = Ez[pgrd[j]];
        FArrayBox& bxfab = Bx[pgrd[j]];
        FArrayBox& byfab = By[pgrd[j]];
        FArrayBox& bzfab = Bz[pgrd[j]];

	const Box & bx = ba[pgrd[j]];
	RealBox grid_box = RealBox( bx, dx, gm.ProbLo() );
	const Real* xyzmin = grid_box.lo();
	long nx = bx.length(0)-1, ny = bx.length(1)-1, nz = bx.length(2)-1; 
	long ng = Ex.nGrow();

        Real strt_copy = ParallelDescriptor::second();
	
	// Loop over particles in that box (to change array layout)
        for (typename PBox::const_iterator it = pbx.begin(); it < pbx.end(); ++it)
        {
            const ParticleType& p = *it;
	    
            if (p.m_id <= 0) {
	      continue;
	    }
	    ++np;
	    xp.push_back( p.m_pos[0] );
	    yp.push_back( p.m_pos[1] );
	    zp.push_back( p.m_pos[2] );
 	    wp.push_back( 1. ); 
        }

        Real end_copy = ParallelDescriptor::second() - strt_copy;

        if (ParallelDescriptor::IOProcessor()) 
            std::cout << "Time in FieldGather : Copy " << end_copy << '\n';

        bool ll4symtry          = false;
        bool l_lower_order_in_v = false;

        Real strt_gather = ParallelDescriptor::second();

        geteb3d_energy_conserving(&np, xp.dataPtr(), yp.dataPtr(), zp.dataPtr(),
                                      exp.dataPtr(),eyp.dataPtr(),ezp.dataPtr(),
                                      bxp.dataPtr(),byp.dataPtr(),bzp.dataPtr(),
				      &xyzmin[0], &xyzmin[1], &xyzmin[2],
				      &nx, &ny, &nz, &ng, &ng, &ng, 
				      &order, &order, &order, 
				      exfab.dataPtr(), eyfab.dataPtr(), ezfab.dataPtr(),
				      bxfab.dataPtr(), byfab.dataPtr(), bzfab.dataPtr(),
				      &ll4symtry, &l_lower_order_in_v);

       Real end_gather = ParallelDescriptor::second() - strt_gather;
       if (ParallelDescriptor::IOProcessor()) 
           std::cout << "Time in PicsarFieldGather : Gather " << end_gather << '\n';
    }

    if (m_verbose > 1)
    {
        Real stoptime = ParallelDescriptor::second() - strttime;

        ParallelDescriptor::ReduceRealMax(stoptime,ParallelDescriptor::IOProcessorNumber());

        if (ParallelDescriptor::IOProcessor())
        {
            std::cout << "ParticleContainer<N>::FieldGather time: " << stoptime << '\n';
        }
    }
}