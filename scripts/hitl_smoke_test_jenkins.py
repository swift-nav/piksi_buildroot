#!/usr/bin/env python2

# Copyright (C) 2017-2018 Swift Navigation Inc.
# Contact: Swift Navigation <dev@swiftnav.com>
#
# This source is subject to the license found in the file 'LICENSE' which must
# be be distributed together with this source. All other rights reserved.
#
# THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
# EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
#
# Script for kicking off HITL smoke tests on piksi_firmware_private Pull
# Requests. Expects HITL_API_GITHUB_TOKEN and GITHUB_COMMENT_TOKEN to be in
# environmental variables.

import os
import sys
import requests

HITL_API_GITHUB_USER = os.environ['HITL_API_GITHUB_USER']
HITL_API_URL = "https://hitl-api.ce.swiftnav.com/"

HITL_API_BUILD_TYPE = "buildroot_pull_request"
HITL_VIEWER_BUILD_TYPE = "buildroot_pr"

# HITL scenarios to kick off, and # of runs for each scenario.
SCENARIOS = (
    ("live-roof-650-townsend", 1),
    ("live-roof-650-townsend-dropouts-zero-baseline", 1),
)

try:
    GITHUB_PULL_REQUEST = os.environ['GITHUB_PULL_REQUEST']
    JENKINS_BUILD_ID = os.environ['BUILD_NUMBER']
    HITL_API_GITHUB_TOKEN = os.environ['HITL_API_GITHUB_TOKEN']
    GITHUB_COMMENT_TOKEN = os.environ['GITHUB_COMMENT_TOKEN']
    JENKINS_BUILD_URL = os.environ['BUILD_URL']
    REPO = os.environ['REPO']
except KeyError as exc:
    sys.stderr.write("Required environment variable not found: {}\n".format(exc))
    sys.exit(1)

os.chdir(os.path.realpath(os.path.dirname(__file__)))
print(">>> CWD: {}".format(os.getcwd()))

from pbr_script_lib import Runner as R
from pbr_script_lib import find_dot_git, build_url

os.chdir('..')

# From https://github.com/travis-ci/travis-ci/issues/8557, it is not trivial to
# get the name / email of the person who made the PR, so we'll use the email of
# the commit instead.
TESTER_EMAIL = R().call("git", "log", "--format='%ae'", "HEAD").pipe().call("head", "-n1").to_str().strip("'")
print(">>> TESTER_EMAIL = {}".format(TESTER_EMAIL))

BUILD_VERSION=os.environ.get('BUILD_VERSION', R().call("git", "describe", "--tags", "--dirty", "--always").to_str())
print(">>> BUILD_VERSION = {}".format(BUILD_VERSION))

COMMENT_URL = "https://api.github.com/repos/swift-nav/{}/issues/{}/comments".format(REPO, GITHUB_PULL_REQUEST)
COMMENT_HEADER = "## HITL smoke tests: {}".format(BUILD_VERSION)


def clear_metrics_yaml():
    with open('metrics.yaml', 'wb'):
        pass


def gen_hitl_error_comment():
    return (
        "{}\nThere was an error using the HITL API to kick off smoke tests for this commit.\nSee {}"
        .format(COMMENT_HEADER, JENKINS_BUILD_URL)
    )


def post_github_comment(comment_body):
    post_data = {
        "body": comment_body
    }
    try:
        response = requests.post(COMMENT_URL, auth=(HITL_API_GITHUB_USER, GITHUB_COMMENT_TOKEN), json=post_data)
        print("Comment-post result: {}".format(response.content))
    except Exception as exc:
        sys.stdout.write("There was an error posting a comment to GitHub.  The error:\n\n")
        sys.stdout.write(str(exc) + "\n")


metrics_template = """
- scenario_minimum: {}
  scenario_name: {}
"""


def write_metrics_yaml(scenario_name, run_count):
    with open('metrics.yaml', 'a') as fp:
        fp.write(metrics_template.format(run_count, scenario_name))


hitl_links_template = """
{COMMENT_HEADER}

These test runs are kicked off whenever you push a new commit to this PR. All passfail metrics in these runs must pass for the `hitl/pass-fail` status to be marked successful.

### job status
{JOB_LIST}

### gnss-analysis results
At least one run must complete for these links to have data.

#### passfail
{PASS_FAIL_LIST}

#### detailed
{DETAILED_LIST}
"""


def gen_job_list(capture_ids):
    ls = []
    for index, capture_id in enumerate(capture_ids):
        url = "https://gnss-analysis.swiftnav.com/jobs/id={}".format(capture_id)
        ls.append("+ [{SCENARIO_NAME} runs]({URL})".format(SCENARIO_NAME=SCENARIOS[index][0], URL=url))
    return str.join("\n", ls)


def build_gnss_analysis_url(scenario_name, metrics_preset):
    base_url = "https://gnss-analysis.swiftnav.com/"
    params = {
        "summary_type" : "q50",
        "metrics_preset" : metrics_preset,
        "scenario" : scenario_name,
        "build_type" : HITL_VIEWER_BUILD_TYPE,
        "firmware_versions": BUILD_VERSION,
        "groupby_key": "firmware",
        "display_type": "table"
    }
    import urllib
    return base_url + urllib.urlencode(params)


def gen_pass_fail_list():
    ls = []
    for scenario_name, _ in SCENARIOS:
        url = build_gnss_analysis_url(scenario_name, "pass_fail")
        ls.append("+ [{SCENARIO_NAME}]({URL})".format(SCENARIO_NAME=scenario_name, URL=url))
    return str.join("\n", ls)


def gen_detailed_list():
    ls = []
    for scenario_name, _ in SCENARIOS:
        url = build_gnss_analysis_url(scenario_name, "detailed")
        ls.append("+ [{SCENARIO_NAME}]({URL})".format(SCENARIO_NAME=scenario_name, URL=url))
    return str.join("\n", ls)


def gen_hitl_comment(job_list, pass_fail_list, detailed_list):
    return hitl_links_template.format(COMMENT_HEADER=COMMENT_HEADER, JOB_LIST=job_list, PASS_FAIL_LIST=pass_fail_list, DETAILED_LIST=detailed_list)


def main():

    # Capture IDs of kicked off jobs.
    capture_ids = []

    clear_metrics_yaml()

    for scenario, runs in SCENARIOS:

        query_args = {
            'build_type'    : HITL_API_BUILD_TYPE,
            'build'         : BUILD_VERSION,
            'tester_email'  : TESTER_EMAIL,
            'runs'          : runs,
            'scenario_name' : scenario,
            'priority'      : 1,
        }

        hitl_api_url = build_url(HITL_API_URL, "jobs", query_args)
        print(">>> Posting to HITL API URL: {}".format(hitl_api_url))

        try:
            resp = requests.post(hitl_api_url,
                                 auth=(HITL_API_GITHUB_USER, HITL_API_GITHUB_TOKEN))
            result = resp.json()
            capture_ids.append(result['id'])
        except Exception as exc:
            sys.stdout.write("There was an error using the HITL API. Posted comment to GitHub PR. The error:\n\n")
            sys.stdout.write(str(exc) + "\n")
            post_github_comment(gen_hitl_error_comment())
            sys.exit(1)

        write_metrics_yaml(scenario, runs)
        # metrics.yaml should be published by Jenkins pipeline

    pr_comment = gen_hitl_comment(gen_job_list(capture_ids), gen_pass_fail_list(), gen_detailed_list())

    print("PR comment:\n{}".format(pr_comment))
    post_github_comment(pr_comment)


if __name__ == '__main__':
    main()


# vim: ft=python:et:sts=4:ts=4:sw=4:tw=120:
