// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file /src/apps/pilat/will/genmatch.cc
/// @brief ???

#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/option.hh>
#include <basic/Tracer.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/conformation/Residue.hh>
#include <core/conformation/ResidueFactory.hh>
#include <core/kinematics/MoveMap.hh>
#include <protocols/minimization_packing/MinMover.hh>
#include <core/import_pose/import_pose.hh>
#include <devel/init.hh>
#include <core/io/pdb/pdb_writer.hh>
#include <core/kinematics/Stub.hh>
#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/sasa.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <numeric/xyz.functions.hh>
#include <numeric/xyz.io.hh>
#include <ObjexxFCL/FArray2D.hh>
#include <ObjexxFCL/string.functions.hh>
#include <protocols/scoring/ImplicitFastClashCheck.hh>
#include <sstream>
#include <utility/io/izstream.hh>
// #include <devel/init.hh>

// #include <core/scoring/constraints/LocalCoordinateConstraint.hh>
#include <utility/vector1.hh>

//Auto Headers
#include <platform/types.hh>
#include <core/pose/init_id_map.hh>
#include <apps/pilot/will/will_util.ihh>

using core::Real;
using core::Size;
using core::pose::Pose;
using core::kinematics::Stub;
using protocols::scoring::ImplicitFastClashCheck;
using std::string;
using utility::vector1;
using ObjexxFCL::string_of;
using ObjexxFCL::lead_zero_string_of;
using numeric::min;
using core::import_pose::pose_from_file;
using basic::options::option;

typedef numeric::xyzVector<Real> Vec;
typedef numeric::xyzMatrix<Real> Mat;

static basic::Tracer TR( "dunscores" );


void run() {
	using namespace basic::options::OptionKeys;
	using namespace core::id;

	vector1<string> infiles;
	if ( option[in::file::l].user() ) {
		utility::io::izstream in(option[in::file::l]()[1]);
		string tmp;
		while ( in >> tmp ) infiles.push_back(tmp);
	} else if ( option[in::file::s].user() ) {
		infiles = option[in::file::s]();
	} else {
		utility_exit_with_message("no input!");
	}

	core::scoring::ScoreFunctionOP sf = core::scoring::get_score_function(); //new core::scoring::ScoreFunction;
	//sf->set_weight(core::scoring::fa_dun,1.0);

	for ( Size ifile = 1; ifile <= infiles.size(); ifile++ ) {
		string infile = infiles[ifile];
		Pose pose;
		pose_from_file(pose,infile, core::import_pose::PDB_file);

		core::kinematics::MoveMapOP movemap = new core::kinematics::MoveMap;
		movemap->set_chi(true);
		movemap->set_bb(false);
		movemap->set_jump(false);
		protocols::minimization_packing::MinMover m( movemap, sf, "lbfgs_armijo_nonmonotone", 1e-5, true, false, false );
		m.apply(pose);

		sf->score(pose);
		for ( Size i = 1; i <= pose.size(); ++i ) {
			vector1<Real> c(4,0.0);
			for ( Size j = 1; j <= pose.residue(i).nchi(); ++j ) c[j] = pose.chi(j,i);
			TR << utility::file_basename(infile) << " " << pose.residue(i).name3() << " " << pose.energies().residue_total_energies(i)[core::scoring::fa_dun] << " " << c[1] << " " << c[2] << " " << c[3] << " " << c[4] << std::endl;
		}
	}
}


int main (int argc, char *argv[]) {

	try {

		devel::init(argc,argv);
		run();

	} catch (utility::excn::Exception const & e ) {
		e.display();
		return -1;
	}

}


