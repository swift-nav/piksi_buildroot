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

  auto match_fn = [](const forwarding_rule_t *r, const filter_t *f, const u8 *d, const size_t l) {
    if (f->action == FILTER_ACTION_ACCEPT) {
      match_fn_accept = true;
    }
  };

  process_forwarding_rule(rules, data, 3, match_fn);
  EXPECT_TRUE(match_fn_accept);

  router_teardown(&r);
}

char accept_ports[32][32] = {0};
char reject_ports[32][32] = {0};

int accept_count = 0;
int reject_count = 0;

void reset_accept_reject_state()
{
  accept_count = 0;
  reject_count = 0;

  memset(accept_ports, 0, sizeof(accept_ports));
  memset(reject_ports, 0, sizeof(reject_ports));
}

TEST_F(EndpointRouterTests, FullTest)
{

  char path[PATH_MAX];
  sprintf(path, "%s/sbp_router_full.yml", test_data_dir);

  router_t *r = router_load(path);
  EXPECT_NE(r, nullptr);

  forwarding_rule_t *rules = r->ports_list[0].forwarding_rules_list;

  auto match_fn = [](const forwarding_rule_t *r, const filter_t *f, const u8 *d, const size_t l) {
    if (f->action == FILTER_ACTION_ACCEPT) {
      strcpy(accept_ports[accept_count++], r->dst_port_name);
    } else if (f->action == FILTER_ACTION_REJECT) {
      strcpy(reject_ports[reject_count++], r->dst_port_name);
    } else {
      EXPECT_TRUE(false);
    }
  };

  const u8 settings_register_data[] = {0x55, 0xAE, 0x00};
  process_forwarding_rules(rules, settings_register_data, 3, match_fn);

  EXPECT_EQ(accept_count, 2);
  EXPECT_EQ(reject_count, 5);

  EXPECT_STREQ(accept_ports[0], "SBP_PORT_SETTINGS_DAEMON");
  EXPECT_STREQ(accept_ports[1], "SBP_PORT_INTERNAL");

  EXPECT_STREQ(reject_ports[0], "SBP_PORT_SETTINGS_CLIENT");
  EXPECT_STREQ(reject_ports[1], "SBP_PORT_EXTERNAL");
  EXPECT_STREQ(reject_ports[2], "SBP_PORT_FILEIO_FIRMWARE");
  EXPECT_STREQ(reject_ports[3], "SBP_PORT_SKYLARK");
  EXPECT_STREQ(reject_ports[4], "SBP_PORT_NAV_DAEMON");

  reset_accept_reject_state();
  const u8 settings_read_resp_data[] = {0x55, 0xA5, 0x00};
  process_forwarding_rules(rules, settings_read_resp_data, 3, match_fn);

  EXPECT_EQ(accept_count, 3);
  EXPECT_EQ(reject_count, 4);

  EXPECT_STREQ(accept_ports[0], "SBP_PORT_SETTINGS_DAEMON");
  EXPECT_STREQ(accept_ports[1], "SBP_PORT_EXTERNAL");
  EXPECT_STREQ(accept_ports[2], "SBP_PORT_INTERNAL");

  EXPECT_STREQ(reject_ports[0], "SBP_PORT_SETTINGS_CLIENT");
  EXPECT_STREQ(reject_ports[1], "SBP_PORT_FILEIO_FIRMWARE");
  EXPECT_STREQ(reject_ports[2], "SBP_PORT_SKYLARK");
  EXPECT_STREQ(reject_ports[3], "SBP_PORT_NAV_DAEMON");

  reset_accept_reject_state();
  const u8 settings_write_resp_data[] = {0x55, 0xAF, 0x00};
  process_forwarding_rules(rules, settings_write_resp_data, 3, match_fn);

  EXPECT_EQ(accept_count, 4);
  EXPECT_EQ(reject_count, 3);

  EXPECT_STREQ(accept_ports[0], "SBP_PORT_SETTINGS_DAEMON");
  EXPECT_STREQ(accept_ports[1], "SBP_PORT_SETTINGS_CLIENT");
  EXPECT_STREQ(accept_ports[2], "SBP_PORT_EXTERNAL");
  EXPECT_STREQ(accept_ports[3], "SBP_PORT_INTERNAL");

  EXPECT_STREQ(reject_ports[0], "SBP_PORT_FILEIO_FIRMWARE");
  EXPECT_STREQ(reject_ports[1], "SBP_PORT_SKYLARK");
  EXPECT_STREQ(reject_ports[2], "SBP_PORT_NAV_DAEMON");

  router_teardown(&r);
}

int main(int argc, char **argv)
{

  ::testing::InitGoogleTest(&argc, argv);

  test_data_dir = getenv("TEST_DATA_DIR");
  assert(test_data_dir != NULL);

  return RUN_ALL_TESTS();
}
