/* indivudal.cpp - CS 472 Project #2: Genetic Programming
 * Copyright 2014 Andrew Schwartzmeyer
 *
 * Source file for Individual
 */

#include <cmath>
#include <iostream>
#include <memory>
#include <tuple>
#include <vector>

#include "individual.hpp"
#include "../problem/problem.hpp"
#include "../random/random_generator.hpp"

namespace individual {
  const int terminals = 2;
  const int unaries = 4;
  const int binaries = 4;
  const int quadnaries = 1;
}

using namespace individual;
using namespace random_generator;
using problem::Problem;

Node::Node(const Problem & problem, const int & depth) {
  if (depth < problem.max_depth) {
    // assign random internal type
    int_dist dist(terminals + unaries, terminals + unaries + binaries + quadnaries - 1); // closed interval
    type = Type(dist(engine));
    int arity = 0;
    if (type - terminals < unaries) arity = 1; // for unary operators
    else if (type - terminals < unaries + binaries) arity = 2; // for binary operators
    else if (type - terminals < unaries + binaries + quadnaries) arity = 4; // if-else conditional
    // recursively create subtrees
    for (int i = 0; i < arity; i++)
      children.emplace_back(Node(problem, depth + 1));
  } else {
    // reached max depth, assign random terminal type
    int_dist dist(0, terminals - 1); // closed interval
    type = Type(dist(engine));
    // setup constant type; input is provided on evaluation
    if (type == constant) {
      // choose a random value between the problem's min and max
      real_dist dist(problem.constant_min, problem.constant_max);
      k = dist(engine);
    }
  }
}

double Node::evaluate(const double & x) const {
  double a, b, c, d;
  if (type > terminals) // evaulate unary
    a = children[0].evaluate(x);
  if (type > terminals + unaries) // evaluate binary
    b = children[1].evaluate(x);
  if (type > terminals + unaries + binaries) {
    c = children[2].evaluate(x);
    d = children[3].evaluate(x);
  }
  switch(type) {
  case constant:
    return k;
  case input:
    return x;
  case sqrt:
    return std::sqrt(std::abs(a)); // protected
  case sin:
    return std::sin(a);
  case log:
    return (a == 0) ? 0 : std::log(std::abs(a)); // protected
  case exp:
    return std::exp(a);
  case add:
    return a + b;
  case subtract:
    return a - b;
  case multiply:
    return a * b;
  case divide:
    return (b == 0) ? 1 : a / b; // protected
  case cond:
    return (a < b) ? c : d;
  }
}

Size Node::size() const {
  // Recursively count children via pre-order traversal
  // Keep track of internals, leafs, and total
  Size size;
  for (auto child : children) {
    size.internals += child.size().internals;
    size.leafs += child.size().leafs;
  }
  if (children.size() == 0) ++size.leafs;
  else ++size.internals;
  return size;
}

std::string Node::represent() const {
  switch(type) {
  case constant:
    return std::to_string(k);
  case input:
    return "x";
  case sqrt:
    return " sqrt";
  case sin:
    return " sin";
  case log:
    return " log";
  case exp:
    return " exp";
  case add:
    return " +";
  case subtract:
    return " -";
  case multiply:
    return " *";
  case divide:
    return " /";
  case cond:
    return " a < b ? c : d";
  }
}

void Node::print(const int & depth) const {
  // Post-order traversal print of expression in RPN/posfix notation
  using std::cout;
  cout << '(';
  for (auto child : children)
    child.print(depth + 1);
  cout << represent();
  cout << ')';
}

Individual::Individual(const Problem & p): problem(p), root(Node(problem)) {
  size = root.size();
  fitness = evaluate();
}

double Individual::evaluate() const {
  double fitness = 0;
  for (auto pair : problem.values) {
    double output = root.evaluate(std::get<0>(pair));
    fitness += std::pow(output - std::get<1>(pair), 2);
  }
  return std::sqrt(fitness);
}

void Individual::print_formula() const {
  std::cout << "Expression tree of size " << get_total()
	    << " with " << get_internals() << " internals"
	    << " and " << get_leafs() << " leafs"
	    << " has the following formula: " << std::endl;
  root.print();
  std::cout << std::endl;
}

void Individual::print_calculation() const {
  double fitness = 0;
  for (auto pair : problem.values) {
    double output = root.evaluate(std::get<0>(pair));
    double error = std::pow(output - std::get<1>(pair), 2);
    fitness += error;
    std::cout << "f(" << std::get<0>(pair) << ", " << std::get<1>(pair)
	      << ") = " << output
	      << ", error = " << error << ".\n";
  }
  std::cout << "Total fitness: " << std::sqrt(fitness) << ".\n";
}
