#include "doctest/doctest.h"
#include "sharedstate.hh"
#include "FlightsErrorCode.h"

// Tests that don't naturally fit in the headers/.cpp files directly
// can be placed in a tests/*.cpp file. Integration tests are a good example.

TEST_CASE("returnmerge")
{
  std::string original = "mensajeaverificar";
  std::string merged = SharedState::mergestate(original);
  std::cout << merged << original << "--------------------------------------------------------------"<< std::endl;
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

void verificarOptional(std::string original)
{
  auto merged = SharedState::optMergeState(original);
  CHECK(original.size() == merged.value().size());
  CHECK(original == merged.value());
}

void verificarExpected(std::string original)
{
  auto merged = SharedState::expMergestate(original);
  CHECK(original.size() == merged.value().size());
  CHECK(original == merged.value());
}

void verificarExpectedWillFail(std::string original)
{
  auto merged = SharedState::expMergestate(original,true);
  CHECK_FALSE(merged);
  CHECK(merged.error() == FlightsErrorCode::NonexistentLocations);
  CHECK(merged.error().message() == make_error_code(FlightsErrorCode::NonexistentLocations).message());
}

TEST_CASE("Opt merge")
{
  std::string original = "mensajeaverificar";
  verificarOptional(original);
}

TEST_CASE("Parametrized merge test") {
    std::vector<std::string> data {"","mensajeaverificar", "asdasdaasd < saddsdfsdf","546654654654","546654654654546654654654546654654654546654654654" };

    for(auto& i : data) {
        CAPTURE(i); // log the current input data
        verificarOptional(i);
        verificarExpected(i);
        verificarExpectedWillFail(i);
    }
}