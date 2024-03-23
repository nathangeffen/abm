#include <unistd.h>
#include <iostream>
#include <stdexcept>
#include <string>

#include "abm.hpp"


void print_help(std::ostream &os, const char *prog) {
  os << "Usage: " << prog << " [options]\n";
  os << "Options\n";
  os << "-s <string> where string is the name of a parameter group\n";
  os << "-a <number> where <number> is the number of  initial agents\n";
  os << "-p <parameter_name>:<real> to set a parameter value\n";
  os << "-t <state_from><state_to>:<real> to set a transition value\n";
  os << "-n <number> where number is the number of simulations\n";
  os << "-m <model> where profile is one of these prespecified models.\n"
     << "   covid\n";
}

void process_arguments(int argc, char *argv[],
		       std::vector<Parameters>& parameterVector) {
  char opt;
  Parameters p;
  bool initialized = false;
  while ((opt = getopt(argc, argv, "s:a:p:t:n:m:h")) != -1) {
    switch (opt) {
    case 's':
      if (initialized) {
	parameterVector.push_back(p);
      } else {
	initialized = true;
      }
      p.name = optarg;
      break;
    case 'a':
      p.num_initial_agents = std::stoi(optarg);
      break;
    case 'p':
      p.setParameters(optarg);
      break;
    case 't':
      p.setTransition(optarg);
      break;
    case 'n':
      p.num_simulations = std::stoi(optarg);
      break;
    case 'm':
      p.setModel(optarg);
      break;
    case 'h':
      print_help(std::cout, argv[0]);
      exit(EXIT_SUCCESS);
    default:
      std::cerr << "Unknown option " << opt << "\n";
      print_help(std::cerr, argv[0]);
      exit(EXIT_FAILURE);
    }
  }
  parameterVector.push_back(p);
}

int main(int argc, char *argv[]) {
  std::vector<Parameters> parameterVector;
  try {
    process_arguments(argc, argv, parameterVector);
  } catch (const std::exception& e) {
    std::cerr << "Error in arguments.\n" << e.what() << "\n";
    exit(EXIT_FAILURE);
  }
  try {
    for (auto& parameters: parameterVector) {
      Simulation simulation;
      SimulationGroup group;
      simulation.parameters = parameters;
      group.createSimulations(simulation);
      group.simulate();
    }
  } catch (const std::exception& e) {
    std::cerr << "Error running simulations.\n" << e.what() << "\n";
    exit(EXIT_FAILURE);
  }
  return 0;
}
