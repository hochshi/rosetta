// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file  core/energy_methods/MPPairEnergy.hh
///
/// @brief  Membrane Residue Pair Energy Term
/// @details Two Body Term - score residue-residue interactions in the membrane. Derived from Membrane
///    base potential and uses mpframework data
///    Last Modified: 3/28/14
///
/// @author  Rebecca Alford (rfalford12@gmail.com)

#ifndef INCLUDED_core_scoring_membrane_MPPairEnergy_hh
#define INCLUDED_core_scoring_membrane_MPPairEnergy_hh

// Unit Headers
//#include <core/energy_methods/MPPairEnergy.fwd.hh>

// Package headers
#include <core/scoring/methods/ContextDependentTwoBodyEnergy.hh>
#include <core/scoring/membrane/MembraneData.fwd.hh>
#include <core/scoring/ScoreFunction.fwd.hh>

#include <core/chemical/AA.hh>

// Project headers
#include <core/pose/Pose.fwd.hh>

// Utility Headers
#include <utility/vector1.hh>

namespace core {
namespace energy_methods {

/// @brief Membrane Environemtn Residue Pair Energy Term
class MPPairEnergy : public core::scoring::methods::ContextDependentTwoBodyEnergy {

public:
	typedef core::scoring::methods::ContextDependentTwoBodyEnergy parent;

public: // constructors

	/// @brief Default Constructor
	MPPairEnergy();

	/// @brief Clone Method
	core::scoring::methods::EnergyMethodOP
	clone() const override;

public: // scoring methods

	/// @brief Setup for Scoring - compute cen env and update neighbors
	void
	setup_for_scoring( pose::Pose & pose, core::scoring::ScoreFunction const & ) const override;

	/// @brief Compute residue pair energy in th emembrane
	void
	residue_pair_energy(
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2,
		pose::Pose const & pose,
		core::scoring::ScoreFunction const &,
		core::scoring::EnergyMap & emap
	) const override;


	/// @brief Fianlize whole structure energy
	void
	finalize_total_energy(
		pose::Pose & pose,
		core::scoring::ScoreFunction const &,
		core::scoring::EnergyMap &
	) const override;

	bool
	defines_intrares_energy( core::scoring::EnergyMap const & ) const override { return false; }

	void
	eval_intrares_energy(
		conformation::Residue const &,
		pose::Pose const &,
		core::scoring::ScoreFunction const &,
		core::scoring::EnergyMap &
	) const override {}

	/// @brief Define Atomic Interaction Cutoff == 6A
	Distance
	atomic_interaction_cutoff() const override;

	void
	indicate_required_context_graphs( utility::vector1< bool > & ) const override {}

	/// @brief Versioning
	core::Size version() const override;

public: // energy methods

	/// @brief Compute Reisdue-Residue Pair energy from z_position, aa, and Cb-Cb distance
	core::Real
	compute_mpair_score(
		core::Real const z_position1,
		core::Real const z_position2,
		chemical::AA const & aa1,
		chemical::AA const & aa2,
		core::Real const cendist
	) const;

private: // data

	// MP Potential Base Instance
	core::scoring::membrane::MembraneData const & mpdata_;

	// User option
	bool no_interpolate_mpair_;

};

} // scoring
} // core

#endif // INCLUDED_core_scoring_membrane_MPPairEnergy_hh
