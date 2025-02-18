// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file apps/public/cyclic_peptide/simple_cycpep_predict.cc
/// @brief Predicts structure of N-to-C cyclic peptides from amino acid sequence.
/// @details
/// @author Vikram K. Mulligan (vmullig@uw.edu)

#ifdef BOINC
#include <utility/boinc/boinc_util.hh>
#include <protocols/boinc/boinc.hh>
#include "boinc_zip.h"
#endif // BOINC -- keep this first.

// MPI headers
#ifdef USEMPI
#include <mpi.h> //Message Passing Interface for parallelization -- keep this second
#endif

//General includes
#include <devel/init.hh>
#include <utility/excn/Exceptions.hh>
#include <basic/Tracer.hh>
#include <basic/citation_manager/CitationManager.hh>

//Application-specific includes
#ifdef USEMPI
#include <protocols/cyclic_peptide_predict/SimpleCycpepPredictApplication_MPI.hh>
#include <protocols/cyclic_peptide_predict/util.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#endif
#include <protocols/cyclic_peptide_predict/SimpleCycpepPredictApplication.hh>

// for graphics
#include <protocols/viewer/viewers.hh>

//Tracer:
static basic::Tracer TR( "apps.public.cyclic_peptide.simple_cycpep_predict" );

/// @details only called by graphics code, so no MPI consideration.
void*
my_main( void* )
{
	protocols::cyclic_peptide_predict::SimpleCycpepPredictApplication this_app;
	this_app.run();
	protocols::viewer::clear_conformation_viewers();
	// Final citation manager output:
	basic::citation_manager::CitationManager::get_instance()->write_all_citations_and_unpublished_author_info();
	exit( 0 );
}

/*************************/
/* MAIN: */
/*************************/
int
main( int argc, char * argv [] )
{
	try {
		protocols::cyclic_peptide_predict::SimpleCycpepPredictApplication::register_options(); //Has to be before init().
		devel::init(argc, argv);

#ifdef USEMPI
		//Variables for MPI -- only used in MPI mode:
		int MPI_rank(0); //Note -- must be int for MPI
		int MPI_n_procs(0);
		utility::vector1< core::Size > batchsize_per_level;
		//core::Size hierarchy_level(0); //0 for emperor process, 1 for first-level masters, 2 for second-level masters ... total_hierarchy_levels - 1 for slaves/workers.
		core::Size total_hierarchy_levels(0); //Note that for a 3-level hierarchy, this will be 3, but the highest hierarchy_level index will be 2.
		utility::vector1< core::Size > procs_per_hierarchy_level; //The number of processes at each level of the communications hierarchy.  Should be 1, n>=1, m>=n, etc.
		std::string sort_by("");
		bool select_highest(false);
		core::Real output_fraction(1.0);
		std::string output_filename("");
		core::Real lambda( 0.5 );
		core::Real kbt( 1.0 );
		bool compute_rmsd_to_lowest( false );
		core::Real compute_pnear_to_lowest_fract( 0.0 );
		bool compute_sasa_metrics( false );
		core::Size threads_per_slave(1);
		protocols::cyclic_peptide_predict::set_MPI_vars(
			MPI_rank, MPI_n_procs, total_hierarchy_levels, procs_per_hierarchy_level, batchsize_per_level,
			sort_by, select_highest, output_fraction, output_filename, lambda, kbt, compute_rmsd_to_lowest,
			compute_pnear_to_lowest_fract, compute_sasa_metrics, threads_per_slave, "simple_cycpep_predict"
		); //Get the values of these vars (only used in MPI mode).
#endif

		if ( TR.visible() ) {
#ifdef USEMPI
			if( MPI_rank == 0 ) { //If this is the emperor:
				TR << "Starting simple_cycpep_predict.cc, MPI version." << std::endl;
				TR << "Application created 16 September 2015 by Vikram K. Mulligan, Ph.D., Baker laboratory." << std::endl;
				TR << "For questions, contact vmullig@uw.edu." << std::endl << std::endl;
				TR << "Launching " << MPI_n_procs << " parallel processes with " << total_hierarchy_levels << " levels of communication." << std::endl;
				TR << "Batch size is ";
				for(core::Size i=1, imax=batchsize_per_level.size(); i<=imax; ++i) {
					TR << batchsize_per_level[i] << " (level " << i << " to " << i+1 << ")";
					if(i<imax) TR << ", ";
				}
				TR << "." << std::endl;
				TR.flush();
			}
#else
			TR << "Starting simple_cycpep_predict.cc" << std::endl;
			TR << "Application created 16 September 2015 by Vikram K. Mulligan, Ph.D., Baker laboratory." << std::endl;
			TR << "For questions, contact vmullig@uw.edu." << std::endl << std::endl;
			TR.flush();
#endif
		}

#ifdef USEMPI
		protocols::cyclic_peptide_predict::SimpleCycpepPredictApplication_MPI this_app(
			MPI_rank, MPI_n_procs, core::scoring::get_score_function() /*Reads from file once here.*/, total_hierarchy_levels,
			procs_per_hierarchy_level, batchsize_per_level, sort_by, select_highest, output_fraction, output_filename,
			lambda, kbt, compute_rmsd_to_lowest, compute_pnear_to_lowest_fract, compute_sasa_metrics, threads_per_slave
		);
		this_app.run();
#else
#ifdef GL_GRAPHICS
		protocols::viewer::viewer_main( my_main );
#else
		protocols::cyclic_peptide_predict::SimpleCycpepPredictApplication this_app;
		this_app.run();
#endif
#endif

	} catch (utility::excn::Exception& excn ) {
		excn.display();
		return -1;
	}

	basic::citation_manager::CitationManager::get_instance()->write_all_citations_and_unpublished_author_info();

	if ( TR.visible() ) {
		TR << "Finished simple_cycpep_predict.cc.  Exiting." << std::endl;
		TR.flush();
	}

#ifdef USEMPI
	MPI_Barrier(MPI_COMM_WORLD); //Wait until all procs reach this point.
	MPI_Finalize(); //MPI finalization and cleanup.
#endif

	return 0;
}
