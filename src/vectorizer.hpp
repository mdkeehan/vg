#ifndef VECVEC
#define VECVEC
#include <iostream>
#include <sstream>
#include "sdsl/bit_vectors.hpp"
#include <vector>
#include "vg.hpp"
#include "xg.hpp"
#include "vg.pb.h"

/**
* This class provides a way to transform
* an alignment (or set of alignments) into
* various vector formats useful in downstream
* analysis. For example, the most basic is a
* one-hot vector displaying coverage at each node/edge
* entity in the graph.
*/
using namespace std;
using namespace sdsl;
using namespace vg;
class Vectorizer{
  public:
    Vectorizer(xg::XG x);
    ~Vectorizer();
    void add_bv(bit_vector v);
    void add_name(string n);
    void emit(ostream& out, bool r_format, bool annotate);
    bit_vector alignment_to_onehot(Alignment a);
    vector<double> alignment_to_custom_score(Alignment a, std::function<double(Alignment)> lambda);
  private:
    string format(bit_vector v);
    xg::XG my_xg;
    //We use vectors for both names and bit vectors because we want to allow the use of duplicate
    // names. This allows things like generating simulated data with true cluster as the name.
    vector<bit_vector> my_vectors;
    vector<string> my_names;

};

#endif