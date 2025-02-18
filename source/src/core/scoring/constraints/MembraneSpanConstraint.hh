// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/scoring/constraints/ResidueConstraint.hh
///
/// @brief constarint the TM spans specified by user to stay in the membrane
/// @details penalizes the pose for TM span residues that are far from the membrane mid-plane (Z=0)
/// mostly useful for hetero fold and dock, where sampling space is large, and Rosetta seems to like extracting the TM spans from the membrane
/// @author Jonathan Weinstein


#ifndef INCLUDED_core_scoring_constraints_MembraneSpanConstraint_hh
#define INCLUDED_core_scoring_constraints_MembraneSpanConstraint_hh

#include <core/scoring/constraints/MembraneSpanConstraint.fwd.hh>
#include <core/scoring/constraints/Constraint.hh>

#include <core/conformation/Residue.fwd.hh>
#include <core/id/AtomID.hh>


#include <utility/vector1.hh>

// Membrane headers

#ifdef    SERIALIZATION
// Cereal headers
#include <cereal/types/polymorphic.fwd.hpp>
#endif // SERIALIZATION


namespace core {
namespace scoring {
namespace constraints {

/// @brief This class favors a particular residue identity at a particular position by reducing its res_type energy.
class MembraneSpanConstraint : public Constraint
{
public:
	typedef core::Real Real;
public:
	MembraneSpanConstraint();

	MembraneSpanConstraint(
		pose::Pose const & pose
	);

	~MembraneSpanConstraint() override;

	Size
	natoms() const override { return 0; }

	AtomID const &
	atom( Size const ) const override {
		utility_exit_with_message("MembraneSpanConstraint is not atom-based!.");
		return core::id::GLOBAL_BOGUS_ATOM_ID;  // required for compilation on Windows
	}

	utility::vector1< core::Size >
	residues() const override;

	void
	show( std::ostream & out ) const override;

	ConstraintOP
	remap_resid( core::id::SequenceMapping const & ) const override;

	bool operator == ( Constraint const & other ) const override;
	bool same_type_as_me( Constraint const & other ) const override;

	ConstraintOP remapped_clone(
		pose::Pose const & src,
		pose::Pose const & dest,
		id::SequenceMappingCOP map = nullptr
	) const override;

	// Needed to get the base class overloads
	using Constraint::score;
	using Constraint::dist;

	void
	score( func::XYZ_Func const &, EnergyMap const &, EnergyMap & emap ) const override;

	core::Real
	calc_score() const;

	core::Real
	score_res_z( core::Real z ) const;

	/// @details Return 1.0 if constraint will get a bonus, 0.0 if not
	core::Real
	dist( core::scoring::func::XYZ_Func const & xyz ) const override;

	void
	fill_f1_f2(
		AtomID const & atom,
		func::XYZ_Func const & xyz,
		Vector & F1,
		Vector & F2,
		EnergyMap const & weights
	) const override;

	ConstraintOP
	clone() const override;
private:
	utility::vector1< core::conformation::ResidueCOP > span_res_;

#ifdef    SERIALIZATION
public:
	template< class Archive > void save( Archive & arc ) const;
	template< class Archive > void load( Archive & arc );
#endif // SERIALIZATION

}; // RotamerConstraint

typedef utility::pointer::shared_ptr< MembraneSpanConstraint > MembraneSpanConstraintOP;
typedef utility::pointer::shared_ptr< MembraneSpanConstraint const > MembraneSpanConstraintCOP;

} // namespace constraints
} // namespace scoring
} // namespace core

#ifdef    SERIALIZATION
CEREAL_FORCE_DYNAMIC_INIT( core_scoring_constraints_MembraneSpanConstraint )
#endif // SERIALIZATION


#endif // INCLUDED_core_scoring_constraints_MembraneSpanConstraint_HH
