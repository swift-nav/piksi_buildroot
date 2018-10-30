/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Dev <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/limits.h>

#include <gtest/gtest.h>

#include "endpoint_router.h"
#include "endpoint_router_load.h"
#include "endpoint_router_print.h"

static char *test_data_dir;

class EndpointRouterTests : public ::testing::Test {
};

static bool match_fn_accept = false;

TEST_F(EndpointRouterTests, BasicTest)
{

  char path[PATH_MAX];
  sprintf(path, "%s/sbp_router.yml", test_data_dir);

  router_t *r = router_load(path);
  EXPECT_NE(r, nullptr);

  forwarding_rule_t *rules = r->ports_list[0].forwarding_rules_list;
  const u8 data[] = {0x1, 0x2, 0x3};

  auto match_fn = [](forwarding_rule_t *r, filter_t *f, const u8 *d, size_t l) {
    if (f->action == FILTER_ACTION_ACCEPT) {
      match_fn_accept = true;
    }
  };

  rule_process(rules, data, 3, match_fn);
  EXPECT_TRUE(match_fn_accept);

  router_teardown(&r);
}

int main(int argc, char **argv)
{

  ::testing::InitGoogleTest(&argc, argv);

  test_data_dir = getenv("TEST_DATA_DIR");
  assert(test_data_dir != NULL);

  return RUN_ALL_TESTS();
}
