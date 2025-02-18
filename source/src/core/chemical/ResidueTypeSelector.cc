// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   ResidueTypeSelector.cc
/// @note This is NOT a ResidueSelector!  This file predates ResidueSelectors.
/// @author Phil Bradley


// Unit headers
#include <core/chemical/ResidueTypeSelector.hh>
#include <core/chemical/ResidueType.hh>
#include <core/chemical/MutableResidueType.hh>


// Basic headers
#include <basic/options/option.hh>
#include <basic/options/keys/chemical.OptionKeys.gen.hh>

// Utility headers
#include <utility/vector1.hh>
#include <basic/Tracer.hh>

#ifdef    SERIALIZATION
// Utility serialization headers
#include <utility/vector1.srlz.hh>
#include <utility/serialization/serialization.hh>

// Cereal headers
#include <cereal/access.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/polymorphic.hpp>
#endif // SERIALIZATION

namespace core {
namespace chemical {

static basic::Tracer TR("core.chemical.ResidueTypeSelector");

/// @details Auto-generated virtual destructor
ResidueTypeSelector::~ResidueTypeSelector() = default;

/// @details Auto-generated virtual destructor
ResidueTypeSelectorSingle::~ResidueTypeSelectorSingle() = default;


////////////////////////////////////////////////////////////////////////////////////////

ResidueTypeSelectorSingleOP
residue_selector_single_from_line( std::string const & line )
{
	std::istringstream l( line );
	std::string tag;
	l >> tag;
	if ( l.fail() ) return nullptr;

	bool desired_result( true );
	if ( tag == "NOT" ) {
		desired_result = false;
		l >> tag;
	}

	if ( tag == "AA" ) {
		AA aa;
		utility::vector1< AA > aas;
		l >> aa;
		while ( !l.fail() ) {
			aas.push_back( aa );
			l >> aa;
		}
		//std::cout << "AAline: " << aas.size() << ' ' << line << std::endl;
		if ( !aas.empty() ) return utility::pointer::make_shared< Selector_AA >( aas, desired_result );

	} else if ( tag == "NAME3" ) {
		std::string name3;
		utility::vector1< std::string > name3s;
		l >> name3;
		while ( !l.fail() ) {
			name3s.push_back( name3 );
			l >> name3;
		}
		if ( !name3s.empty() )  return utility::pointer::make_shared< Selector_NAME3 >( name3s, desired_result );

	} else if ( tag == "BASENAME" ) {
		std::string basename;
		utility::vector1< std::string > basenames;
		l >> basename;
		while ( !l.fail() ) {
			basenames.push_back( basename );
			l >> basename;
		}
		if ( !basenames.empty() )  return utility::pointer::make_shared< Selector_BASENAME >( basenames, desired_result );

	} else if ( tag == "HAS_ATOMS" ) {
		std::string atom_name;
		utility::vector1< std::string > atom_names;
		l >> atom_name;
		while ( !l.fail() && atom_name[0] != '#' ) {
			atom_names.push_back( atom_name );
			l >> atom_name;
		}
		if ( !atom_names.empty() ) return utility::pointer::make_shared< Selector_HAS_ATOMS >( atom_names, desired_result );

	} else if ( tag == "PROPERTY" ) {
		std::string property;
		utility::vector1< std::string > properties;
		l >> property;
		while ( !l.fail() && property[0] != '#' ) {
			properties.push_back( property );
			l >> property;
		}
		if ( !properties.empty() ) return utility::pointer::make_shared< Selector_PROPERTY >( properties, desired_result );

	} else if ( tag == "VARIANT_TYPE" ) {
		std::string variant_type;
		utility::vector1< std::string > variant_types;
		l >> variant_type;
		while ( !l.fail() && variant_type[0] != '#' ) {
			variant_types.push_back( variant_type );
			l >> variant_type;
		}
		if ( !variant_types.empty() ) return utility::pointer::make_shared< Selector_VARIANT_TYPE >( variant_types, desired_result );

	} else if ( tag == "UPPER_POSITION" ) {  // This is the position label at which the upper connection is attached.
		utility_exit_with_message("The UPPER_POSITION selector has been removed. Use UPPER_ATOM (specifying the atom name attached to the UPPER connection) instead.");
	} else if ( tag == "UPPER_ATOM" ) {
		std::string position;
		l >> position;
		if ( !position.empty() ) {
			return utility::pointer::make_shared< Selector_UPPER_ATOM >(position, desired_result);
		}

	} else if ( tag == "CMDLINE_SELECTOR" ) {
		std::string selector_string;
		l >> selector_string; // if one wants AND logical operation make this a vector of selector_strings...
		if ( !selector_string.empty() ) return utility::pointer::make_shared< Selector_CMDFLAG >( selector_string, desired_result );
	}
	TR.Warning << "residue_selector_single: unrecognized line: " << line << std::endl;
	return nullptr;
}

// add a new selector single
ResidueTypeSelector & // allow chaining
ResidueTypeSelector::add_line( std::string const & line )
{
	ResidueTypeSelectorSingleOP new_selector( residue_selector_single_from_line( line ) );
	if ( new_selector ) {
		//std::cout << "add_line: success: " << line << std::endl;
		selectors_.push_back( new_selector );
	} else {
		TR.Warning << "ResidueTypeSelector::add_line: bad line:" << line << std::endl;
	}
	return *this;
}

Selector_CMDFLAG::Selector_CMDFLAG(std::string  const & flag_in, bool const result) : ResidueTypeSelectorSingle( result )
{
	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	b_flag_is_present_ = false;
	if ( !option[ OptionKeys::chemical::patch_selectors ].user() ) return;
	for ( auto const & it : option[ OptionKeys::chemical::patch_selectors ]() ) {
		if ( it == flag_in ) {
			b_flag_is_present_ = true;
			break;
		}
	}
}

bool
Selector_UPPER_ATOM::operator[](ResidueTypeBase const & rsd) const {
	if ( ! position_.empty() && rsd.has( position_ ) ) {
		// Working with raw pointers because we need the dynamic cast null fall-back (and we don't worry about lifetime).
		ResidueType const * rt_ptr = dynamic_cast< ResidueType const * >( &rsd );
		if ( rt_ptr != nullptr ) {
			// Do ResidueType version
			if ( rt_ptr->upper_connect_id() != 0 && rt_ptr->atom_index( position_ ) == rt_ptr->upper_connect_atom() ) {
				return desired_result();
			}
		} else {
			MutableResidueType const * mrt_ptr = dynamic_cast< MutableResidueType const * >( &rsd );
			runtime_assert( mrt_ptr != nullptr ); // Shouldn't ever, as there's only two possibilities
			// Do MutableResidueType version
			if ( mrt_ptr->upper_connect_id() != 0 && mrt_ptr->atom_vertex( position_ ) == mrt_ptr->upper_connect_atom() ) {
				return desired_result();
			}
		}
	}
	return !desired_result();
}

} // chemical
} // core

#ifdef    SERIALIZATION

/// @brief Default constructor required by cereal to deserialize this class
core::chemical::Selector_PROPERTY::Selector_PROPERTY() {}

/// @brief Automatically generated serialization method
template< class Archive >
void
core::chemical::Selector_PROPERTY::save( Archive & arc ) const {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( CEREAL_NVP( custom_properties_ ) );
	arc( CEREAL_NVP( properties_ ) ); // utility::vector1<std::string>
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::chemical::Selector_PROPERTY::load( Archive & arc ) {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( custom_properties_ );
	arc( properties_ ); // utility::vector1<std::string>
}

SAVE_AND_LOAD_SERIALIZABLE( core::chemical::Selector_PROPERTY );
CEREAL_REGISTER_TYPE( core::chemical::Selector_PROPERTY )


/// @brief Default constructor required by cereal to deserialize this class
core::chemical::Selector_AA::Selector_AA() {}

/// @brief Automatically generated serialization method
template< class Archive >
void
core::chemical::Selector_AA::save( Archive & arc ) const {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( CEREAL_NVP( aas_ ) ); // utility::vector1<AA>
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::chemical::Selector_AA::load( Archive & arc ) {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( aas_ ); // utility::vector1<AA>
}

SAVE_AND_LOAD_SERIALIZABLE( core::chemical::Selector_AA );
CEREAL_REGISTER_TYPE( core::chemical::Selector_AA )

/// @brief Default constructor required by cereal to deserialize this class
core::chemical::Selector_BASENAME::Selector_BASENAME() {}

/// @brief Automatically generated serialization method
template< class Archive >
void
core::chemical::Selector_BASENAME::save( Archive & arc ) const {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( CEREAL_NVP( basenames_ ) ); // utility::vector1<std::string>
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::chemical::Selector_BASENAME::load( Archive & arc ) {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( basenames_ ); // utility::vector1<std::string>
}

SAVE_AND_LOAD_SERIALIZABLE( core::chemical::Selector_BASENAME );
CEREAL_REGISTER_TYPE( core::chemical::Selector_BASENAME )

/// @brief Default constructor required by cereal to deserialize this class
core::chemical::Selector_NAME3::Selector_NAME3() {}

/// @brief Automatically generated serialization method
template< class Archive >
void
core::chemical::Selector_NAME3::save( Archive & arc ) const {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( CEREAL_NVP( name3s_ ) ); // utility::vector1<std::string>
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::chemical::Selector_NAME3::load( Archive & arc ) {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( name3s_ ); // utility::vector1<std::string>
}

SAVE_AND_LOAD_SERIALIZABLE( core::chemical::Selector_NAME3 );
CEREAL_REGISTER_TYPE( core::chemical::Selector_NAME3 )


/// @brief Default constructor required by cereal to deserialize this class
core::chemical::Selector_VARIANT_TYPE::Selector_VARIANT_TYPE() {}

/// @brief Automatically generated serialization method
template< class Archive >
void
core::chemical::Selector_VARIANT_TYPE::save( Archive & arc ) const {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( CEREAL_NVP( custom_variants_ ) );
	arc( CEREAL_NVP( variants_ ) ); // utility::vector1<std::string>
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::chemical::Selector_VARIANT_TYPE::load( Archive & arc ) {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( custom_variants_ );
	arc( variants_ ); // utility::vector1<std::string>
}

SAVE_AND_LOAD_SERIALIZABLE( core::chemical::Selector_VARIANT_TYPE );
CEREAL_REGISTER_TYPE( core::chemical::Selector_VARIANT_TYPE )


/// @brief Default constructor required by cereal to deserialize this class
core::chemical::Selector_NAME1::Selector_NAME1() {}

/// @brief Automatically generated serialization method
template< class Archive >
void
core::chemical::Selector_NAME1::save( Archive & arc ) const {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( CEREAL_NVP( name1_ ) ); // const char
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::chemical::Selector_NAME1::load( Archive & arc ) {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( name1_ ); // const char
}

SAVE_AND_LOAD_SERIALIZABLE( core::chemical::Selector_NAME1 );
CEREAL_REGISTER_TYPE( core::chemical::Selector_NAME1 )


/// @brief Default constructor required by cereal to deserialize this class
core::chemical::ResidueTypeSelectorSingle::ResidueTypeSelectorSingle() {}

/// @brief Automatically generated serialization method
template< class Archive >
void
core::chemical::ResidueTypeSelectorSingle::save( Archive & arc ) const {
	arc( CEREAL_NVP( desired_result_ ) ); // _Bool
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::chemical::ResidueTypeSelectorSingle::load( Archive & arc ) {
	arc( desired_result_ ); // _Bool
}

SAVE_AND_LOAD_SERIALIZABLE( core::chemical::ResidueTypeSelectorSingle );
CEREAL_REGISTER_TYPE( core::chemical::ResidueTypeSelectorSingle )


/// @brief Default constructor required by cereal to deserialize this class
core::chemical::Selector_NO_VARIANTS::Selector_NO_VARIANTS() {}

/// @brief Automatically generated serialization method
template< class Archive >
void
core::chemical::Selector_NO_VARIANTS::save( Archive & arc ) const {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::chemical::Selector_NO_VARIANTS::load( Archive & arc ) {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
}

SAVE_AND_LOAD_SERIALIZABLE( core::chemical::Selector_NO_VARIANTS );
CEREAL_REGISTER_TYPE( core::chemical::Selector_NO_VARIANTS )


/// @brief Default constructor required by cereal to deserialize this class
core::chemical::Selector_UPPER_ATOM::Selector_UPPER_ATOM() {}

/// @brief Automatically generated serialization method
template< class Archive >
void
core::chemical::Selector_UPPER_ATOM::save( Archive & arc ) const {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( CEREAL_NVP( position_ ) ); // std::string
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::chemical::Selector_UPPER_ATOM::load( Archive & arc ) {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( position_ ); // std::string
}

SAVE_AND_LOAD_SERIALIZABLE( core::chemical::Selector_UPPER_ATOM );
CEREAL_REGISTER_TYPE( core::chemical::Selector_UPPER_ATOM )


/// @brief Automatically generated serialization method
template< class Archive >
void
core::chemical::ResidueTypeSelector::save( Archive & arc ) const {
	arc( CEREAL_NVP( selectors_ ) ); // utility::vector1<ResidueTypeSelectorSingleOP>
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::chemical::ResidueTypeSelector::load( Archive & arc ) {
	arc( selectors_ ); // utility::vector1<ResidueTypeSelectorSingleOP>
}

SAVE_AND_LOAD_SERIALIZABLE( core::chemical::ResidueTypeSelector );
CEREAL_REGISTER_TYPE( core::chemical::ResidueTypeSelector )


/// @brief Default constructor required by cereal to deserialize this class
core::chemical::Selector_MATCH_VARIANTS::Selector_MATCH_VARIANTS() {}

/// @brief Automatically generated serialization method
template< class Archive >
void
core::chemical::Selector_MATCH_VARIANTS::save( Archive & arc ) const {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( CEREAL_NVP( variants_ ) ); // utility::vector1<std::string>
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::chemical::Selector_MATCH_VARIANTS::load( Archive & arc ) {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( variants_ ); // utility::vector1<std::string>
}

SAVE_AND_LOAD_SERIALIZABLE( core::chemical::Selector_MATCH_VARIANTS );
CEREAL_REGISTER_TYPE( core::chemical::Selector_MATCH_VARIANTS )


/// @brief Default constructor required by cereal to deserialize this class
core::chemical::Selector_CMDFLAG::Selector_CMDFLAG() {}

/// @brief Automatically generated serialization method
template< class Archive >
void
core::chemical::Selector_CMDFLAG::save( Archive & arc ) const {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( CEREAL_NVP( b_flag_is_present_ ) ); // _Bool
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::chemical::Selector_CMDFLAG::load( Archive & arc ) {
	arc( cereal::base_class< core::chemical::ResidueTypeSelectorSingle >( this ) );
	arc( b_flag_is_present_ ); // _Bool
}

SAVE_AND_LOAD_SERIALIZABLE( core::chemical::Selector_CMDFLAG );
CEREAL_REGISTER_TYPE( core::chemical::Selector_CMDFLAG )

CEREAL_REGISTER_DYNAMIC_INIT( core_chemical_ResidueTypeSelector )
#endif // SERIALIZATION
