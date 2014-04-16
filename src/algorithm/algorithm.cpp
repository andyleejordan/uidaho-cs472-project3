/* algorithm.cpp - CS 472 Project #3: Genetic Programming
 * Copyright 2014 Andrew Schwartzmeyer
 *
 * Source file for algorithm namespace
 */

#include <algorithm>
#include <cassert>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <future>
#include <thread>

#include "algorithm.hpp"
#include "../individual/individual.hpp"
#include "../options/options.hpp"
#include "../random_generator/random_generator.hpp"

namespace algorithm
{
  using std::vector;
  using individual::Individual;
  using options::Options;
  using namespace random_generator;
  const unsigned int width = 10;

  // Returns true if "a" is closer to 0 than "b" and is also normal.
  bool
  compare_fitness::operator()(const Individual& a, const Individual& b)
  {
    return std::isnormal(a.get_fitness())
      ? (a.get_fitness() > b.get_fitness()) : false;
  }

  // Opens the appropriate log file for given time, trial, and folder.
  void
  open_log(std::ofstream& log, const std::time_t& time, const int& trial,
	   const std::string& folder)
  {
    std::string filename = folder + std::to_string(time) + "_"
      + std::to_string(trial) + ".dat";

    log.open(filename, std::ios_base::app);
    if (not log.is_open())
      {
	std::cerr << "Log file " << filename << " could not be opened!\n";
	std::exit(EXIT_FAILURE);
      }
  }

  /* Log a line of relevant algorithm information (best and average
     fitness and size plus adjusted best fitness). */
  void
  log_info(const unsigned int verbosity, const std::string& logs_dir,
	   const std::time_t& time, const int& trial, const int& iteration,
	   const Individual& best, const vector<Individual>& population)
  {
    // Don't log if verbosity is zero, but still allow calls to this function.
    if (verbosity == 0)
      return;

    float total_fitness =
      std::accumulate(population.begin(), population.end(), 0.,
		      [](const float& a, const Individual& b)->float const
		      {
			return a + b.get_adjusted();
		      });

    unsigned int total_size =
      std::accumulate(population.begin(), population.end(), 0,
		      [](const unsigned int& a, const Individual& b)->unsigned int const
		      {
			return a + b.get_total();
		      });

    unsigned int total_depth =
      std::accumulate(population.begin(), population.end(), 0,
		      [](const unsigned int& a, const Individual& b)->unsigned int const
		      {
			return a + b.get_depth();
		      });

    std::ofstream log;
    using std::setw;
    open_log(log, time, trial, logs_dir);
    log << std::setprecision(4) << std::left
	<< setw(width) << iteration
	<< setw(width) << best.get_score()
	<< setw(width) << best.get_adjusted()
	<< setw(width) << total_fitness / population.size()
	<< setw(width) << best.get_total()
	<< setw(width) << static_cast<float>(total_size) / population.size()
	<< setw(width) << best.get_depth()
	<< setw(width) << static_cast<float>(total_depth) / population.size()
	<< std::endl;
    log.close();
  }

  /* Create an initial population using "ramped half-and-half" (half
     full trees, half randomly grown trees, all to random depths
     between 0 and maximum depth). */
  vector<Individual>
  new_population(const Options& options)
  {
    vector<Individual> population;
    int_dist depth_dist{0, static_cast<int>(options.max_depth)}; // ramped
    for (unsigned int i = 0; i < options.population_size; ++i)
      {
	/* The depth is ramped, and so drawn randomly for each
	   individual. */
	unsigned int depth = depth_dist(rg.engine);
	population.emplace_back(Individual{
	    depth, options.grow_chance, options.map});
      }
    return population;
  }

  /* Return best candidate from size number of contestants randomly
     drawn from population. */
  Individual
  selection(const unsigned int& size, const vector<Individual>& population)
  {
    int_dist dist{0, static_cast<int>(population.size()) - 1}; // closed interval
    vector<Individual> contestants;

    for (unsigned int i = 0; i < size; ++i)
      contestants.emplace_back(population[dist(rg.engine)]);

    return *std::min_element(contestants.begin(), contestants.end(),
			     compare_fitness());
  }

  /* Return new offspring population reassembled from parallel calls to
     get_children () delegate. */
  vector<Individual>
  new_offspring(const Options& options, const vector<Individual>& population)
  {
    vector<Individual> offspring;
    offspring.reserve(population.size());
    // Select parents for children.
    while (offspring.size() != population.size())
      {
	offspring.emplace_back(selection(options.tournament_size, population));
      }
    // Binary crossover with probability.
    if (options.crossover_size == 2)
      {
	real_dist dist{0, 1};
	for (unsigned int i = 0; i < offspring.size(); i += 2)
	  {
	    if (dist(rg.engine) < options.crossover_chance)
	      {
		crossover(options.internals_chance,
			  offspring[i], offspring[i + 1]);
	      }
	  }
      }
    // Mutate and evaluate children.
    for (Individual& child : offspring)
      {
	real_dist dist{0, 1};
	// Mutate children conditionally
	if (dist(rg.engine) < options.mutate_chance)
	  child.mutate();
	// Evaluate all children
	child.evaluate(options.map, options.penalty); // Evaluate children
      }
    assert(offspring.size() == population.size());
    return offspring;
  }

  /* The actual genetic algorithm applied which (hopefully) produces a
     well-fit expression for a given dataset. */
  const individual::Individual
  genetic(const Options& options, const std::time_t time, const int& trial)
  {
    // Start logging
    std::ofstream log;
    if (options.verbosity > 0)
      {
	using std::setw;
	open_log(log, time, trial, options.logs_dir);
	log << "# running a Genetic Program @ "
	    << std::ctime(&time)
	    << "# initial depth: " << options.max_depth
	    << ", iterations: " << options.iterations
	    << ", population size: " << options.population_size
	    << ", tournament size: " << options.tournament_size
	    << ", elitism size: " << options.elitism_size
	    << ", fitness penalty: " << options.penalty << " * total size"
	    << ", crossover chance: " << options.crossover_chance
	    << ", mutate chance: " << options.mutate_chance
	    << ", grow chance: " << options.grow_chance
	    << std::left
	    << setw(width) << "\n# gen"
	    << setw(width) << "score"
	    << setw(width) << "best fit"
	    << setw(width) << "avg fit"
	    << setw(width) << "best size"
	    << setw(width) << "avg size"
	    << setw(width) << "best dep"
	    << setw(width) << "avg dep"
	    << std::endl;
	log.close();
      }
    // Begin timing algorithm.
    std::chrono::time_point<std::chrono::system_clock> start, end;
    start = std::chrono::system_clock::now();

    // Create initial population.
    vector<Individual> population = new_population(options);
    Individual best;

    // Run algorithm to termination.
    for (unsigned int i = 0; i < options.iterations; ++i)
      {
	// Find best Individual of current population.
	best = *std::min_element(population.begin(), population.end(),
				 compare_fitness());
	// Launch background logging thread.
	auto log_thread = std::async(std::launch::async, log_info,
				     options.verbosity, options.logs_dir, time,
				     trial, i, best, population);
	// Create replacement population.
	vector<Individual> offspring = new_offspring(options, population);
	// Perform elitism replacement of random individuals.
	int_dist dist{0, static_cast<int>(options.population_size) - 1};
	for (unsigned int e = 0; e < options.elitism_size; ++e)
	  offspring[dist(rg.engine)] = best;
	// Replace current population with offspring.
	population = offspring;
	// Sync with background logging thread.
	log_thread.wait();
      }
    // End timing algorithm.
    end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    std::time_t end_time = std::chrono::system_clock::to_time_t(end);

    // Log time information.
    if (options.verbosity > 0)
      {
	open_log(log, time, trial, options.logs_dir);
	log << best.print() << best.print_formula()
	    << "# Finished computation @ " << std::ctime(&end_time)
	    << "# Elapsed time: " << elapsed_seconds.count() << "s\n";
	log.close();
      }
    // Log evaluation plot data of best individual.
    std::ofstream plot;
    open_log(plot, time, trial, options.plots_dir);
    plot << best.evaluate(options.map, options.penalty, true);
    plot.close();
    return best;
  }
}
