#ifndef DECON_HPP
#define DECON_HPP
#include <vector>
#include <string>
#include <ostream>
#include "genotypekit.hpp"
#include "Variant.h"
#include "path.hpp"
#include "vg.hpp"
#include "vg.pb.h"
#include "Fasta.h"

/** \file
* Deconstruct is getting rewritten.
* New functionality:
* -Detect superbubbles and bubbles
* -Fix command line interface.
* -harmonize on XG / raw graph (i.e. deprecate index)
* -Use unroll/DAGify if needed to avoid cycles

** Much of this is taken from Brankovic's
** "Linear-Time Superbubble Identification Algorithm for Genome Assembly"
*/
namespace vg{
    using namespace std;
          class Deconstructor{
        public:

            Deconstructor();
            ~Deconstructor();

            void deconstruct(string refpath, vg::VG* graph);
            void deconstruct(vector<string> refpaths, vg::VG* graph); 

        private:
    };
}
#endif
