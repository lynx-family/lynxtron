#ifndef LYNXTRON_SHELL_TEST_LYNXTRON_TEST_SUITE_H_
#define LYNXTRON_SHELL_TEST_LYNXTRON_TEST_SUITE_H_

#include "base/test/test_suite.h"

namespace lynxtron {

class LynxtronTestSuite : public base::TestSuite {
 public:
  LynxtronTestSuite(int argc, char** argv);
  ~LynxtronTestSuite() override;

 protected:
  void Initialize() override;
  void Shutdown() override;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_TEST_LYNXTRON_TEST_SUITE_H_
