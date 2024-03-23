#define BOOST_TEST_MODULE fit_tests
#include <boost/process.hpp>
#include <boost/test/included/unit_test.hpp>

#include "abm.hpp"

namespace bp = boost::process;

BOOST_AUTO_TEST_CASE(simulation_setup) {
  Simulation s;
  s.before_events();
  BOOST_TEST(s.agents.size() == 100);
  s.eventTallyStates();
  BOOST_TEST(s.state_counts[State::SUSCEPTIBLE] == 99);
  BOOST_TEST(s.state_counts[State::EXPOSED] == 1);
  // Check that shuffled. Theoretically could fail. Extremely unlikely.
  bool shuffled =
    s.agents[0].id != 0 &&
    s.agents[1].id != 1 &&
    s.agents[2].id != 2 &&
    s.agents[3].id != 3 &&
    s.agents[4].id != 4 &&
    s.agents[5].id != 5 &&
    s.agents[6].id != 6;
  BOOST_TEST(shuffled == true);
  s.eventSortAgents();
  bool sorted =
    s.agents[0].id == 0 &&
    s.agents[1].id == 1 &&
    s.agents[2].id == 2 &&
    s.agents[3].id == 3 &&
    s.agents[4].id == 4;
  BOOST_TEST(sorted == true);
}

BOOST_AUTO_TEST_CASE(simulation_transitions) {
  Simulation s;
  for (int i = 0; i < FINAL; i++) {
    s.parameters.risks[i] = 0.0;
  }
  s.parameters.risks[IND(EXPOSED, INFECTIOUS_ASYMPTOMATIC)] =
    0.0000000001;
  s.parameters.risks[IND(EXPOSED, DEAD)] = 1.0;
  s.eventCreateAgents(s.parameters.num_initial_agents);
  s.eventShuffleAgents();
  s.eventInitAgentStates(s.parameters.proportions);
  s.eventShuffleAgents();
  s.eventTallyStates();
  BOOST_TEST(s.state_counts[State::EXPOSED] == 1);
  s.eventTransitions(s.parameters.risks);
  s.eventTallyStates();
  BOOST_TEST(s.state_counts[State::EXPOSED] == 0);
  BOOST_TEST(s.state_counts[State::DEAD] == 1);
}

BOOST_AUTO_TEST_CASE(simulation_susceptible_exposed_a) {
  Simulation s;
  s.parameters.beta = 0.01;
  s.parameters.proportions = {
    {State::SUSCEPTIBLE, 0.9},
    {State::INFECTIOUS_ASYMPTOMATIC, 0.1}
  };
  s.before_events();
  s.eventTallyStates();
  BOOST_TEST(s.state_counts[State::INFECTIOUS_ASYMPTOMATIC] == 10);
  BOOST_TEST(s.state_counts[State::EXPOSED] == 0);
  s.eventSusceptibleExposedHomogenous();
  s.eventTallyStates();
  BOOST_TEST(s.state_counts[State::INFECTIOUS_ASYMPTOMATIC] == 10);
  BOOST_TEST(s.state_counts[State::EXPOSED] > 5);
  BOOST_TEST(s.state_counts[State::EXPOSED] < 30);
}

BOOST_AUTO_TEST_CASE(simulation_susceptible_exposed_b) {
  Simulation s;
  s.parameters.proportions = {
    {State::SUSCEPTIBLE, 0.9},
    {State::INFECTIOUS_ASYMPTOMATIC, 0.1}
  };
  s.before_events();
  s.eventTallyStates();
  BOOST_TEST(s.state_counts[State::INFECTIOUS_ASYMPTOMATIC] == 10);
  BOOST_TEST(s.state_counts[State::EXPOSED] == 0);
  for (int i = 0; i < 100; i++) {
    s.eventSusceptibleExposedHomogenous();
  }
  s.eventTallyStates();
  BOOST_TEST(s.state_counts[State::INFECTIOUS_ASYMPTOMATIC] == 10);
  BOOST_TEST(s.state_counts[State::EXPOSED] > 5);
  BOOST_TEST(s.state_counts[State::EXPOSED] < 90);
}

BOOST_AUTO_TEST_CASE(simulation_set_parameters) {
  Parameters parameters;

  parameters.setParameters("beta:0.5");
  BOOST_TEST(parameters.beta == 0.5);

  parameters.setParameters("contacts:20");
  BOOST_TEST(parameters.contacts_per_iteration == 20);

  parameters.setParameters("isolation:0.1");
  BOOST_TEST(parameters.isolation_prob == 0.1);
}

BOOST_AUTO_TEST_CASE(simulation_set_transition) {
  Parameters parameters;

  parameters.setTransition("SD:0.5");
  BOOST_TEST(parameters.risks[IND(State::SUSCEPTIBLE, State::DEAD)] == 0.5);

  parameters.setTransition("YV:0.2");
  BOOST_TEST(parameters.risks[IND(State::INFECTIOUS_SYMPTOMATIC,
				  State::VACCINATED)] == 0.2);
}
