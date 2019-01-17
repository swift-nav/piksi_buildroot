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

#include <libpiksi/logging.h>

#include "endpoint_router.h"
#include "endpoint_router_load.h"
#include "endpoint_router_print.h"

#define PROGRAM_NAME "router"

static char *test_data_dir;

class EndpointRouterTests : public ::testing::Test {
};

static bool match_fn_accept = false;

#define MAX_SEND_RECORDS 1024

static size_t send_record[MAX_SEND_RECORDS];
static size_t send_record_index = 0;

static size_t ept_ptr = 0;

static int dummy_pk_endpoint_send(pk_endpoint_t *endpoint, const u8 *buf, size_t len)
{
  (void)buf;
  (void)len;

  send_record[send_record_index++] = (size_t)endpoint;

  return 0;
}

static void dummy_pk_endpoint_destroy(pk_endpoint_t **endpoint)
{
  (void)endpoint;
}

static void reset_dummy_state()
{
  send_record_index = 0;
  memset(send_record, 0, sizeof(send_record));

  ept_ptr = 0;
}

static int router_create_endpoints(router_cfg_t *router, pk_loop_t *loop)
{
  (void)loop;
  port_t *port;

  for (port = router->ports_list; port != NULL; port = port->next) {

    port->pub_ept = (pk_endpoint_t *)ept_ptr++;
    port->sub_ept = (pk_endpoint_t *)ept_ptr++;
  }

  return 0;
}

TEST_F(EndpointRouterTests, BasicTest)
{
  char path[PATH_MAX];
  sprintf(path, "%s/sbp_router.yml", test_data_dir);

  router_cfg_t *r = router_cfg_load(path);
  EXPECT_NE(r, nullptr);

  forwarding_rule_t *rules = r->ports_list[0].forwarding_rules_list;
  const u8 data[] = {0x1, 0x2, 0x3};

  auto match_fn = [](const forwarding_rule_t *r, const filter_t *f, const u8 *d, size_t l, void *) {
    if (f->action == FILTER_ACTION_ACCEPT) {
      match_fn_accept = true;
    }
  };

  process_forwarding_rules(rules, data, 3, match_fn, NULL);
  EXPECT_TRUE(match_fn_accept);

  EXPECT_STREQ(r->ports_list->metric, "sbp/firmware");

  EXPECT_NE(r->ports_list->next, nullptr);
  EXPECT_STREQ(r->ports_list->next->metric, "sbp/settings");

  router_cfg_teardown(&r);
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

  router_cfg_t *r = router_cfg_load(path);
  EXPECT_NE(r, nullptr);

  forwarding_rule_t *rules = r->ports_list[0].forwarding_rules_list;

  auto match_fn = [](const forwarding_rule_t *r, const filter_t *f, const u8 *d, size_t l, void *) {
    if (f->action == FILTER_ACTION_ACCEPT) {
      strcpy(accept_ports[accept_count++], r->dst_port_name);
    } else if (f->action == FILTER_ACTION_REJECT) {
      strcpy(reject_ports[reject_count++], r->dst_port_name);
    } else {
      EXPECT_TRUE(false);
    }
  };

  const u8 settings_register_data[] = {0x55, 0xAE, 0x00};
  process_forwarding_rules(rules, settings_register_data, 3, match_fn, NULL);

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
  process_forwarding_rules(rules, settings_read_resp_data, 3, match_fn, NULL);

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
  process_forwarding_rules(rules, settings_write_resp_data, 3, match_fn, NULL);

  EXPECT_EQ(accept_count, 4);
  EXPECT_EQ(reject_count, 3);

  EXPECT_STREQ(accept_ports[0], "SBP_PORT_SETTINGS_DAEMON");
  EXPECT_STREQ(accept_ports[1], "SBP_PORT_SETTINGS_CLIENT");
  EXPECT_STREQ(accept_ports[2], "SBP_PORT_EXTERNAL");
  EXPECT_STREQ(accept_ports[3], "SBP_PORT_INTERNAL");

  EXPECT_STREQ(reject_ports[0], "SBP_PORT_FILEIO_FIRMWARE");
  EXPECT_STREQ(reject_ports[1], "SBP_PORT_SKYLARK");
  EXPECT_STREQ(reject_ports[2], "SBP_PORT_NAV_DAEMON");

  router_cfg_teardown(&r);
}

TEST_F(EndpointRouterTests, RouterCreate)
{

  char path[PATH_MAX];
  sprintf(path, "%s/sbp_router.yml", test_data_dir);

  router_t *r = router_create(path, NULL, router_create_endpoints);
  EXPECT_NE(r, nullptr);

  router_teardown(&r);

  sprintf(path, "%s/sbp_router_full2.yml", test_data_dir);

  reset_dummy_state();
  r = router_create(path, NULL, router_create_endpoints);

  EXPECT_NE(r, nullptr);
  EXPECT_NE(r->port_rule_cache, nullptr);

  EXPECT_EQ(r->port_rule_cache[0].rule_prefixes->count, 7);

  const u8 settings_write_resp_data[] = {0x55, 0xAF, 0x00};
  router_reader(settings_write_resp_data, 3, &r->port_rule_cache[0]);

  EXPECT_EQ(send_record_index, 2);

  EXPECT_EQ(send_record[0], 2);
  EXPECT_EQ(send_record[1], 6);

  router_teardown(&r);
}

TEST_F(EndpointRouterTests, BrokenRules)
{

  char path[PATH_MAX];
  sprintf(path, "%s/sbp_router_broken.yml", test_data_dir);

  router_t *r = router_create(path, NULL, router_create_endpoints);
  EXPECT_EQ(r, nullptr);
}

TEST_F(EndpointRouterTests, DedupeRulePrefixes)
{

  char path[PATH_MAX];
  sprintf(path, "%s/sbp_router_dupe.yml", test_data_dir);

  router_cfg_t *r = router_cfg_load(path);
  EXPECT_NE(r, nullptr);

  rule_cache_t rule_cache;

  rule_cache.rule_count = 2;
  rule_cache.accept_ports =
    (pk_endpoint_t **)calloc(rule_cache.rule_count, sizeof(pk_endpoint_t *));

  port_t *port = &r->ports_list[0];
  rule_prefixes_t *rule_prefixes = extract_rule_prefixes(NULL, port, &rule_cache);

  EXPECT_NE(rule_prefixes, nullptr);
  EXPECT_EQ(rule_prefixes->count, 5);

  u8 prefix1[] = {0x01, 0x02, 0x03};
  EXPECT_EQ(memcmp(rule_prefixes->prefixes[0], prefix1, rule_prefixes->prefix_len), 0);

  u8 prefix2[] = {0x01, 0x02, 0x04};
  EXPECT_EQ(memcmp(rule_prefixes->prefixes[1], prefix2, rule_prefixes->prefix_len), 0);

  u8 prefix3[] = {0x01, 0x02, 0x05};
  EXPECT_EQ(memcmp(rule_prefixes->prefixes[2], prefix3, rule_prefixes->prefix_len), 0);

  u8 prefix4[] = {0x02, 0x02, 0x03};
  EXPECT_EQ(memcmp(rule_prefixes->prefixes[3], prefix4, rule_prefixes->prefix_len), 0);

  u8 prefix5[] = {0x02, 0x02, 0x05};
  EXPECT_EQ(memcmp(rule_prefixes->prefixes[4], prefix5, rule_prefixes->prefix_len), 0);

  rule_prefixes_destroy(&rule_prefixes);
  router_cfg_teardown(&r);

  free(rule_cache.accept_ports);
}

int main(int argc, char **argv)
{
  endpoint_destroy_fn = dummy_pk_endpoint_destroy;
  endpoint_send_fn = dummy_pk_endpoint_send;

  ::testing::InitGoogleTest(&argc, argv);

  logging_init(PROGRAM_NAME);
  logging_log_to_stdout_only(true);

  test_data_dir = getenv("TEST_DATA_DIR");
  assert(test_data_dir != NULL);

  int rc = RUN_ALL_TESTS();

  logging_deinit();
  return rc;
}
