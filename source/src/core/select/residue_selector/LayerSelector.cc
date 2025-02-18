// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/select/residue_selector/LayerSelector.cc
/// @brief  A ResidueSelector for selecting residues based on layers defined by burial (core, boundary, surface, etc.).
/// @author Vikram K. Mulligan (vmullig@uw.edu)

// Unit headers
#include <core/select/residue_selector/LayerSelector.hh>
#include <core/select/residue_selector/ResidueSelectorCreators.hh>

// Package headers
#include <core/select/residue_selector/util.hh>
#include <core/pose/Pose.hh>
#include <core/select/util/SelectResiduesByLayer.hh>

// Basic Headers
#include <basic/datacache/DataMap.fwd.hh>
#include <basic/citation_manager/CitationCollection.hh>
#include <basic/citation_manager/CitationManager.hh>

// Utility Headers
#include <utility/tag/Tag.hh>
#include <utility/tag/XMLSchemaGeneration.hh>
#include <utility/exit.hh>
#include <utility/pointer/memory.hh>

// C++ headers

#include <basic/Tracer.hh> // AUTO IWYU For Tracer

#ifdef    SERIALIZATION
// Utility serialization headers
#include <utility/vector1.srlz.hh>
#include <utility/serialization/serialization.hh>

// Cereal headers
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/string.hpp>
#endif // SERIALIZATION

static basic::Tracer TR( "core.select.residue_selector.LayerSelector" );

namespace core {
namespace select {
namespace residue_selector {

/// @brief Default constructor
///
LayerSelector::LayerSelector() :
	cache_selection_( false ),
	asu_only_( false ),
	srbl_( utility::pointer::make_shared< core::select::util::SelectResiduesByLayer >() )
	//TODO -- initialize here
{
	set_use_sc_neighbors(true);
	set_cutoffs((use_sc_neighbors() ? 5.2 : 20.0 ),(use_sc_neighbors() ? 2.0 : 40.0 ));
}

/// @brief Copy constructor.
///
LayerSelector::LayerSelector( LayerSelector const &src ) :
	cache_selection_( src.cache_selection_ ),
	asu_only_( src.asu_only_ ),
	srbl_( src.srbl_->clone() ) //CLONE this -- don't copy it.
{}

/// @brief Destructor.
///
LayerSelector::~LayerSelector() = default;

/// @brief Clone operator.
/// @details Copy this object and return an owning pointer to the new object.
ResidueSelectorOP LayerSelector::clone() const {
	return utility::pointer::make_shared< LayerSelector >( *this );
	//return utility::pointer::make_shared< LayerSelector >(*this);
}

/// @brief Apply function: generate a ResidueSubset given a Pose.
///
ResidueSubset
LayerSelector::apply( core::pose::Pose const & pose ) const
{
	core::Size const nres( pose.size() );

	// Return the cached value if requested by the user
	if ( cache_selection_ && srbl_->is_selection_initialized() ) {
		ResidueSubset cached_subset( nres, false );
		const utility::vector1< Size >* layer_selections[3] = { &srbl_->selected_core_residues(), &srbl_->selected_boundary_residues(), &srbl_->selected_surface_residues() };
		for ( auto & layer_selection : layer_selections ) {
			for ( Size i = 1; i <= layer_selection->size(); i++ ) {
				Size residue_index = (*layer_selection)[i];
				runtime_assert(residue_index <= nres);
				cached_subset[residue_index] = true;
			}
		}

		TR << "Returning cached selection result " << std::endl;
		return cached_subset;
	}

	// Compute srbl (SelectResiduesByLayer)
	utility::vector1<core::Size> selected_residues = srbl_->compute(pose, "", true, asu_only_ );

	// Assign selection
	ResidueSubset subset( nres, false );
	for ( core::Size i=1, imax=selected_residues.size(); i<=imax; ++i ) {
		runtime_assert(selected_residues[i] > 0 && selected_residues[i] <= nres);
		subset[selected_residues[i]] = true;
	}

	return subset;
}

/// @brief Parse xml tag setting up this selector.
///
void
LayerSelector::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap &/*data*/)
{
	//Determine which layers we're picking:
	set_layers(
		tag->getOption<bool>("select_core", false),
		tag->getOption<bool>("select_boundary", false),
		tag->getOption<bool>("select_surface", false)
	);

	//Select algorithm:
	set_use_sc_neighbors( tag->getOption<bool>("use_sidechain_neighbors", true) );

	//Symmetry options
	set_asu_only( tag->getOption< bool >( "asu_only", false ) );

	//Set algorithm details:
	if ( tag->hasOption("ball_radius") ) {
		set_ball_radius( tag->getOption<core::Real>("ball_radius") );
	}
	if ( tag->hasOption("sc_neighbor_dist_midpoint") ) {
		set_sc_neighbor_dist_midpoint( tag->getOption<core::Real>("sc_neighbor_dist_midpoint") );
	}
	if ( tag->hasOption("sc_neighbor_denominator") ) {
		set_sc_neighbor_denominator( tag->getOption<core::Real>("sc_neighbor_denominator") );
	}
	if ( tag->hasOption("sc_neighbor_angle_shift_factor") ) {
		set_angle_shift_factor( tag->getOption<core::Real>("sc_neighbor_angle_shift_factor") );
	}
	if ( tag->hasOption("sc_neighbor_angle_exponent") ) {
		set_angle_exponent( tag->getOption<core::Real>("sc_neighbor_angle_exponent") );
	}
	if ( tag->hasOption("sc_neighbor_dist_exponent") ) {
		set_dist_exponent( tag->getOption<core::Real>("sc_neighbor_dist_exponent") );
	}
	set_cutoffs(
		tag->getOption( "core_cutoff", (use_sc_neighbors() ? 5.2 : 20.0 ) ),
		tag->getOption( "surface_cutoff", (use_sc_neighbors() ? 2.0 : 40.0 ) )
	);
	cache_selection_ = tag->getOption<bool>("cache_selection", false);

	return;
}

/// @brief Get the class name.
/// @details Calls class_name().
std::string LayerSelector::get_name() const {
	return LayerSelector::class_name();
}

/// @brief Get the class name.
///
std::string
LayerSelector::class_name() {
	return "Layer";
}

void
LayerSelector::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) {
	using namespace utility::tag;
	AttributeList attributes;
	attributes
		+ XMLSchemaAttribute::attribute_w_default( "select_core",                    xsct_rosetta_bool, "Should the core (buried) layer be selected?",  "false"  )
		+ XMLSchemaAttribute::attribute_w_default( "select_boundary",                xsct_rosetta_bool, "Should the boundary (semi-buried) layer be selected?",  "false"  )
		+ XMLSchemaAttribute::attribute_w_default( "select_surface",                 xsct_rosetta_bool, "Should the surface (exposed) layer be selected?",  "false"  )
		+ XMLSchemaAttribute::attribute_w_default( "cache_selection",                xsct_rosetta_bool, "Should the selection be stored, so that it needn't be recomputed?",  "false"  )
		+ XMLSchemaAttribute::attribute_w_default( "use_sidechain_neighbors",        xsct_rosetta_bool, "If true (the default), then the sidechain neighbor algorithm is used to determine burial.  If false, burial is based on SASA (solvent-accessible surface area) calculations.",  "true"  )
		+ XMLSchemaAttribute::attribute_w_default( "asu_only",                       xsct_rosetta_bool, "if true, returns selection of only asymmetric unit.",  "false"  )
		+ XMLSchemaAttribute( "ball_radius",                    xsct_real , "The radius value for the rolling ball algorithm used for SASA (solvent-accessible surface area) calculations.  Only used if use_sidechain_neighbors=false." )
		+ XMLSchemaAttribute( "sc_neighbor_dist_midpoint",      xsct_real , "The midpoint of the distance-dependent sigmoidal falloff for the sidechain-neighbors algorithm.  Only used if use_sidechain_neighbors=true." )
		+ XMLSchemaAttribute( "sc_neighbor_denominator",        xsct_real , "A parameter affecting the selection of residues by the method of sidechain neighbors.  This almost "
		"never needs to be changed from its default value of 1.0."
		)
		+ XMLSchemaAttribute( "sc_neighbor_angle_shift_factor", xsct_real , "A parameter affecting the angular falloff when selecting residues by the method of sidechain "
		"neighbors.  This rarely needs to be changed from its default value of 0.5." )
		+ XMLSchemaAttribute( "sc_neighbor_angle_exponent",     xsct_real , "The angle exponent when selecting residues by the method of sidechain neighbors.  This determines "
		"how fuzzy the edges of the cones are.  The default value is 2.0.  See the code in core/select/util/SelectResiduesByLayer.hh for more details of the algorithm."
		)
		+ XMLSchemaAttribute( "sc_neighbor_dist_exponent",      xsct_real , "The distance exponent when selecting residues by the method of sidechain neighbors.  This determines "
		"how fuzzy the distance cutoff is.  The default value is 1.0.  See the code in core/select/util/SelectResiduesByLayer.hh for more details of the algorithm."
		)
		+ XMLSchemaAttribute( "core_cutoff",                    xsct_real , "The cutoff value for considering a position to be part of the core.  If use_sidechain_neighbors is true "
		"(the default), then positions with MORE than this number of neighbors will be counted as core, and the default value is 5.2 neighbors.  (Note that the fuzziness of "
		"sidechain cones allows for non-integer neighbor counts.)  If use_sidechain_neighbors is false, then positions with LESS than this amount of solvent-exposed surface "
		"area will be counted as core, and the default is 20.0 A."
		)
		+ XMLSchemaAttribute( "surface_cutoff",                 xsct_real , "The cutoff value for considering a position to be part of the surface.  If use_sidechain_neighbors is true "
		"(the default), then positions with FEWER than this number of neighbors will be counted as surface, and the default value is 2.0 neighbors.  (Note that the fuzziness of "
		"sidechain cones allows for non-integer neighbor counts.)  If use_sidechain_neighbors is false, then positions with MORE than this amount of solvent-exposed surface area "
		"will be counted as surface, and the default is 40.0 A." );
	xsd_type_definition_w_attributes( xsd, class_name(), "The layer selector divides residues into core, boundary, and surface regions based either on number of sidechain neighbors "
		"(the default) or solvent-accessible surface area, and then selects residues in one or more of these regions.", attributes );
}


/// @brief Set the layers that this selector will choose.
///
void
LayerSelector::set_layers(
	bool const pick_core,
	bool const pick_boundary,
	bool const pick_surface
) {
	srbl_->set_design_layer(pick_core, pick_boundary, pick_surface);
	if ( TR.visible() ) {
		TR << "Setting core=" << (pick_core ? "true" : "false") << " boundary=" << (pick_boundary ? "true" : "false") << " surface=" << (pick_surface ? "true" : "false") << " in LayerSelector." << std::endl;
		TR.flush();
	}
	return;
}

/// @brief Set the radius for the rolling ball algorithm used to determine burial.
///
void
LayerSelector::set_ball_radius(
	core::Real const radius
) {
	srbl_->pore_radius(radius);
	if ( TR.visible() ) {
		TR << "Setting radius for rolling ball algorithm to " << radius << " in LayerSelector.  (Note that this will have no effect if the sidechain neighbors method is used.)" << std::endl;
		TR.flush();
	}
	return;
}

/// @brief Set whether the sidechain neighbors algorithm is used to determine burial (as
/// opposed to the rolling ball algorithm).
void LayerSelector::set_use_sc_neighbors( bool const val )
{
	srbl_->use_sidechain_neighbors( val );
	if ( TR.visible() ) {
		if ( val ) {
			TR << "Setting LayerSelector to use sidechain neighbors to determine burial." << std::endl;
		} else {
			TR << "Setting LayerSelector to use rolling ball-based occlusion to determine burial." << std::endl;
		}
		TR.flush();
	}
	return;
}

/// @brief Get whether the sidechain neighbors algorithm is used to determine burial (as
/// opposed to the rolling ball algorithm).
bool LayerSelector::use_sc_neighbors() const { return srbl_->use_sidechain_neighbors(); }

/// @brief Set the midpoint of the distance falloff if the sidechain neighbors method is used
/// to define layers.
void LayerSelector::set_sc_neighbor_dist_midpoint( core::Real const val )
{
	srbl_->set_dist_midpoint(val);
	if ( TR.visible() ) {
		TR << "Set distance falloff midpoint for the LayerSelector to " << val << ".  Note that this has no effect if the sidechain neighbors method is not used." << std::endl;
		TR.flush();
	}
	return;
}

/// @brief Set the factor by which sidechain neighbor counts are divided if the sidechain
/// neighbors method is used to define layers.
void LayerSelector::set_sc_neighbor_denominator( core::Real const val )
{
	srbl_->set_rsd_neighbor_denominator(val);
	if ( TR.visible() ) {
		TR << "Set denominator factor for the LayerSelector to " << val << ".  Note that this has no effect if the sidechain neighbors method is not used." << std::endl;
		TR.flush();
	}
	return;
}

/// @brief Set the cutoffs for core and surface layers.
/// @details Boundary is defined implicitly.  This can be a SASA cutoff or a neighbor count, depending
/// on the algorithm.
void LayerSelector::set_cutoffs (core::Real const core, core::Real const surf)
{
	srbl_->sasa_core(core);
	srbl_->sasa_surface(surf);
	if ( TR.visible() ) {
		TR << "Set cutoffs for core and surface to " << core << " and " << surf << ", respectively, in LayerSelector." << std::endl;
		TR.flush();
	}
	return;
}

/// @brief Set the sidechain neighbor angle shift value.
/// @details See the core::select::util::SelectResiduesByLayer class for details of the math.
void
LayerSelector::set_angle_shift_factor( core::Real const val )
{
	srbl_->set_angle_shift_factor(val);
	if ( TR.visible() ) {
		TR << "Set angle shift value to " << val << " in LayerSelector.  Note that this has no effect if the sidechain neighbors method is not used." << std::endl;
		TR.flush();
	}
	return;
}

/// @brief Set the sidechain neighbor angle exponent.
/// @details See the core::select::util::SelectResiduesByLayer class for details of the math.
void
LayerSelector::set_angle_exponent( core::Real const val )
{
	srbl_->set_angle_exponent(val);
	if ( TR.visible() ) {
		TR << "Set angle exponent to " << val << " in LayerSelector.  Note that this has no effect if the sidechain neighbors method is not used." << std::endl;
		TR.flush();
	}
	return;
}

/// @brief Set the sidechain neighbor distance exponent.
/// @details See the core::select::util::SelectResiduesByLayer class for details of the math.
void
LayerSelector::set_dist_exponent( core::Real const val )
{
	srbl_->set_dist_exponent(val);
	if ( TR.visible() ) {
		TR << "Set distance exponent to " << val << " in LayerSelector.  Note that this has no effect if the sidechain neighbors method is not used." << std::endl;
		TR.flush();
	}
	return;
}

void
LayerSelector::set_asu_only( bool const asu_only )
{
	asu_only_ = asu_only;
}

/// @brief Provide the citation.
void
LayerSelector::provide_citation_info(basic::citation_manager::CitationCollectionList & citations ) const {
	basic::citation_manager::CitationCollectionOP cc(
		utility::pointer::make_shared< basic::citation_manager::CitationCollection >(
		"LayerSelector", basic::citation_manager::CitedModuleType::ResidueSelector
		)
	);

	cc->add_citation( basic::citation_manager::CitationManager::get_instance()->get_citation_by_doi( "10.1073/pnas.1710695114" ) );

	citations.add( cc );
}

/// @brief Return an owning pointer to a new LayerSelector object.
///
ResidueSelectorOP
LayerSelectorCreator::create_residue_selector() const {
	return utility::pointer::make_shared< LayerSelector >();
	//return utility::pointer::make_shared< LayerSelector >();
}

/// @brief Get the class name.
/// @details Also calls class_name.
std::string
LayerSelectorCreator::keyname() const {
	return LayerSelector::class_name();
}

void
LayerSelectorCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const {
	LayerSelector::provide_xml_schema( xsd );
}

} //namespace residue_selector
} //namespace select
} //namespace core

#ifdef SERIALIZATION

/// @brief Automatically generated serialization method
template< class Archive >
void
core::select::residue_selector::LayerSelector::save( Archive & arc ) const {
	arc( cereal::base_class< core::select::residue_selector::ResidueSelector >( this ) );
	arc( CEREAL_NVP( cache_selection_ ) ); // bool
	arc( CEREAL_NVP( asu_only_ ) ); // bool
	arc( CEREAL_NVP( srbl_ ) ); // core::select::util::SelectResiduesByLayerOP
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::select::residue_selector::LayerSelector::load( Archive & arc ) {
	arc( cereal::base_class< core::select::residue_selector::ResidueSelector >( this ) );
	arc( cache_selection_ ); // bool
	arc( asu_only_ ); // bool
	arc( srbl_ ); // core::select::util::SelectResiduesByLayerOP
}

SAVE_AND_LOAD_SERIALIZABLE( core::select::residue_selector::LayerSelector );
CEREAL_REGISTER_TYPE( core::select::residue_selector::LayerSelector )

CEREAL_REGISTER_DYNAMIC_INIT( core_select_residue_selector_LayerSelector )
#endif // SERIALIZATION
