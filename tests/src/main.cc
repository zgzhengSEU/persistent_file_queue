
#include <string>

#include "gtest/gtest.h"
#include "persistent_file_queue/hello.h"

namespace {
  TEST(HelloTest, ComposeMessages) {
    EXPECT_EQ(std::string("hello/1.0: Hello World Release! (with color!)\n"),
              compose_message("Release", "with color!"));
    EXPECT_EQ(std::string("hello/1.0: Hello World Debug! (with color!)\n"),
              compose_message("Debug", "with color!"));
    EXPECT_EQ(std::string("hello/1.0: Hello World Release! (without color)\n"),
              compose_message("Release", "without color"));
    EXPECT_EQ(std::string("hello/1.0: Hello World Debug! (without color)\n"),
              compose_message("Debug", "without color"));
  }

  TEST(PersistentFileQueueTest, Greet) {
    using namespace persistent_file_queue;

    PersistentFileQueue persistent_file_queue("Tests");

    CHECK(persistent_file_queue.greet(LanguageCode::EN) == "Hello, Tests!");
    CHECK(persistent_file_queue.greet(LanguageCode::DE) == "Hallo Tests!");
    CHECK(persistent_file_queue.greet(LanguageCode::ES) == "¡Hola Tests!");
    CHECK(persistent_file_queue.greet(LanguageCode::FR) == "Bonjour Tests!");
  }
}  // namespace