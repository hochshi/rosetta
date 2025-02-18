
########################
How to get this sample_source setup:

1) put directory of pdbs into the input/ directory, e.g. 
   ln -s /path/to/datasets/top800/input/top8000_chains_eds_70 .

2) run setup_resfiles.py to generate rosetta_inputs.db3:
   python setup_resfiles.py --data_dir top4400_chains_eds_70

3) run convert_htam_names.py to convert Reduce names to hydrogen names:
   python convert hatm_names.py --data_dir top8000_chains_eds_70 --output_dir top8000_chains_eds_70_rosetta_named_hydrogens

4) setup the list of structures to use:
   ls top8000_chains_eds_70_rosetta_named_hydrogens/ > all_pdbs.list' 

5) if you didn't use the path 'top8000_chains_eds_70_rosetta_named_hydrogens', edit the '-in:path' option in in flags.TEMPLATE 

6) add this sample_source to features/sample_source/benchmark.list
################################

The Top8000: An Origin Story

Daniel A. Keedy, W. Bryan Arendall, Lindsay N. Deis, Jane S. Richardson
July 6, 2011

Motivation

The Protein Data Bank has grown rapidly in the years since our most
recently published quality-filtered structural database, the Top500
(Lovell 2003).  In 2007 we attempted to take advantage of this new
data by creating the Top5200.  It maintained similar standards of
resolution and structure quality but, due to sheer logistics, required
a more automated selection protocol.  Unfortunately, we
unintentionally included chains with MolProbity score > 2.0, up to >
2.7 in some cases, although we included only chains from structures
with resolution < 2.0 as intended.  In 2010 we implemented a stop-gap
successor, the Top4400, by simply eliminating all chains with
MolProbity score > 2.0.  This database was inherently suboptimal
because no attempt was made to find suitable replacements.

The Top5200 was used for DAK's backrubs-in-evolution paper (to be
published) and the Top4400 was used for JSR's Validation Task Force
paper (to be published), and both have been distributed informally to
collaborators, but otherwise neither was fully accepted as our
next-generation database (e.g. neither is available on our website).
To facilitate new structural bioinformatics studies, this month (May
2011) we constructed the Top8000 databases of high-quality protein
structures.

Methodology

WBA ran 'reduce -flip' on all X-ray structures in the PDB as of March
29, 2011, in order to allow Asn/Gln/His flips throughout the
structure, including at interfaces where multimer partners may
participate in hydrogen-bonding networks.  He then extracted single
protein chains along with any "het" atoms or waters with the same
chain identifier.

Next, for each chain he calculated MolProbity score, an estimate of
the resolution at which a structure's steric clashes, rotamer quality,
and Ramachandran quality would be average; thus the average of
resolution and MolProbity score is a combined experimental and
statistical indicator of structural quality.

In terms of filtering, we eliminated chains that were marked by the
PDB as obsolete as of 4/13/2011, retracted in the Murthy UAB scandal,
atomically incomplete (<25% of residues with sidechains), or too short
(<38 residues according to the MolProbity "oneline" script).  Finally,
WBA downloaded the PDB's chain-level homology clusters as of
March 29, 2011 (actually released earlier that week on March 25,
2011).

After conversations with WBA, LND, and JSR, DAK required each chain to
have resolution < 2.0, chain MolProbity score < 2.0, ≤ 5% of
residues with bond length outliers (> 4σ), ≤ 5% of residues
with bond angle outliers (> 4σ), and ≤ 5% of residues with
Cβ deviation outliers (> 0.25Å).  He then selected the best chain
(in terms of average of resolution and chain MolProbity score) per
homology cluster.  There was a small number of ties within clusters
(for < 1% of the final chain tallies); these were resolved,
arbitrarily but reproducibly, by alphabetical order of PDB ID +
single-character chain ID.

This was done separately for 50% (“most stringent”
homology filtering), 70%, 90%, and 95% (“least
stringent” homology filtering) levels.  The homology filters,
to varying levels, prevent redundancy and thus over-representation of
certain motifs or substructures.

Originally a filter was planned to eliminate “suspiciously
good” chains with resolution > 1, chain MolProbity score = 0.5
(the optimal score), and clashscore = 0 (ditto).  However, we
determined that such a filter would eliminate several true
“paragons” (truly error-free models), and thus we did
not use it.

The “geometry” filters (bond lengths and angles and
Cβ deviations) were not used in the Top500, Top5200, or Top4400
– they are new to the Top8000.  Fortunately, these filters
eliminate a remarkably small number of clusters, so we gain in quality
within each cluster with little loss in quantity of clusters.

Chain MolProbity scores, as opposed to file MolProbity scores, were
used here, although in the future we may investigate some alternative
that accounts for both or applies some average chain MolProbity score
“correction factor” differentially for chains from
single-chain vs. multi-chain structures.

Availability of electron density maps from the EDS was tabulated but
not used for selection for the “primary” version of
the Top8000.  A second version was also compiled in which the
availability of a map in the EDS was required during the database
creation process for each entry.

Results

MySQL tables were stored on c3po in the top8000 database.  The
single-chain PDB files were stored on c3po in
/c3po-work/DataSets/top8000/ and on arachne (i.e. mcp) in
/labwork/top8000/.

Chain counts without the EDS requirement (the
“standard” or “normal” Top8000):

top8000_chains_50   7232
top8000_chains_70   7957 (≈ 8000, thus the name Top8000!)
top8000_chains_90   8562
top8000_chains_95   8825

Chain counts with the EDS requirement:
top8000_chains_eds_50   6107
top8000_chains_eds_70   6663
top8000_chains_eds_90   7138
top8000_chains_eds_95   7342
