/** \file call_main.cpp
 *
 * Defines the "vg call" subcommand, which calls variation from a graph and a pileup.
 */

#include <omp.h>
#include <unistd.h>
#include <getopt.h>

#include <list>
#include <fstream>

#include "subcommand.hpp"

#include "../option.hpp"

#include "../vg.hpp"
#include "../caller.hpp"



using namespace std;
using namespace vg;
using namespace vg::subcommand;

void help_call(char** argv, ConfigurableParser& parser) {
    cerr << "usage: " << argv[0] << " call [options] <graph.vg> <pileup.vgpu> > output.vcf" << endl
         << "Output variant calls in VCF or Loci format given a graph and pileup" << endl
         << endl
         << "options:" << endl
         << "    -d, --min-depth INT         minimum depth of pileup [" << Caller::Default_min_depth <<"]" << endl
         << "    -e, --max-depth INT         maximum depth of pileup [" << Caller::Default_max_depth <<"]" << endl
         << "    -s, --min-support INT       minimum number of reads required to support snp [" << Caller::Default_min_support <<"]" << endl
         << "    -f, --min-frac FLOAT        minimum percentage of reads required to support snp[" << Caller::Default_min_frac <<"]" << endl
         << "    -q, --default-read-qual N   phred quality score to use if none found in the pileup ["
         << (int)Caller::Default_default_quality << "]" << endl
         << "    -b, --max-strand-bias FLOAT limit to absolute difference between 0.5 and proportion of supporting reads on reverse strand. [" << Caller::Default_max_strand_bias << "]" << endl
         << "    -a, --link-alts             add all possible edges between adjacent alts" << endl
         << "    -A, --aug-graph FILE        write out the agumented graph in vg format" << endl
         << "    -U, --subgraph              expect a subgraph and ignore extra pileup entries outside it" << endl
         << "    -P, --pileup                write pileup under VCF lines (for debugging, output not valid VCF)" << endl
         << "    -h, --help                  print this help message" << endl
         << "    -p, --progress              show progress" << endl
         << "    -v, --verbose               print information and warnings about vcf generation" << endl
         << "    -t, --threads N             number of threads to use" << endl;
     
     // Then report more options
     parser.print_help(cerr);
}

int main_call(int argc, char** argv) {

    double het_prior = Caller::Default_het_prior;
    int min_depth = Caller::Default_min_depth;
    int max_depth = Caller::Default_max_depth;
    int min_support = Caller::Default_min_support;
    double min_frac = Caller::Default_min_frac;
    int default_read_qual = Caller::Default_default_quality;
    double max_strand_bias = Caller::Default_max_strand_bias;
    string aug_file;
    bool bridge_alts = false;
    
    
    
    // Should we expect a subgraph and ignore pileups for missing nodes/edges?
    bool expect_subgraph = false;
    
    // Should we annotate the VCF with pileup info?
    bool pileup_annotate = false;

    // This manages conversion from an augmented graph to a VCF, and makes the
    // actual calls.
    Call2Vcf call2vcf;

    bool show_progress = false;
    int thread_count = 1;

    static const struct option long_options[] = {
        {"min-depth", required_argument, 0, 'd'},
        {"max-depth", required_argument, 0, 'e'},
        {"min-support", required_argument, 0, 's'},
        {"min-frac", required_argument, 0, 'f'},
        {"default-read-qual", required_argument, 0, 'q'},
        {"max-strand-bias", required_argument, 0, 'b'},
        {"aug-graph", required_argument, 0, 'A'},
        {"link-alts", no_argument, 0, 'a'},
        {"progress", no_argument, 0, 'p'},
        {"verbose", no_argument, 0, 'v'},
        {"threads", required_argument, 0, 't'},
        {"subgraph", no_argument, 0, 'U'},
        {"pileup", no_argument, 0, 'P'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    static const char* short_options = "d:e:s:f:q:b:A:apvt:UPh";
    optind = 2; // force optind past command positional arguments

    // This is our command-line parser
    ConfigurableParser parser(short_options, long_options, [&](int c) {
        // Parse all the options we have defined here.
        switch (c)
        {
        case 'd':
            min_depth = atoi(optarg);
            break;
        case 'e':
            max_depth = atoi(optarg);
            break;
        case 's':
            min_support = atoi(optarg);
            break;
        case 'f':
            min_frac = atof(optarg);
            break;
        case 'q':
            default_read_qual = atoi(optarg);
            break;
        case 'b':
            max_strand_bias = atof(optarg);
            break;
        case 'A':
            aug_file = optarg;
            break;
        case 'a':
            bridge_alts = true;
            break;
        case 'U':
            expect_subgraph = true;
            break;
        // old glenn2vcf opts start here
        case 'P':
            pileup_annotate = true;
            break;
        case 'p':
            show_progress = true;
            break;
        case 'v':
            call2vcf.verbose = true;
            break;
        case 't':
            thread_count = atoi(optarg);
            break;
        case 'h':
        case '?':
            /* getopt_long already printed an error message. */
            help_call(argv, parser);
            exit(1);
            break;
        default:
          abort ();
        }
    });
    // Register the call2vcf converter for configuring with its options.
    parser.register_configurable(&call2vcf);

    if (argc <= 3) {
        help_call(argv, parser);
        return 1;
    }
    
    // Parse the command line options, updating optind.
    parser.parse(argc, argv);

    // Set up threading according to info parsed from the options.
    omp_set_num_threads(thread_count);
    thread_count = get_thread_count();

    // Parse the arguments
    if (optind >= argc) {
        help_call(argv, parser);
        return 1;
    }
    string graph_file_name = get_input_file_name(optind, argc, argv);
    if (optind >= argc) {
        help_call(argv, parser);
        return 1;
    }
    string pileup_file_name = get_input_file_name(optind, argc, argv);
    
    if (pileup_file_name == "-" && graph_file_name == "-") {
        cerr << "error: graph and pileup can't both be from stdin." << endl;
        exit(1);
    }
    
    // read the graph
    if (show_progress) {
        cerr << "Reading input graph" << endl;
    }
    VG* graph;
    get_input_file(graph_file_name, [&](istream& in) {
        graph = new VG(in);
    });

    if (show_progress) {
        cerr << "Computing augmented graph" << endl;
    }
    Caller caller(graph,
                  het_prior, min_depth, max_depth, min_support,
                  min_frac, Caller::Default_min_log_likelihood,
                  default_read_qual, max_strand_bias, bridge_alts);

    // setup pileup stream
    get_input_file(pileup_file_name, [&](istream& pileup_stream) {
        // compute the augmented graph
        function<void(Pileup&)> lambda = [&](Pileup& pileup) {
            for (int i = 0; i < pileup.node_pileups_size(); ++i) {
                if (!graph->has_node(pileup.node_pileups(i).node_id())) {
                    // This pileup doesn't belong in this graph
                    if(!expect_subgraph) {
                        throw runtime_error("Found pileup for nonexistent node " + to_string(pileup.node_pileups(i).node_id()));
                    }
                    // If that's expected, just skip it
                    continue;
                }
                // Send approved pileups to the caller
                caller.call_node_pileup(pileup.node_pileups(i));
            }
            for (int i = 0; i < pileup.edge_pileups_size(); ++i) {
                if (!graph->has_edge(pileup.edge_pileups(i).edge())) {
                    // This pileup doesn't belong in this graph
                    if(!expect_subgraph) {
                        throw runtime_error("Found pileup for nonexistent edge " + pb2json(pileup.edge_pileups(i).edge()));
                    }
                    // If that's expected, just skip it
                    continue;
                }
                // Send approved pileups to the caller
                caller.call_edge_pileup(pileup.edge_pileups(i));
            }
        };
        stream::for_each(pileup_stream, lambda);
    });
    
    // map the edges from original graph
    if (show_progress) {
        cerr << "Mapping edges into augmented graph" << endl;
    }
    caller.update_augmented_graph();

    // map the paths from the original graph
    if (show_progress) {
        cerr << "Mapping paths into augmented graph" << endl;
    }
    caller.map_paths();

    if (!aug_file.empty()) {
        // write the augmented graph
        if (show_progress) {
            cerr << "Writing augmented graph" << endl;
        }
        ofstream aug_stream(aug_file.c_str());
        caller.write_augmented_graph(aug_stream, false);
    }
    
    if (show_progress) {
        cerr << "Calling variants" << endl;
    }

    // project the augmented graph to a reference path
    // in order to create a VCF of calls.
    call2vcf.call(caller._augmented_graph,
        pileup_annotate ? pileup_file_name : string());

    return 0;
}

// Register subcommand
static Subcommand vg_call("call", "call variants on a graph from a pileup", main_call);

