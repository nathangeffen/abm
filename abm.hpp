#include <cassert>

#include <algorithm>
#include <functional>
#include <iterator>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#include <boost/asio.hpp>

std::random_device rd;
thread_local std::default_random_engine rng(rd());

enum State {
  SUSCEPTIBLE = 0,
  EXPOSED,
  INFECTIOUS_ASYMPTOMATIC,
  INFECTIOUS_SYMPTOMATIC,
  INFECTIOUS_HOSPITALIZED,
  INFECTIOUS_ICU,
  VACCINATED,
  RECOVERED,
  DEAD
};

struct StateEntry {
  const char *full;
  const char abbr;
};

const StateEntry STATES[] = {
  {"SUSCEPTIBLE", 'S'},
  {"EXPOSED", 'E'},
  {"INFECTIOUS_ASYMPTOMATIC", 'A'},
  {"INFECTIOUS_SYMPTOMATIC", 'Y'},
  {"INFECTIOUS_HOSPITALIZED", 'H'},
  {"INFECTIOUS_ICU", 'I'},
  {"VACCINATED", 'V'},
  {"RECOVERED", 'R'},
  {"DEAD", 'D'}
};

static const int FINAL = State::DEAD + 1;

#define IND(state_a, state_b) ((state_a) * FINAL)  + (state_b)

struct Agent {
  int id = 0;
  int iteration_changed = 0;
  bool isolated = false;
  double infectiousness = 0.0;
  double infectable  = 0.0;
  State state = State::SUSCEPTIBLE;
  std::unordered_map<int, State> history;
  void setState(const State new_state, int iteration,
		bool allow_change_on_same_iteration = false) {
    if (new_state == state) {
      ;
    } else if (iteration > 0 && iteration_changed == iteration &&
	       allow_change_on_same_iteration == false) {
      ;
    } else {
      state = new_state;
      history[iteration] = new_state;
      iteration_changed = iteration;
    }
  }
  void infect(const Agent& agent, int iteration,
	      bool allow_change_on_same_iteration = false) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double infection_risk = (agent.infectiousness + infectable) / 2.0;
    if (dist(rng) < infection_risk) {
      setState(State::EXPOSED, iteration, allow_change_on_same_iteration);
    }
  }
};

typedef std::vector<Agent> AgentVector;

std::ostream &operator<<(std::ostream &os, const Agent &agent) {
  os << agent.id << "," << agent.state << "\n";
  return os;
}

enum ExposeMethod {
  HOMOGENOUS = 0,
  RANDOM_CONTACTS
};

const std::vector<double> COVID_RISKS =  {
    //S,E,I_A,I_S,I_H,I_I,V,R,D
    0, 0, 0, 0, 0, 0, 0.00136986301369863, 0, 0.0000273973, //S
    0, 0, 0.2, 0, 0, 0, 0.00136986301369863, 0.001, 0.0000273973, //E
    0, 0, 0, 0.2, 0, 0, 0.00136986301369863, 0.2, 0.0000273973, //I_A
    0, 0, 0, 0, 0.1, 0, 0, 0.1, 0.0000547945, //I_S
    0, 0, 0, 0, 0, 0.1, 0, 0.1, 0.0001369863, //I_H
    0, 0, 0, 0, 0, 0, 0, 0.0002739726, 0.0002739726, //I_I
    0.001, 0, 0, 0, 0, 0, 0, 0, 0.0000273973, //V
    0.001, 0, 0, 0, 0, 0, 0.00136986301369863, 0, 0.0000273973, //R
    0, 0, 0, 0, 0, 0, 0, 0, 0 //D
};


struct Parameters {
  std::string name = "unnamed";
  int begin_iteration = 0;
  int end_iteration = 365;
  int num_initial_agents = 100;
  int num_simulations = 10;
  int contacts_per_iteration = 0;
  ExposeMethod expose_method = HOMOGENOUS;
  double beta = 0.004; // force of infection
  double isolation_prob = 0.0;
  std::vector<std::pair<State, double>> proportions = {
    {State::SUSCEPTIBLE, 0.99},
    {State::EXPOSED, 0.01}
  };

  std::vector<double> risks = COVID_RISKS;
  bool allow_change_on_same_iteration = false;

  void setModel(const std::string& arg) {
    if (arg == "covid") {
      std::copy(COVID_RISKS.begin(), COVID_RISKS.end(),
		std::back_inserter(risks));
    }
  }

  void setParameters(const std::string& arg) {
    size_t n = arg.find_first_of(":", 1);
    if (n == std::string::npos) {
      std::string s = "Parameters must be of the form <parameter_name:real>. "
	"Argument is " + arg;
      throw std::invalid_argument(s);
    }
    std::string parameter = arg.substr(0, n);
    double value = std::stod(arg.substr(n+1));
    if (parameter == "beta") {
      beta = value;
    } else if (parameter == "contacts") {
      contacts_per_iteration = (int) value;
      if (contacts_per_iteration > 0) {
	expose_method = ExposeMethod::RANDOM_CONTACTS;
      }
    } else if (parameter == "isolation") {
      isolation_prob = value;
    } else {
      std::string s = "Unknown parameter: " + parameter;
      throw std::invalid_argument(s);
    }
  }

  void setTransition(const std::string &arg) {
    size_t n = arg.find_first_of(":", 1);
    std::string parameter = arg.substr(0, n);
    if (parameter.size() != 2) {
      std::string s = "Transition argument must be exactly two characters. "
	"Argument is: " + parameter;
      throw std::invalid_argument(s);
    }
    char from = parameter[0];
    char to = parameter[1];

    int from_index = -1;
    int to_index = -1;

    for (int i = 0, j = 0; i < (int) (sizeof(STATES) / sizeof(StateEntry));
	 i++, j++) {
      if (from == STATES[i].abbr) {
	from_index = i;
      }
      if (to == STATES[j].abbr) {
	to_index = j;
      }
    }

    if (from_index < 0 || to_index < 0) {
      std::string s = "Transition argument must have valid state. "
	"Unknown state: " + parameter;
      throw std::invalid_argument(s);
    }

    double value = std::stod(arg.substr(n+1));
    risks[from_index * FINAL + to_index] = value;
  }

};

struct Simulation {
  int sim_num = 0;
  int iteration = 0;
  Parameters parameters;
  std::vector<int> state_counts;
  // totals
  int total_infections = 0;
  int total_vaccinations = 0;
  AgentVector agents;
  void simulate() {
    iteration =  parameters.begin_iteration;
    // Before events
    before_events();
    ++iteration;
    // During events
    for (;iteration <= parameters.end_iteration; ++iteration) {
      during_events();
    }
    // After events
    after_events();
  };

  void before_events() {
    eventCreateAgents(parameters.num_initial_agents);
    eventShuffleAgents();
    eventInitAgentStates(parameters.proportions);
    eventShuffleAgents();
    eventTallyStates();
    eventReportTallies();
    eventReportTotals();
  }

  void during_events() {
    eventTallyStates();
    eventSusceptibleExposed();
    eventTransitions(parameters.risks);
    eventIsolate();
    eventDeisolate();
    eventTallyStates();
    eventReportTallies();
    eventReportTotals();
  }

  void after_events()  {
    //eventTallyStates();
    //eventReportTallies();
  }

  void eventCreateAgents(int n) {
    std::gamma_distribution<double> d1(2.0, 1.0);
    std::gamma_distribution<double> d2(2.0, 1.0);
    agents.resize(n);
    int i = 0;
    for (auto &agent: agents) {
      agent.id = i;
      if (parameters.contacts_per_iteration > 0) {
	double x, y;
	x = d1(rng);
	y = d2(rng);
	agent.infectiousness = x / (x + y);
	x = d1(rng);
	y = d2(rng);
	agent.infectable = x / (x + y);
      }
      i++;
    }
  }

  void eventTallyStates() {
    state_counts.clear();
    state_counts.resize(FINAL);
    for (auto &agent : agents) {
      ++state_counts[agent.state];
    }
  }

  void eventShuffleAgents() {
    std::shuffle(agents.begin(), agents.end(), rng);
  }

  void eventSortAgents() {
    std::sort(agents.begin(), agents.end(),
	      [](const Agent& a, const Agent& b) {
		return a.id < b.id;
	      });
  }
  void eventInitAgentStates(const std::vector<std::pair<State, double>>& proportions) {
    size_t i = 0;
    for (auto &proportion: proportions) {
      size_t n = proportion.second * agents.size() + i;
      for (; i < n; i++) {
	agents[i].setState(proportion.first, iteration,
			   parameters.allow_change_on_same_iteration);
      }
    }
  }

  void eventTransitions(std::vector<double>& risks) {
    assert(risks.size() == FINAL * FINAL);
    for (auto& agent: agents) {
      for (int i = 0; i < FINAL; i++) {
	double risk = risks[agent.state * FINAL + i];
	if (risk > 0.0) {
	  std::uniform_real_distribution<double> dist(0.0, 1.0);
	  if (dist(rng) < risk) {
	    agent.setState( (State) i, iteration,
			    parameters.allow_change_on_same_iteration);
	    if (i == State::VACCINATED) ++total_vaccinations;
	    break;
	  }
	}
      }
    }
  }

  int count_infections() {
    return
      state_counts[INFECTIOUS_ASYMPTOMATIC] +
      state_counts[INFECTIOUS_SYMPTOMATIC] +
      state_counts[INFECTIOUS_HOSPITALIZED] +
      state_counts[INFECTIOUS_ICU];
  }

  void eventSusceptibleExposed() {
    switch(parameters.expose_method) {
    case ExposeMethod::HOMOGENOUS:
      eventSusceptibleExposedHomogenous();
      break;
    case ExposeMethod::RANDOM_CONTACTS:
      eventSusceptibleExposedRandomContacts();
      break;
    default:
      std::cerr << "Unknown expose contact method\n";
    }
  }

  void eventSusceptibleExposedHomogenous() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    int infections = count_infections();
    double gamma = parameters.beta * infections;
    for (auto &agent: agents) {
      if (agent.state == SUSCEPTIBLE) {
	if (dist(rng) < gamma) {
	  ++total_infections;
	  agent.setState(State::EXPOSED, iteration,
			 parameters.allow_change_on_same_iteration);
	}
      }
    }
  }

  void eventSusceptibleExposedRandomContacts() {
    std::vector<size_t> indices(parameters.contacts_per_iteration);
    std::uniform_int_distribution<size_t> dist(0, agents.size() - 1);
    for (auto &agent: agents) {
      if (agent.state >= INFECTIOUS_ASYMPTOMATIC &&
	  agent.state <= INFECTIOUS_ICU && agent.isolated == false) {
	for (int i = 0; i < parameters.contacts_per_iteration; i++) {
	  indices[i] = dist(rng);
	}
	for (auto index: indices) {
	  if (agents[index].state == SUSCEPTIBLE) {
	    agents[index].infect(agent, iteration);
	    if (agents[index].state == EXPOSED) {
	      ++total_infections;
	    }
	  }
	}
      }
    }
  }

  void eventIsolate() {
    if (parameters.isolation_prob > 0.0) {
      for (auto &agent: agents) {
	if (agent.state == State::INFECTIOUS_SYMPTOMATIC &&
	    agent.iteration_changed == iteration) {
	  agent.isolated = true;
	}
      }
    }
  };

  void eventDeisolate() {
    if (parameters.isolation_prob > 0.0) {
      std::uniform_real_distribution<double> dist(0.0, 1.0);
      for (auto &agent: agents) {
	if (agent.isolated == true) {
	  if (dist(rng) < parameters.isolation_prob) {
	    agent.isolated = false;
	  }
	}
      }
    }
  };

  void eventReportTallies() {
    std::stringstream ss;
    ss << "A," << parameters.name << "," << sim_num << "," << iteration;
    for (int i = 0; i < FINAL; i++) {
      ss << "," << state_counts[i];
    }
    ss << "\n";
    std::cout << ss.str();
  }
  void eventReportTotals() {
    std::stringstream ss;
    ss << "B," << parameters.name << "," << sim_num << "," << iteration;
    ss << "," << total_infections << "," << total_vaccinations << "\n";
    std::cout << ss.str();
  }
};

struct SimulationGroup {
  std::vector<Simulation> simulations;
  void createSimulations(Simulation &simulation) {
    for (int i = 0; i < simulation.parameters.num_simulations; i++) {
      simulations.push_back(simulation);
    }
  }

  void simulate() {
    group_before_events();
    int n = std::thread::hardware_concurrency();
    boost::asio::thread_pool pool(n);
    for (size_t i = 0; i < simulations.size(); i++) {
      simulations[i].sim_num = (int) i;
      boost::asio::post(pool, [this, i]() { simulations[i].simulate(); });
    }
    pool.join();
  }

  void eventReportTalliesHeader() {
    std::cout << "A,Desc,Sim,Iter,S,E,I_A,I_S,I_H,I_I,V,R,D\n";
  }

  void eventReportTotalsHeader() {
    std::cout << "B,Desc,Sim,Iter,Infections,Vaccinations\n";
  }

  void group_before_events() {
    eventReportTalliesHeader();
    eventReportTotalsHeader();
  }

};
