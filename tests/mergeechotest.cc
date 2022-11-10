#include "doctest/doctest.h"
#include "sharedstate.hh"

// Tests that don't naturally fit in the headers/.cpp files directly
// can be placed in a tests/*.cpp file. Integration tests are a good example.

TEST_CASE("returnmerge")
{
  std::string original = "mensajeaverificar";
  std::string merged = SharedState::mergestate(original);
  std::cout << merged << original << "--------------------------------------------------------------";
  CHECK(original.size() == merged.size());
  CHECK(original == merged);
}

TEST_CASE("parametrizedmerge")
{
  std::string original = "mensajeaverificar";
  std::string merged;
  SharedState::mergestate(original, merged);
  std::cout << merged << original;
  CHECK(original.size() == merged.size());
  CHECK(original == merged);
}

