// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/pack/interaction_graph/InteractionGraphFactory.cc
/// @brief  Interation graph factory class definition
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)

// Unit headers
#include <core/pack/interaction_graph/InteractionGraphFactory.hh>

// Package headers
#include <utility/graph/Graph.hh>
#include <core/pack/interaction_graph/AnnealableGraphBase.fwd.hh>
#include <core/pack/interaction_graph/InteractionGraphBase.hh>
#include <core/pack/interaction_graph/MultiplexedAnnealableGraph.hh>

#include <core/pack/interaction_graph/PDInteractionGraph.hh>
#include <core/pack/interaction_graph/DensePDInteractionGraph.hh>
#include <core/pack/interaction_graph/DoubleLazyInteractionGraph.hh>
#include <core/pack/interaction_graph/HPatchInteractionGraph.hh>
#include <core/pack/interaction_graph/LazyInteractionGraph.hh>
#include <core/pack/interaction_graph/LinearMemoryInteractionGraph.hh>
#include <core/pack/interaction_graph/NPDHBondInteractionGraph.hh>
#include <core/pack/interaction_graph/SurfaceInteractionGraph.hh>
#include <core/pack/interaction_graph/SymmLinMemInteractionGraph.hh>

#include <core/pack/interaction_graph/ResidueArrayAnnealingEvaluator.hh>

#include <core/pack/rotamer_set/RotamerSets.hh>
#include <core/pack/rotamer_set/RotamerSet.hh>

#include <core/conformation/Residue.hh>

#include <core/pack/task/PackerTask.hh>
#include <core/pack/task/IGEdgeReweightContainer.hh>

#include <core/pose/symmetry/util.hh>

#include <basic/prof.hh>
#include <basic/Tracer.hh>
#include <basic/options/keys/packing.OptionKeys.gen.hh>

#include <utility/pointer/owning_ptr.hh>

#include <core/chemical/ChemicalManager.fwd.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/ScoreFunction.hh>

#ifdef MULTI_THREADED
#include <basic/thread_manager/RosettaThreadManager.hh>
#endif

namespace core {
namespace pack {
namespace interaction_graph {

static basic::Tracer T( "core.pack.interaction_graph.interaction_graph_factory", basic::t_info );

///@brief estimate the amount of work require for precomputation
///@details This may be off by a constant factor, which will be measured using benchmarks
Size
estimate_n_2body_calcs_for_precomputation(
	task::PackerTask const & the_task,
	rotamer_set::RotamerSets const & rotsets,
	utility::graph::Graph const & packer_neighbor_graph
){
	core::Size twobody_count_sum = 0;

	//Count m*n rotamer pair calculations for each pair of residues
	for ( auto iter = packer_neighbor_graph.const_edge_list_begin(),
			end = packer_neighbor_graph.const_edge_list_end();
			iter != end; ++iter ) {
		core::Size const resid1 = (*iter)->get_first_node_ind();
		core::Size const resid2 = (*iter)->get_second_node_ind();

		core::Size const mres1 = rotsets.resid_2_moltenres( resid1 );
		core::Size const mres2 = rotsets.resid_2_moltenres( resid2 );

		// For the sake of this estimation, fixed residues have 1 rotamer
		// This assumption falls apart when both residues are fixed but the penalty of this bad assumption will quickly be drowned out by large m*n products
		core::Size const nrot1 = ( the_task.being_packed( resid1 ) ?
			rotsets.nrotamers_for_moltenres( mres1 ) :
			1 );
		core::Size const nrot2 = ( the_task.being_packed( resid2 ) ?
			rotsets.nrotamers_for_moltenres( mres2 ) :
			1 );

		twobody_count_sum += nrot1 * nrot2;
	}

	return twobody_count_sum;
}

///@brief estimate the amount of work require for on-the-fly calculations
///@details This may be off by a constant factor, which will be measured using benchmarks
Size
estimate_n_2body_calcs_for_linmem_ig(
	rotamer_set::RotamerSets const & rotsets,
	utility::graph::Graph const & packer_neighbor_graph
){
	//For each rotamer, count the number of neighbors it has
	Size nbr_sum = 0;
	for ( Size rot = 1; rot <= rotsets.nrotamers(); ++rot ) {
		Size const resid = rotsets.res_for_rotamer( rot );
		runtime_assert( resid > 0 && resid <= packer_neighbor_graph.num_nodes() );
		nbr_sum += packer_neighbor_graph.get_node( resid )->num_edges();
	}
	return nbr_sum;
}

///@brief This function is called when the user does not specify an IG Type.
///@details The goal is to use the most economical IG type based on the use case. This can get very complicated to predict, but the gist is that the O(N^2) precomputed IG should be used with a small number of rotamers and the O(N) linmem_ig should be used for a large number of rotamers.
///@author Jack Maguire
bool
auto_use_linmem_ig(
	task::PackerTask const & the_task,
	rotamer_set::RotamerSets const & rotsets,
	utility::graph::Graph const & packer_neighbor_graph,
	core::Size const nloop
){
	//This is not the optimal "line in the sand", but it is better than nothing.
	//Feel free to replace this with a more complicated metric if you have the free time to run some benchmarks

	using namespace basic::options;
	//using namespace basic::options::OptionKeys;

	core::Real const iteration_scaling_factor = 1.0 * basic::options::option[ OptionKeys::packing::outeriterations_scaling ]() * option[ OptionKeys::packing::inneriterations_scaling ]();

	//units for expected_linmem_ig_runtime are seconds
	core::Real const expected_linmem_ig_runtime =
		( estimate_n_2body_calcs_for_linmem_ig( rotsets, packer_neighbor_graph ) * nloop * iteration_scaling_factor * 0.000108567 ) + 28.603;

	if ( expected_linmem_ig_runtime < 0 ) {
		//These estimates are noisy for small runtime values.
		//If the value is less than 0, assume that we are so deep into precompute territory that we should not use the linmem_ig
		return false;
	}

	//units for expected_precompute_ig_runtime are seconds
	core::Real expected_precompute_ig_runtime =
		( estimate_n_2body_calcs_for_precomputation( the_task, rotsets, packer_neighbor_graph ) * 9.39307e-07 ) - 11.041;

#ifdef MULTI_THREADED
	expected_precompute_ig_runtime /= std::min( the_task.ig_threads_to_request(), basic::thread_manager::RosettaThreadManager::total_threads() );
#endif

	//The special "unit test" clause
	if ( expected_precompute_ig_runtime < 60 ) {
		//1 minute or less defaults to precopmuted IG
		return false;
		//Again, this is due to these estimates being very noisy for small runtime values
	}

	//Note that there still may be reasons to want the linmem_ig - such as memory usage
	return expected_linmem_ig_runtime < expected_precompute_ig_runtime;
}

InteractionGraphBaseOP
make_linmem_ig(
	task::PackerTask const & the_task,
	rotamer_set::RotamerSets const & rotsets,
	pose::Pose const & pose,
	scoring::ScoreFunction const & sfxn,
	utility::graph::Graph const & packer_neighbor_graph,
	core::Real const surface_weight,
	core::Real const hpatch_weight,
	core::Real const npd_hbond_weight
){
	if ( the_task.ig_threads_to_request() > 1 ) {
		T.Warning << "User requested >1 threads for Interaction Graph computation. The LinearMemoryInteractionGraph does not have multithreading functionality yet so we will proceed in a single-threaded computation." << std::endl;
	}

	// Symmetric OTFIGs are not currently capable of handling either the Surface, HPatch, or NPDHBond scores, so check
	// for symmetry first and return a (pairwise-decomposable) SymmLinearMemoryInteractionGraph if requested.
	if ( pose::symmetry::is_symmetric( pose ) ) {
		T << "Instantiating SymmLinearMemoryInteractionGraph" << std::endl;
		SymmLinearMemoryInteractionGraphOP symlinmemig( new SymmLinearMemoryInteractionGraph( the_task.num_to_be_packed() ) );
		symlinmemig->set_pose( pose );
		symlinmemig->set_score_function( sfxn );
		symlinmemig->set_recent_history_size( the_task.linmem_ig_history_size() );
		return symlinmemig;
	}

	if ( surface_weight ) {
		T << "Instantiating LinearMemorySurfaceInteractionGraph" << std::endl;
		LinearMemorySurfaceInteractionGraphOP lmsolig( new LinearMemorySurfaceInteractionGraph( the_task.num_to_be_packed() ) );
		lmsolig->set_pose( pose );
		lmsolig->set_packer_task( the_task );
		lmsolig->set_score_function( sfxn );
		lmsolig->set_rotamer_sets( rotsets );
		lmsolig->set_surface_score_weight( surface_weight );
		lmsolig->set_recent_history_size( the_task.linmem_ig_history_size() );
		return lmsolig;
	}

	if ( hpatch_weight ) {
		T << "Instantiating LinearMemoryHPatchInteractionGraph" << std::endl;
		LinearMemoryHPatchInteractionGraphOP lmhig( new LinearMemoryHPatchInteractionGraph( the_task.num_to_be_packed() ) );
		lmhig->set_pose( pose );
		lmhig->set_packer_task( the_task );
		lmhig->set_score_function( sfxn );
		lmhig->set_rotamer_sets( rotsets );
		lmhig->set_score_weight( hpatch_weight );
		lmhig->set_recent_history_size( the_task.linmem_ig_history_size() );
		return lmhig;
	}
	if ( npd_hbond_weight ) {
		T << "Instantiating LinearMemoryNPDHBondInteractionGraph" << std::endl;
		LinearMemoryNPDHBondInteractionGraphOP lmig( new LinearMemoryNPDHBondInteractionGraph( the_task.num_to_be_packed() ) );
		lmig->set_pose( pose );
		lmig->set_packer_neighbor_graph( packer_neighbor_graph );
		lmig->set_packer_task( the_task );
		lmig->set_score_function( sfxn );
		lmig->set_rotamer_sets( rotsets );
		lmig->set_recent_history_size( the_task.linmem_ig_history_size() );
		return lmig;
	}

	T << "Instantiating LinearMemoryInteractionGraph" << std::endl;
	LinearMemoryInteractionGraphOP lmig( new LinearMemoryInteractionGraph( the_task.num_to_be_packed() ) );
	lmig->set_pose( pose );
	lmig->set_score_function( sfxn );
	lmig->set_recent_history_size( the_task.linmem_ig_history_size() );
	return lmig;
}

InteractionGraphBaseOP
InteractionGraphFactory::create_interaction_graph(
	task::PackerTask const & the_task,
	rotamer_set::RotamerSets const & rotsets,
	pose::Pose const & pose,
	scoring::ScoreFunction const & sfxn,
	utility::graph::Graph const & packer_neighbor_graph,
	core::Size const nloop
)
{
	core::Real surface_weight( sfxn.get_weight( core::scoring::surface ) );
	core::Real hpatch_weight( sfxn.get_weight( core::scoring::hpatch ) );
	core::Real npd_hbond_weight( std::max( {
		sfxn.get_weight( core::scoring::npd_hbond ),
		sfxn.get_weight( core::scoring::npd_hbond_sr_bb ),
		sfxn.get_weight( core::scoring::npd_hbond_lr_bb ),
		sfxn.get_weight( core::scoring::npd_hbond_bb_sc ),
		sfxn.get_weight( core::scoring::npd_hbond_sr_bb_sc ),
		sfxn.get_weight( core::scoring::npd_hbond_lr_bb_sc ),
		sfxn.get_weight( core::scoring::npd_hbond_sc ),
		sfxn.get_weight( core::scoring::npd_hbond_intra ) } ));

	// don't use the surface or hpatch interaction graphs if we're not designing
	if ( ! the_task.design_any() ) { surface_weight = 0; hpatch_weight = 0; }

	if ( the_task.linmem_ig() ) {
		return make_linmem_ig( the_task, rotsets, pose, sfxn, packer_neighbor_graph, surface_weight, hpatch_weight, npd_hbond_weight );
	} else if ( the_task.design_any() ) {

		if ( rotsets.nmoltenres() >= 1 ) { //we are altering at least one residue
			if ( rotsets.rotamer_set_for_moltenresidue(1)->num_rotamers() >= 1 ) { //and it has at least one rotamer
				if ( rotsets.rotamer_set_for_moltenresidue(1)->rotamer(1)->type().mode() != chemical::CENTROID_t ) { //and it's not centroid repacking
					if ( surface_weight ) { //Note that surface overrides lazy!
						T << "Instantiating PDSurfaceInteractionGraph" << std::endl;
						PDSurfaceInteractionGraphOP pdsig( new PDSurfaceInteractionGraph( the_task.num_to_be_packed() ) );
						pdsig->set_pose( pose );
						pdsig->set_packer_task( the_task );
						pdsig->set_rotamer_sets( rotsets );
						pdsig->set_surface_score_weight( surface_weight );
						return pdsig;

					} else if ( hpatch_weight ) {
						T << "Instantiating PDHPatchInteractionGraph" << std::endl;
						PDHPatchInteractionGraphOP hig( new PDHPatchInteractionGraph( the_task.num_to_be_packed() ) );
						hig->set_pose( pose );
						hig->set_packer_task( the_task );
						hig->set_rotamer_sets( rotsets );
						hig->set_score_weight( hpatch_weight );
						return hig;

					} else if ( npd_hbond_weight != 0 ) {
						T << "Instantiating StandardNPDHBondInteractionGraph" << std::endl;
						StandardNPDHBondInteractionGraphOP ig( new StandardNPDHBondInteractionGraph( the_task.num_to_be_packed() ));
						ig->set_pose( pose );
						ig->set_score_function( sfxn );
						ig->set_packer_neighbor_graph( packer_neighbor_graph );
						ig->set_packer_task( the_task );
						ig->set_rotamer_sets( rotsets );
						return ig;
					} else if ( the_task.lazy_ig() ) {
						T << "Instantiating LazyInteractionGraph" << std::endl;
						LazyInteractionGraphOP lazy_ig( new LazyInteractionGraph( the_task.num_to_be_packed() ) );
						lazy_ig->set_pose( pose );
						lazy_ig->set_score_function( sfxn );
						return lazy_ig;
					} else if ( the_task.double_lazy_ig() ) {
						T << "Instantiating DoubleLazyInteractionGraph" << std::endl;
						DoubleLazyInteractionGraphOP double_lazy_ig( new DoubleLazyInteractionGraph( the_task.num_to_be_packed() ) );
						double_lazy_ig->set_pose( pose );
						double_lazy_ig->set_score_function( sfxn );
						//T << "Setting DoubleLazyIngeractionGraph memory limit to " << the_task.double_lazy_ig_memlimit()  << std::endl;
						double_lazy_ig->set_memory_max_for_rpes( the_task.double_lazy_ig_memlimit() );
						return double_lazy_ig;
					} else if ( the_task.precompute_ig() ) {
						T << "Instantiating PDInteractionGraph" << std::endl;
						return utility::pointer::make_shared< PDInteractionGraph >( the_task.num_to_be_packed() );
					} else {
						//Fall back to auto detection
						if ( auto_use_linmem_ig( the_task, rotsets, packer_neighbor_graph, nloop ) ) {
							return make_linmem_ig( the_task, rotsets, pose, sfxn, packer_neighbor_graph, surface_weight, hpatch_weight, npd_hbond_weight );
						} else {
							T << "Instantiating PDInteractionGraph" << std::endl;
							return utility::pointer::make_shared< PDInteractionGraph >( the_task.num_to_be_packed() );
						}
					}
				}
			}
		}
	} else if ( npd_hbond_weight ) {
		T << "Instantiating DenseNPDHBondInteractionGraph" << std::endl;

		DenseNPDHBondInteractionGraphOP ig( new DenseNPDHBondInteractionGraph( the_task.num_to_be_packed() ));
		ig->set_pose( pose );
		ig->set_score_function( sfxn );
		ig->set_packer_neighbor_graph( packer_neighbor_graph );
		ig->set_packer_task( the_task );
		ig->set_rotamer_sets( rotsets );
		return ig;

	}

	// either of the two below
	// 'linmem_ig flag is off and design is not being performed', or 'linmem_ig flag is off and centroid mode design is being performed'
	//This will also trigger if there are no rotamers

	T << "Instantiating DensePDInteractionGraph" << std::endl;
	return utility::pointer::make_shared< DensePDInteractionGraph >( the_task.num_to_be_packed() );

}

/// @brief Create and initialize two-body interaction graph for the given
/// pose, rotamer sets, packer task and score function.
///
/// Call only valid after:
/// Pose has been scored by scorefxn.
/// Pose residue neighbors updated.
/// ScoreFunction setup for packing.
/// Rotamer sets built.
/// @note In multi-threaded builds, this function takes an extra parameter: a number
/// of threads to request, for multi-threaded construction of the interaction graph.
InteractionGraphBaseOP
InteractionGraphFactory::create_and_initialize_two_body_interaction_graph(
	task::PackerTask const & packer_task,
	rotamer_set::RotamerSets & rotsets,
	pose::Pose const & pose,
	scoring::ScoreFunction const & scfxn,
	utility::graph::GraphCOP packer_neighbor_graph,
	core::Size const nloop
) {
	InteractionGraphBaseOP ig = create_interaction_graph( packer_task, rotsets, pose, scfxn, *packer_neighbor_graph, nloop );

	PROF_START( basic::GET_ENERGIES );
	rotsets.compute_energies( pose, scfxn, packer_neighbor_graph, ig, packer_task.ig_threads_to_request() );
	PROF_STOP( basic::GET_ENERGIES );
	unsigned int mem_usage( ig->getTotalMemoryUsage() );
	T.Debug << "IG: " << mem_usage << " bytes" << std::endl;
	if ( mem_usage > 256*1024*1024 ) { // 256 MB - threshold picked somewhat arbitrarily
		T << "High IG memory usage (>256 MB). If this becomes an issue, consider using a different interaction graph type." << std::endl;
	}

	setup_IG_res_res_weights(pose, packer_task, rotsets, *ig);

	return ig;
}

/// @brief Create and initialize annealable graph for the given
/// pose, rotamer sets, packer task and score function. Initalizes
/// two-body interaction graph, as well as other annealable graphs
/// sepecified by the given score function and task.
/// @details
/// Call only valid after:
/// Pose has been scored by scorefxn.
/// Pose residue neighbors updated.
/// ScoreFunction setup for packing.
/// Rotamer sets built.
/// @note Pose is nonconst as there may still be data to cache in
/// the pose at this point.  Also note that in multi-threaded builds,
/// this function takes an extra parameter: a number of threads to
/// request, for multi-threaded construction of the interaction graph.
AnnealableGraphBaseOP
InteractionGraphFactory::create_and_initialize_annealing_graph(
	task::PackerTask const & packer_task,
	rotamer_set::RotamerSets & rotsets,
	pose::Pose & pose,
	scoring::ScoreFunction const & scfxn,
	utility::graph::GraphCOP packer_neighbor_graph,
	core::Size const nloop
) {
	//Clear cached information in the pose that the ResidueArrayAnnealableEneriges use:
	clear_cached_residuearrayannealableenergy_information(pose);

	std::list< AnnealableGraphBaseOP > annealing_graphs;
	annealing_graphs.push_back(create_and_initialize_two_body_interaction_graph(packer_task, rotsets, pose, scfxn, packer_neighbor_graph,nloop));

	//TODO alexford This resolves target methods via dynamic cast of all activated WholeStructureMethods
	// in the score function. This should likely be replaced with some sort of registration mechanism.
	ResidueArrayAnnealingEvaluatorOP residue_array_evaluator( new ResidueArrayAnnealingEvaluator());
	residue_array_evaluator->initialize( scfxn, pose, rotsets, packer_neighbor_graph );
	if ( residue_array_evaluator->has_methods() ) {
		annealing_graphs.push_back( residue_array_evaluator );
	}

	if ( annealing_graphs.size() == 1 ) {
		return annealing_graphs.front();
	} else {
		return utility::pointer::make_shared< MultiplexedAnnealableGraph >(annealing_graphs);
	}
}

/// @brief Clear specific types of cached information in the pose that the ResidueArrayAnnealableEneriges use.
/// @author Vikram K. Mulligan (vmullig@uw.edu).
void
InteractionGraphFactory::clear_cached_residuearrayannealableenergy_information(
	core::pose::Pose &pose
) {
	if ( pose.energies().data().has( core::scoring::EnergiesCacheableDataType::BURIED_UNSAT_HBOND_GRAPH ) ) {
		T << "Clearing cached buried unsaturated hbond graph from the pose." << std::endl;
		pose.energies().data().clear( core::scoring::EnergiesCacheableDataType::BURIED_UNSAT_HBOND_GRAPH );
	}
}

void
InteractionGraphFactory::setup_IG_res_res_weights(
	pose::Pose const & pose,
	task::PackerTask const & task,
	rotamer_set::RotamerSets const & rotsets,
	interaction_graph::InteractionGraphBase & ig
)
{
	task::IGEdgeReweightContainerCOP edge_reweights = task.IGEdgeReweights();

	if ( edge_reweights ) {

		for ( Size ii = 1; ii<= rotsets.nmoltenres(); ++ii ) {
			Size const res1id = rotsets.moltenres_2_resid( ii );

			for ( ig.reset_edge_list_iterator_for_node( ii ); !ig.edge_list_iterator_at_end(); ig.increment_edge_list_iterator() ) {

				interaction_graph::EdgeBase const & edge( ig.get_edge() );
				Size const other_node = edge.get_other_ind( ii );
				if ( other_node < ii ) {
					continue; // only deal with upper edges
				}
				Size const res2id = rotsets.moltenres_2_resid( other_node );
				ig.set_edge_weight( ii, other_node, edge_reweights->res_res_weight( pose, task, res1id, res2id ) );
			}
		}
	}

}//setup_IG_res_res_weights

}
}
}
